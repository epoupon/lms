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

#include "AudioEncoder.hpp"

#include <algorithm>
#include <array>
#include <cassert>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

#include "core/ILogger.hpp"

#include "audio/Exception.hpp"

#include "EncoderUtils.hpp"
#include "Exception.hpp"
#include "Utils.hpp"

namespace lms::audio::ffmpeg
{
    namespace
    {
        // Small enough to keep the streaming latency low, large enough not to call back for every packet
        constexpr int avioBufferSize{ 32'768 };

        // Used by encoders that accept a variable frame size
        constexpr int defaultFrameSampleCount{ 4'096 };

        int writePacketCallback(void* opaque, const std::uint8_t* buffer, int bufferSize)
        {
            auto& outputBuffer{ *static_cast<std::vector<std::byte>*>(opaque) };

            const std::byte* data{ static_cast<const std::byte*>(static_cast<const void*>(buffer)) };
            outputBuffer.insert(std::end(outputBuffer), data, data + bufferSize);

            return bufferSize;
        }

        PcmSampleType bitsPerSampleToPcmSampleType(std::optional<unsigned> bitsPerSample)
        {
            if (bitsPerSample && *bitsPerSample <= 16)
                return PcmSampleType::Signed16;

            return PcmSampleType::Signed32;
        }
    } // namespace

    AudioEncoder::AudioEncoder(const EncodeParameters& parameters)
    {
        assert(utils::isInit());

        const AVCodec* encoder{ utils::findEncoder(parameters.codec) };

        createOutputContext(parameters);
        createEncoder(parameters, *encoder);
        writeHeader(parameters);
        allocateBuffers();
    }

    AudioEncoder::~AudioEncoder() = default;

    void AudioEncoder::createOutputContext(const EncodeParameters& parameters)
    {
        {
            ::AVFormatContext* context{};
            const int error{ ::avformat_alloc_output_context2(&context, nullptr, utils::getMuxerName(parameters.container), nullptr) };
            if (error < 0 || !context)
                throw FFmpegException{ "Cannot allocate output context", error };

            _formatContext = AVFormatContextOutputPtr{ context };
        }

        {
            auto* buffer{ static_cast<std::uint8_t*>(::av_malloc(avioBufferSize)) };
            if (!buffer)
                throw Exception{ "Cannot allocate IO buffer" };

            ::AVIOContext* context{ ::avio_alloc_context(buffer, avioBufferSize, 1, &_outputBuffer, nullptr, &writePacketCallback, nullptr) };
            if (!context)
            {
                ::av_free(buffer);
                throw Exception{ "Cannot allocate IO context" };
            }

            _ioContext = AVIOContextPtr{ context };
        }

        // Output is not seekable, just like when piping out the ffmpeg command output
        _formatContext->pb = _ioContext.get();
        _formatContext->flags |= AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_FLUSH_PACKETS;
    }

    void AudioEncoder::createEncoder(const EncodeParameters& parameters, const AVCodec& encoder)
    {
        _encoderContext = AVCodecContextPtr{ ::avcodec_alloc_context3(&encoder) };
        if (!_encoderContext)
            throw Exception{ "Cannot allocate encoder context" };

        const ::AVSampleFormat sampleFormat{ utils::pickSampleFormat(encoder, bitsPerSampleToPcmSampleType(parameters.bitsPerSample)) };
        const int sampleRate{ utils::pickSampleRate(encoder, parameters.sampleRate) };
        utils::pickChannelLayout(encoder, parameters.channelCount, _encoderContext->ch_layout);

        _encoderContext->sample_fmt = sampleFormat;
        _encoderContext->sample_rate = sampleRate;
        _encoderContext->time_base = AVRational{ 1, sampleRate };

        if (parameters.bitrate && !core::media::getCodecDesc(parameters.codec).isLossless)
            _encoderContext->bit_rate = *parameters.bitrate;

        if (parameters.bitsPerSample && sampleFormat == AV_SAMPLE_FMT_S32)
            _encoderContext->bits_per_raw_sample = static_cast<int>(*parameters.bitsPerSample);

        if (_formatContext->oformat->flags & AVFMT_GLOBALHEADER)
            _encoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        // Native Opus and Vorbis encoders are still flagged as experimental
        if (encoder.capabilities & AV_CODEC_CAP_EXPERIMENTAL)
            _encoderContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

        LMS_LOG(TRANSCODING, DEBUG, "Using encoder '" << encoder.name << "', sample format = " << ::av_get_sample_fmt_name(sampleFormat)
                                                      << ", sample rate = " << sampleRate << ", channel count = " << _encoderContext->ch_layout.nb_channels);

        const int error{ ::avcodec_open2(_encoderContext.get(), &encoder, nullptr) };
        if (error < 0)
            throw FFmpegException{ "Cannot open encoder '" + std::string{ encoder.name } + "'", error };

        _inputParameters = PcmParameters{
            .channelCount = static_cast<unsigned>(_encoderContext->ch_layout.nb_channels),
            .sampleRate = static_cast<unsigned>(sampleRate),
            .sampleType = utils::toPcmSampleType(sampleFormat),
            .byteOrder = std::endian::native,
            .planar = ::av_sample_fmt_is_planar(sampleFormat) != 0,
        };
    }

    void AudioEncoder::writeHeader(const EncodeParameters& parameters)
    {
        _stream = ::avformat_new_stream(_formatContext.get(), nullptr);
        if (!_stream)
            throw Exception{ "Cannot allocate output stream" };

        {
            const int error{ ::avcodec_parameters_from_context(_stream->codecpar, _encoderContext.get()) };
            if (error < 0)
                throw FFmpegException{ "Cannot set output stream parameters", error };
        }
        _stream->time_base = _encoderContext->time_base;

        // Keys are passed as is: muxers match them case insensitively and convert them to their own representation
        for (const auto& [key, value] : parameters.metadata)
        {
            const int error{ ::av_dict_set(&_formatContext->metadata, key.c_str(), value.c_str(), 0) };
            if (error < 0)
                LMS_LOG(TRANSCODING, WARNING, "Cannot set metadata '" << key << "': " << utils::averrorToString(error));
        }

        const int error{ ::avformat_write_header(_formatContext.get(), nullptr) };
        if (error < 0)
            throw FFmpegException{ "Cannot write output header", error };
    }

    void AudioEncoder::allocateBuffers()
    {
        _frameSampleCount = _encoderContext->frame_size > 0 ? _encoderContext->frame_size : defaultFrameSampleCount;

        _frame = AVFramePtr{ ::av_frame_alloc() };
        if (!_frame)
            throw Exception{ "Cannot allocate encoding frame" };

        _frame->nb_samples = _frameSampleCount;
        _frame->format = _encoderContext->sample_fmt;
        _frame->sample_rate = _encoderContext->sample_rate;
        {
            const int error{ ::av_channel_layout_copy(&_frame->ch_layout, &_encoderContext->ch_layout) };
            if (error < 0)
                throw FFmpegException{ "Cannot set frame channel layout", error };
        }

        {
            const int error{ ::av_frame_get_buffer(_frame.get(), 0) };
            if (error < 0)
                throw FFmpegException{ "Cannot allocate frame buffer", error };
        }

        _packet = AVPacketPtr{ ::av_packet_alloc() };
        if (!_packet)
            throw Exception{ "Cannot allocate output packet" };

        _fifo = AVAudioFifoPtr{ ::av_audio_fifo_alloc(_encoderContext->sample_fmt, _encoderContext->ch_layout.nb_channels, _frameSampleCount) };
        if (!_fifo)
            throw Exception{ "Cannot allocate sample FIFO" };
    }

    void AudioEncoder::writeSamples(std::span<const ReadableBuffer> inputChannelBuffers, std::size_t sampleCountPerChannel)
    {
        assert(!_flushed);

        if (sampleCountPerChannel == 0)
            return;

        const std::size_t expectedBufferCount{ _inputParameters.planar ? _inputParameters.channelCount : 1 };
        if (inputChannelBuffers.size() != expectedBufferCount)
            throw Exception{ "Expected " + std::to_string(expectedBufferCount) + " input buffers, got " + std::to_string(inputChannelBuffers.size()) };

        std::array<void*, AV_NUM_DATA_POINTERS> inputData{};
        for (std::size_t i{}; i < inputChannelBuffers.size(); ++i)
            inputData[i] = const_cast<void*>(static_cast<const void*>(inputChannelBuffers[i].data()));

        const int writtenSampleCount{ ::av_audio_fifo_write(_fifo.get(), inputData.data(), static_cast<int>(sampleCountPerChannel)) };
        if (writtenSampleCount < 0)
            throw FFmpegException{ "Cannot write to the sample FIFO", writtenSampleCount };
        if (static_cast<std::size_t>(writtenSampleCount) != sampleCountPerChannel)
            throw Exception{ "Sample FIFO did not accept all the samples" };

        encodePendingFrames();
    }

    void AudioEncoder::encodePendingFrames()
    {
        while (::av_audio_fifo_size(_fifo.get()) >= _frameSampleCount)
            encodeFrame(_frameSampleCount);
    }

    void AudioEncoder::encodeFrame(int sampleCount)
    {
        _frame->nb_samples = sampleCount;

        const int error{ ::av_frame_make_writable(_frame.get()) };
        if (error < 0)
            throw FFmpegException{ "Cannot make the encoding frame writable", error };

        const int readSampleCount{ ::av_audio_fifo_read(_fifo.get(), static_cast<void**>(static_cast<void*>(_frame->extended_data)), sampleCount) };
        if (readSampleCount < 0)
            throw FFmpegException{ "Cannot read from the sample FIFO", readSampleCount };

        _frame->pts = _nextPts;
        _nextPts += readSampleCount;

        sendFrame(_frame.get());
    }

    void AudioEncoder::sendFrame(const AVFrame* frame)
    {
        const int sendError{ ::avcodec_send_frame(_encoderContext.get(), frame) };
        if (sendError < 0)
            throw FFmpegException{ "avcodec_send_frame failed", sendError };

        while (true)
        {
            const int recvError{ ::avcodec_receive_packet(_encoderContext.get(), _packet.get()) };
            if (recvError == AVERROR(EAGAIN) || recvError == AVERROR_EOF)
                break;
            if (recvError < 0)
                throw FFmpegException{ "avcodec_receive_packet failed", recvError };

            _packet->stream_index = _stream->index;
            ::av_packet_rescale_ts(_packet.get(), _encoderContext->time_base, _stream->time_base);

            const int writeError{ ::av_interleaved_write_frame(_formatContext.get(), _packet.get()) };
            ::av_packet_unref(_packet.get());
            if (writeError < 0)
                throw FFmpegException{ "av_interleaved_write_frame failed", writeError };
        }
    }

    void AudioEncoder::flush()
    {
        if (_flushed)
            return;

        _flushed = true;

        const int remainingSampleCount{ ::av_audio_fifo_size(_fifo.get()) };
        if (remainingSampleCount > 0)
            encodeFrame(remainingSampleCount);

        sendFrame(nullptr);

        const int error{ ::av_write_trailer(_formatContext.get()) };
        if (error < 0)
            throw FFmpegException{ "Cannot write output trailer", error };
    }

    std::size_t AudioEncoder::readBytes(std::span<std::byte> buffer)
    {
        const std::size_t availableByteCount{ _outputBuffer.size() - _outputReadIndex };
        const std::size_t byteCount{ std::min(availableByteCount, buffer.size()) };
        if (byteCount == 0)
            return 0;

        std::copy_n(std::cbegin(_outputBuffer) + _outputReadIndex, byteCount, std::begin(buffer));
        _outputReadIndex += byteCount;

        if (_outputReadIndex == _outputBuffer.size())
        {
            _outputBuffer.clear();
            _outputReadIndex = 0;
        }
        else if (_outputReadIndex > _outputBuffer.size() / 2)
        {
            _outputBuffer.erase(std::begin(_outputBuffer), std::begin(_outputBuffer) + _outputReadIndex);
            _outputReadIndex = 0;
        }

        return byteCount;
    }

    bool AudioEncoder::finished() const
    {
        return _flushed && _outputReadIndex == _outputBuffer.size();
    }
} // namespace lms::audio::ffmpeg
