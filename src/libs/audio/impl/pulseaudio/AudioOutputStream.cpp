/*
 * Copyright (C) 2026 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "AudioOutputStream.hpp"

#include <algorithm>

#include <boost/asio/post.hpp>
#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/error.h>
#include <pulse/introspect.h>
#include <pulse/operation.h>
#include <pulse/proplist.h>
#include <pulse/stream.h>
#include <pulse/thread-mainloop.h>

#include "audio/Exception.hpp"
#include "core/ILogger.hpp"
#include "core/LiteralString.hpp"

#include "audio/PcmTypes.hpp"

#include "Exception.hpp"
#include "MainLoopScopedLock.hpp"

namespace lms::audio::pulseaudio
{
    namespace
    {
        core::LiteralString streamStateToString(pa_stream_state_t state)
        {
            switch (state)
            {
            case PA_STREAM_UNCONNECTED: // The stream is not yet connected to any sink or
                return "Unconnected";
            case PA_STREAM_CREATING: // The stream is being created
                return "Creating";
            case PA_STREAM_READY: // The stream is established, you may pass audio data to it now
                return "Ready";
            case PA_STREAM_FAILED: // An error occurred that made the stream invalid
                return "Failed";
            case PA_STREAM_TERMINATED: // The stream has been terminated cleanly
                return "Terminated";
            }
            return "Unknown";
        }

        ::pa_sample_format toPaSampleFormat(PcmSampleType sampleType, std::endian byteOrder)
        {
            switch (sampleType)
            {
            case PcmSampleType::Signed16:
                return byteOrder == std::endian::little ? PA_SAMPLE_S16LE : PA_SAMPLE_S16BE;
            case PcmSampleType::Signed32:
                return byteOrder == std::endian::little ? PA_SAMPLE_S32LE : PA_SAMPLE_S32BE;
            case PcmSampleType::Float32:
                return byteOrder == std::endian::little ? PA_SAMPLE_FLOAT32LE : PA_SAMPLE_FLOAT32BE;
            case PcmSampleType::Float64:
                throw Exception{ "Float64 sample not supported" };
            }

            throw Exception{ "Unexpected sample type!" };
        }
    } // namespace

    void PaPropListDeleter::operator()(pa_proplist* proplist) const noexcept
    {
        ::pa_proplist_free(proplist);
    }

    void PaStreamDeleter::operator()(pa_stream* stream) const noexcept
    {
        ::pa_stream_unref(stream);
    }

    AudioOutputStream::AudioOutputStream(boost::asio::io_context& ioContext, pa_context* context, pa_threaded_mainloop* mainLoop, std::string_view name, const PcmParameters& outputParameters)
        : _ioContext{ ioContext }
        , _context{ context }
        , _mainLoop{ mainLoop }
        , _outputParameters{ outputParameters }
    {
        if (_outputParameters.planar)
            throw Exception{ "Planar output format not supported" };

        ::pa_sample_spec specs;
        specs.channels = _outputParameters.channelCount;
        specs.format = toPaSampleFormat(_outputParameters.sampleType, _outputParameters.byteOrder);
        specs.rate = _outputParameters.sampleRate;

        LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "channels = " << (int)specs.channels << ", format = " << specs.format << ", rate = " << specs.rate);
        PaPropListPtr props{ ::pa_proplist_new() };

        if (::pa_proplist_sets(props.get(), PA_PROP_MEDIA_ROLE, "music") != 0)
            throw Exception{ "pa_proplist_sets failed" };

        _stream = PaStreamPtr{ pa_stream_new_with_proplist(_context, std::string{ name }.c_str(), &specs, nullptr, props.get()) };
        if (!_stream)
            throw PaException{ "pa_stream_new_with_proplist failed", ::pa_context_errno(_context) };

        ::pa_stream_set_state_callback(_stream.get(), [](pa_stream*, void* userdata) { static_cast<AudioOutputStream*>(userdata)->onStateChanged(); }, this);
        ::pa_stream_set_write_callback(_stream.get(), [](pa_stream*, std::size_t nbytes, void* userdata) { static_cast<AudioOutputStream*>(userdata)->onWriteRequested(nbytes); }, this);

        ::pa_stream_set_started_callback(_stream.get(), [](pa_stream*, void*) { LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Stream started!"); }, nullptr);
        ::pa_stream_set_overflow_callback(_stream.get(), [](pa_stream*, void*) { LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Stream overflow!"); }, nullptr);
        ::pa_stream_set_underflow_callback(_stream.get(), [](pa_stream*, void*) { LMS_LOG(AUDIO_OUTPUT_STREAM, WARNING, "Stream underflow!"); }, nullptr);

        connect();
    };

    AudioOutputStream::~AudioOutputStream()
    {
        assert(_pendingWriteOperations.empty());
        assert(_ongoingWriteOperationCount == 0);

        // We don't want to be notified for termination as this holder class will be destroyed
        ::pa_stream_set_state_callback(_stream.get(), NULL, NULL);
    }

    const PcmParameters& AudioOutputStream::getParameters() const
    {
        return _outputParameters;
    }

    void AudioOutputStream::asyncWaitReady(WaitReadyCallback cb)
    {
        MainLoopScopedLock lock{ _mainLoop };

        if (_waitReadyCallback)
            throw Exception{ "asyncWaitReady already called!" };

        if (::pa_stream_get_state(_stream.get()) == PA_STREAM_READY)
        {
            boost::asio::post(_ioContext, std::move(cb));
        }
        else
        {
            _ioContext.get_executor().on_work_started();
            _waitReadyCallback = std::move(cb);
        }
    }

    void AudioOutputStream::asyncWrite(std::span<const std::byte> buffer, WriteCompletionCallback cb)
    {
        if (buffer.size() == 0)
            throw Exception{ "Empty buffer!" };

        MainLoopScopedLock lock{ _mainLoop };

        if (_drainRequested)
            throw Exception{ "asyncDrain already called!" };

        WriteOperation* operation{ acquireWriteOperation() };
        operation->buffer = buffer;
        operation->callback = std::move(cb);

        _ioContext.get_executor().on_work_started();
        _pendingWriteOperations.push_back(operation);
        if (_pendingWriteOperations.size() == 1 && _ongoingWriteOperationCount == 0 && ::pa_stream_get_state(_stream.get()) == PA_STREAM_READY)
        {
            LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Audio buffer shortage? immediate write!");
            writeSome(pa_stream_writable_size(_stream.get()));
        }
    }

    void AudioOutputStream::asyncDrain(DrainCompletionCallback cb)
    {
        LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "asyncDrain called...");

        MainLoopScopedLock lock{ _mainLoop };

        if (_drainRequested)
            throw Exception{ "asyncDrain already called! " };

        _ioContext.get_executor().on_work_started();
        _drainRequested = true;
        _drainCallback = std::move(cb);
        if (_pendingWriteOperations.empty() && ::pa_stream_get_state(_stream.get()) == PA_STREAM_READY)
        {
            LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "audio buffer shortage? immediate drain");
            drain();
        }
    }

    std::chrono::microseconds AudioOutputStream::getPlaybackTime() const
    {
        pa_usec_t duration{};
        if (::pa_stream_get_time(_stream.get(), &duration) == -PA_ERR_NODATA)
            duration = 0;

        return std::chrono::microseconds{ duration };
    }

    std::chrono::microseconds AudioOutputStream::getLatency() const
    {
        pa_usec_t latency{};
        int negative{};
        if (::pa_stream_get_latency(_stream.get(), &latency, &negative) == -PA_ERR_NODATA)
            latency = 0;

        return std::chrono::microseconds{ latency };
    }

    void AudioOutputStream::flush()
    {
        MainLoopScopedLock lock{ _mainLoop };

        assert(_stream);

        {
            pa_operation* op{ ::pa_stream_flush(_stream.get(), nullptr, nullptr) };
            if (!op)
                throw PaException{ "pa_stream_flush failed", pa_context_errno(_context) };

            ::pa_operation_unref(op);
        }

        // post all pending writes
        while (!_pendingWriteOperations.empty())
        {
            WriteOperation* writeOperation{ _pendingWriteOperations.front() };
            _pendingWriteOperations.pop_front();

            onWriteOperationCancelled(writeOperation);
        }
    }

    void AudioOutputStream::pause()
    {
        LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Pausing stream");

        MainLoopScopedLock lock{ _mainLoop };

        pa_operation* op{ ::pa_stream_cork(_stream.get(), 1, nullptr, nullptr) };
        if (!op)
            throw PaException{ "pa_stream_cork (pause) failed", pa_context_errno(_context) };

        ::pa_operation_unref(op);
    }

    void AudioOutputStream::resume()
    {
        LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Resuming stream");

        MainLoopScopedLock lock{ _mainLoop };

        {
            pa_operation* op{ ::pa_stream_cork(_stream.get(), 0, nullptr, nullptr) };
            if (!op)
                throw PaException{ "pa_stream_cork (resume) failed", pa_context_errno(_context) };
            ::pa_operation_unref(op);
        }

        {
            pa_operation* op{ ::pa_stream_trigger(_stream.get(), NULL, NULL) };
            if (!op)
                throw PaException{ "pa_stream_trigger failed", pa_context_errno(_context) };
            ::pa_operation_unref(op);
        }
    }

    bool AudioOutputStream::isPaused() const
    {
        MainLoopScopedLock lock{ _mainLoop };

        return pa_stream_is_corked(_stream.get());
    }

    void AudioOutputStream::setVolume(float volume)
    {
        MainLoopScopedLock lock{ _mainLoop };

        if (volume < 0.F || volume > 1.F)
            throw Exception{ "Volume must be in range 0-1" };

        const pa_volume_t paVol{ pa_sw_volume_from_linear(volume) };

        pa_cvolume paChanVol;
        pa_cvolume_set(&paChanVol, _outputParameters.channelCount, paVol);

        pa_operation* op{ ::pa_context_set_sink_input_volume(
            _context,
            pa_stream_get_index(_stream.get()),
            &paChanVol,
            nullptr,
            nullptr) };
        if (!op)
            throw PaException{ "pa_context_set_sink_input_volume failed", pa_context_errno(_context) };

        ::pa_operation_unref(op);

        _currentVolume = volume;
    }

    float AudioOutputStream::getVolume() const
    {
        return _currentVolume;
    }

    void AudioOutputStream::connect()
    {
        constexpr pa_stream_flags_t flags{ static_cast<pa_stream_flags_t>(
            PA_STREAM_START_CORKED | PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE) };

        const int error{ pa_stream_connect_playback(
            _stream.get(), // The stream to connect to a sink
            NULL,          // Name of the sink to connect to, or NULL to let the server decide
            NULL,          // Buffering attributes, or NULL for default
            flags,         // Additional flags, or 0 for default
            NULL,          // Initial volume, or NULL for default
            NULL           // Synchronize this stream with the specified one, or NULL for a standalone stream */
            ) };

        if (error != 0)
        {
            LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "pa_stream_connect_playback failed: " << pa_strerror(error));
            throw PaException{ "pa_stream_connect_playback failed", error };
        }
    }

    void AudioOutputStream::onStateChanged()
    {
        const pa_stream_state_t state{ pa_stream_get_state(_stream.get()) };
        LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Stream state changed to '" << streamStateToString(state) << "'");

        switch (state)
        {
        case PA_STREAM_READY:
            LMS_LOG(AUDIO_OUTPUT_STREAM, INFO, "Stream connected to device '" << pa_stream_get_device_name(_stream.get()) << "'");

            if (_waitReadyCallback)
            {
                boost::asio::post(_ioContext, std::move(_waitReadyCallback));
                _ioContext.get_executor().on_work_finished();
            }
            break;
        default:
            break;
        }
    }

    void AudioOutputStream::onWriteRequested(std::size_t writableSize)
    {
        writeSome(writableSize);

        if (_drainRequested && !_drainDone && _pendingWriteOperations.empty())
            drain();
    }

    void AudioOutputStream::writeSome(std::size_t writableSize)
    {
        while (writableSize > 0 && !_pendingWriteOperations.empty())
        {
            WriteOperation* writeOperation{ _pendingWriteOperations.front() };
            const std::span<const std::byte> buffer{ writeOperation->buffer };

            const std::size_t byteCountToWrite{ std::min(writableSize, buffer.size()) };

            pa_free_cb_t freeCallback{};
            void* freeCallbackArg{ writeOperation };

            if (byteCountToWrite == buffer.size())
            {
                _pendingWriteOperations.pop_front();
                freeCallback = [](void* userdata) {
                    WriteOperation* operation{ static_cast<WriteOperation*>(userdata) };
                    operation->stream->onWriteOperationComplete(operation);
                };
            }
            else
            {
                freeCallback = [](void* userdata) {
                    WriteOperation* operation{ static_cast<WriteOperation*>(userdata) };
                    operation->stream->onPartialWriteOperationComplete(operation);
                };
                writeOperation->buffer = std::span<const std::byte>(buffer.data() + byteCountToWrite, buffer.size() - byteCountToWrite);
            }

            LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Operation ID " << writeOperation->id << ", writing " << byteCountToWrite << " bytes");

            _ongoingWriteOperationCount++;
            const int error{
                ::pa_stream_write_ext_free(_stream.get(),    // The stream to use
                                           buffer.data(),    // The data to write
                                           byteCountToWrite, // The length of the data to write in bytes
                                           freeCallback,     // A cleanup routine for the data
                                           freeCallbackArg,  // Argument passed to free_cb function
                                           0,                // Offset for seeking
                                           PA_SEEK_RELATIVE) // Seek mode
            };
            if (error != 0)
                throw PaException{ "pa_stream_write_ext_free failed", error };

            writableSize -= byteCountToWrite;
        }
    }

    void AudioOutputStream::drain()
    {
        assert(_ongoingWriteOperationCount == 0);
        assert(_pendingWriteOperations.empty());
        assert(!_drainDone);

        LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Draining stream...");
        _drainDone = true;

        // We still receive underflow notifications while draining => discarding
        ::pa_stream_set_underflow_callback(_stream.get(), nullptr, nullptr);

        ::pa_operation* op{ ::pa_stream_drain(_stream.get(), [](pa_stream*, int success, void* userdata) { static_cast<AudioOutputStream*>(userdata)->onDrainComplete(success); }, this) };
        if (!op)
            throw PaException{ "pa_stream_drain failed", pa_context_errno(_context) };

        ::pa_operation_unref(op);
    }

    void AudioOutputStream::onDrainComplete(bool success)
    {
        {
            int error{ ::pa_stream_disconnect(_stream.get()) };
            if (error != 0)
                throw PaException{ "pa_stream_disconnect failed", error };
        }

        LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "AudioOutputStream::onDrainComplete, success = " << success << ", posting CB");
        boost::asio::post(_ioContext, std::move(_drainCallback));
        _ioContext.get_executor().on_work_finished();
    }

    AudioOutputStream::WriteOperation* AudioOutputStream::acquireWriteOperation()
    {
        if (_freeOperations.empty())
        {
            WriteOperation* operation{ &_operations.emplace_back() };
            _freeOperations.push_back(operation);
        }

        WriteOperation* operation{ _freeOperations.back() };
        _freeOperations.pop_back();
        operation->id = _nextWriteOperationId++;
        operation->stream = this;

        return operation;
    }

    void AudioOutputStream::releaseWriteOperation(WriteOperation* operation)
    {
        _freeOperations.push_back(operation);
    }

    void AudioOutputStream::onWriteOperationComplete(WriteOperation* operation)
    {
        LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Operation ID " << operation->id << ", onWriteOperationComplete");

        assert(_ongoingWriteOperationCount > 0);
        _ongoingWriteOperationCount -= 1;

        boost::asio::post(_ioContext, std::move(operation->callback));
        _ioContext.get_executor().on_work_finished();

        releaseWriteOperation(operation);
    }

    void AudioOutputStream::onPartialWriteOperationComplete(WriteOperation* operation)
    {
        LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Operation ID " << operation->id << ", onPartialWriteOperationComplete");
        assert(_ongoingWriteOperationCount > 0);
        _ongoingWriteOperationCount -= 1;
    }

    void AudioOutputStream::onWriteOperationCancelled(WriteOperation* operation)
    {
        LMS_LOG(AUDIO_OUTPUT_STREAM, DEBUG, "Operation ID " << operation->id << ", onWriteOperationCancelled");

        boost::asio::post(_ioContext, std::move(operation->callback));
        _ioContext.get_executor().on_work_finished();

        releaseWriteOperation(operation);
    }
} // namespace lms::audio::pulseaudio