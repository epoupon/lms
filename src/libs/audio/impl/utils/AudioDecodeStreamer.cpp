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

#include "AudioDecodeStreamer.hpp"

#include <boost/asio/post.hpp>

#include "core/ILogger.hpp"

#include "audio/Exception.hpp"
#include "audio/IAudioDecoder.hpp"
#include "audio/IAudioOutput.hpp"

namespace lms::audio::utils
{
    std::unique_ptr<IAudioDecodeStreamer> createAudioDecodeStreamer(boost::asio::io_context& ioContext, const AudioDecodeStreamerParameters& parameters)
    {
        return std::make_unique<AudioDecodeStreamer>(ioContext, parameters);
    }

    AudioDecodeStreamer::AudioDecodeStreamer(boost::asio::io_context& ioContext, const AudioDecodeStreamerParameters& parameters)
        : _ioContext{ ioContext }
        , _strand{ _ioContext }
        , _outputStream{ parameters.outputStream }
    {
        prepareBuffers(parameters.bufferCount, parameters.bufferDuration);
    }

    AudioDecodeStreamer::~AudioDecodeStreamer()
    {
        assert(!isWritePending());
    }

    void AudioDecodeStreamer::start(const std::filesystem::path& path, std::chrono::microseconds offset, DecodeCompleteCallback cb)
    {
        assert(cb);
        assert(isComplete()); // previous job must be finished or cancelled

        _audioDecoder = audio::createAudioDecoder(path, offset, getPcmParameters()); // may throw
        _aborted = false;
        _eofReached = false;

        _ioContext.get_executor().on_work_started();
        _decodeCompleteCallback = std::move(cb);

        boost::asio::post(_strand, [this] {
            decodeSome();
        });
    }

    void AudioDecodeStreamer::abort()
    {
        if (isComplete())
            return;

        boost::asio::post(_strand, [this] {
            LMS_LOG(AUDIO, DEBUG, "Processing abort");
            _aborted = true;
            _outputStream.flush();

            if (!isWritePending())
                notifyDecodeComplete();
        });
    }

    bool AudioDecodeStreamer::isComplete() const
    {
        return !_audioDecoder;
    }

    const audio::PcmParameters& AudioDecodeStreamer::getPcmParameters() const
    {
        return _outputStream.getParameters();
    }

    void AudioDecodeStreamer::prepareBuffers(std::size_t bufferCount, std::chrono::microseconds bufferDuration)
    {
        const std::size_t sampleCountPerBuffer{ static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::microseconds>(bufferDuration).count() * getPcmParameters().sampleRate / std::chrono::microseconds::period::den) };
        const std::size_t bufferSize{ sampleCountToByteCount(sampleCountPerBuffer) };

        _buffers.resize(bufferCount);
        for (BufferDesc& bufferDesc : _buffers)
            bufferDesc.buffer.resize(bufferSize);
    }

    bool AudioDecodeStreamer::isWritePending() const
    {
        return std::any_of(std::cbegin(_buffers), std::cend(_buffers), [](const BufferDesc& bufferDesc) {
            return bufferDesc.isWritePending;
        });
    }

    void AudioDecodeStreamer::decodeSome()
    {
        assert(_strand.running_in_this_thread());

        while (!_eofReached && !_aborted)
        {
            BufferDesc& bufferDesc{ _buffers[_nextBufferIndex] };
            if (bufferDesc.isWritePending)
                break;

            const std::size_t bufferIndex{ _nextBufferIndex };
            if (++_nextBufferIndex >= _buffers.size())
                _nextBufferIndex = 0;

            std::span<std::byte> buffer{ bufferDesc.buffer };
            const std::size_t sampleCount{ readSamples(buffer) };
            if (sampleCount == 0) // EOF
            {
                LMS_LOG(AUDIO, DEBUG, "EOF reached");
                _eofReached = true;
                if (!isWritePending())
                    notifyDecodeComplete();

                break;
            }

            bufferDesc.isWritePending = true;
            buffer = { buffer.data(), sampleCountToByteCount(sampleCount) };

            _outputStream.asyncWrite(buffer, [this, bufferIndex] {
                boost::asio::post(_strand, [this, bufferIndex] { onBufferWriteComplete(bufferIndex); });
            });

            boost::asio::post(_strand, [this] { decodeSome(); });
        }
    }

    std::size_t AudioDecodeStreamer::readSamples(std::span<std::byte> buffer)
    {
        assert(_strand.running_in_this_thread());

        try
        {
            std::size_t totalSampleCount{};

            // Buffer must be multiple of sample
            assert(buffer.size() % (audio::getSampleSize(getPcmParameters().sampleType) * getPcmParameters().channelCount) == 0);

            while (!buffer.empty())
            {
                std::array outputBuffers{ audio::IAudioDecoder::WritableBuffer{ buffer } };
                const std::size_t sampleCount{ _audioDecoder->readSamples(outputBuffers) };
                if (sampleCount == 0)
                    break;

                const std::size_t offset{ sampleCountToByteCount(sampleCount) };
                buffer = std::span<std::byte>{ buffer.data() + offset, buffer.size() - offset };

                totalSampleCount += sampleCount;
            }

            return totalSampleCount;
        }
        catch (const audio::Exception& e)
        {
            LMS_LOG(AUDIO, ERROR, "Failed to read pcm samples: " << e.what());
            return 0;
        }
    }

    void AudioDecodeStreamer::onBufferWriteComplete(std::size_t bufferIndex)
    {
        assert(_strand.running_in_this_thread());

        BufferDesc& bufferDesc{ _buffers[bufferIndex] };

        assert(bufferDesc.isWritePending);
        bufferDesc.isWritePending = false;

        if (!_aborted && !_eofReached)
            decodeSome();
        else if (!isWritePending())
            notifyDecodeComplete();
    }

    void AudioDecodeStreamer::notifyDecodeComplete()
    {
        LMS_LOG(AUDIO, DEBUG, "Decode complete notification");

        boost::asio::post(_ioContext, [this, cb = std::move(_decodeCompleteCallback)] {
            _audioDecoder.reset();
            cb(_aborted);
            _ioContext.get_executor().on_work_finished();
        });
    }

    std::size_t AudioDecodeStreamer::sampleCountToByteCount(std::size_t sampleCount) const
    {
        return sampleCount * audio::getSampleSize(getPcmParameters().sampleType) * getPcmParameters().channelCount;
    }
} // namespace lms::audio::utils