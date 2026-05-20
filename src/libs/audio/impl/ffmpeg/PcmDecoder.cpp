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

#include "PcmDecoder.hpp"

#include <algorithm>
#include <array>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include "core/ILogger.hpp"

#include "audio/Exception.hpp"
#include "audio/IPcmDecoder.hpp"

#include "Exception.hpp"

namespace lms::audio
{
    std::unique_ptr<IPcmDecoder> createPcmDecoder(const std::filesystem::path& filePath, std::chrono::microseconds offset, const PcmParameters& parameters)
    {
        return std::make_unique<ffmpeg::PcmDecoder>(filePath, offset, parameters);
    }
} // namespace lms::audio

namespace lms::audio::ffmpeg
{
    namespace
    {
        ::AVSampleFormat toAvSampleFormat(PcmSampleType type, bool planar)
        {
            switch (type)
            {
            case PcmSampleType::Signed16:
                return planar ? AV_SAMPLE_FMT_S16P : AV_SAMPLE_FMT_S16;
            case PcmSampleType::Signed32:
                return planar ? AV_SAMPLE_FMT_S32P : AV_SAMPLE_FMT_S32;
            case PcmSampleType::Float32:
                return planar ? AV_SAMPLE_FMT_FLTP : AV_SAMPLE_FMT_FLT;
            case PcmSampleType::Float64:
                return planar ? AV_SAMPLE_FMT_DBLP : AV_SAMPLE_FMT_DBL;
            }

            throw Exception("Unsupported PcmSampleType");
        }
    } // namespace

    PcmDecoder::PcmDecoder(const std::filesystem::path& filePath, std::chrono::microseconds offset, const PcmParameters& parameters)
        : _parameters{ parameters }
    {
        if (_parameters.channelCount > AV_NUM_DATA_POINTERS)
            throw Exception("Channel count exceeds maximum supported channels");

        utils::init();

        // TODO: use AudioFile wrapper?
        {
            ::AVFormatContext* context{};
            int error{ ::avformat_open_input(&context, filePath.c_str(), nullptr, nullptr) };
            if (error < 0)
            {
                LMS_LOG(AUDIO, ERROR, "Cannot open " << filePath << ": " << utils::averrorToString(error));
                throw FFmpegException{ "Cannot open '" + filePath.string() + "'", error };
            }
            _context = AVFormatContextPtr{ context };
        }

        {
            int error{ ::avformat_find_stream_info(_context.get(), nullptr) };
            if (error < 0)
            {
                LMS_LOG(AUDIO, ERROR, "Cannot find stream information in " << filePath << ": " << utils::averrorToString(error));
                throw FFmpegException{ "Cannot find stream information in '" + filePath.string() + "'", error };
            }
        }

        const ::AVCodec* decoder{};
        _inputStreamIndex = ::av_find_best_stream(_context.get(),
                                                  AVMEDIA_TYPE_AUDIO,
                                                  -1, // auto
                                                  -1, // auto
                                                  &decoder,
                                                  0);

        if (_inputStreamIndex < 0)
        {
            LMS_LOG(AUDIO, ERROR, "Cannot find best audio stream in " << filePath << ": " << utils::averrorToString(_inputStreamIndex));
            throw FFmpegException{ "Cannot find best audio stream in '" + filePath.string() + "'", _inputStreamIndex };
        }

        if (offset.count() > 0)
        {
            const AVStream* stream{ _context->streams[_inputStreamIndex] };

            using OffsetPeriod = decltype(offset)::period;
            constexpr AVRational offsetTimebase{ static_cast<int>(OffsetPeriod::num), static_cast<int>(OffsetPeriod::den) };

            const int64_t targetTimestamp{ static_cast<int64_t>(av_rescale_q(offset.count(), offsetTimebase, stream->time_base)) };
            const int seekError{ ::av_seek_frame(_context.get(), _inputStreamIndex, targetTimestamp, AVSEEK_FLAG_BACKWARD) };
            if (seekError < 0)
            {
                LMS_LOG(AUDIO, WARNING, "Failed to seek to offset: " << utils::averrorToString(seekError));
            }
        }

        {
            _estimatedDuration = std::chrono::milliseconds{ _context->duration == AV_NOPTS_VALUE ? 0 : _context->duration / AV_TIME_BASE * 1'000 };
            if (_estimatedDuration > offset)
                _estimatedDuration = _estimatedDuration - std::chrono::duration_cast<std::chrono::milliseconds>(offset);
            else
                _estimatedDuration = {};
        }

        _decoderContext = AVCodecContextPtr{ ::avcodec_alloc_context3(decoder) };
        if (!_decoderContext)
            throw Exception{ "Cannot allocate decoder context" };

        {
            int error{ ::avcodec_parameters_to_context(_decoderContext.get(), _context->streams[_inputStreamIndex]->codecpar) };
            if (error < 0)
                throw FFmpegException{ "Cannot init decoder parameters", error };
        }

        {
            int error{ ::avcodec_open2(_decoderContext.get(), decoder, nullptr) };
            if (error < 0)
                throw FFmpegException("Cannot open decoder", error);
        }

        _decodedFrame = AVFramePtr{ av_frame_alloc() };
        if (!_decodedFrame)
            throw Exception{ "Cannot allocate decoded frame" };

        _inputPacket = AVPacketPtr{ ::av_packet_alloc() };
        if (!_inputPacket)
            throw Exception{ "Cannot allocate input packet" };

        // Resampler
        const ::AVSampleFormat outFmt{ toAvSampleFormat(_parameters.sampleType, _parameters.planar) };
        AVChannelLayout outLayout;
        ::av_channel_layout_default(&outLayout, _parameters.channelCount);

        {
            ::SwrContext* context{};
            ::swr_alloc_set_opts2(
                &context,                                 // existing context
                &outLayout,                               // out layout
                outFmt,                                   // out format
                static_cast<int>(_parameters.sampleRate), // out rate
                &_decoderContext->ch_layout,              // in layout
                _decoderContext->sample_fmt,              // in format
                _decoderContext->sample_rate,             // in rate
                0,                                        // log offset
                nullptr);
            ::av_channel_layout_uninit(&outLayout);

            if (!context)
                throw Exception{ "Cannot allocate resampler context" };

            _resampleContext = SwrContextPtr{ context };
        }

        {
            int error{ ::swr_init(_resampleContext.get()) };
            if (error < 0)
                throw FFmpegException{ "Cannot initialize resampler", error };
        }
    }

    PcmDecoder::~PcmDecoder() = default;

    const PcmParameters& PcmDecoder::getParameters() const
    {
        return _parameters;
    }

    std::size_t PcmDecoder::readSamples(std::span<WritableBuffer> outputChannelBuffers)
    {
        if (_finished)
            return 0;

        const std::size_t maxSamplesPerChannel{ computeSampleCountPerChannel(outputChannelBuffers) };

        if (getEstimatedResamplerAvailableSamples() >= maxSamplesPerChannel)
            return drainResampler(outputChannelBuffers, maxSamplesPerChannel);

        while (true)
        {
            if (!_eof)
                feedDecoder();

            // Try to receive a decoded frame
            int recvErr{ ::avcodec_receive_frame(_decoderContext.get(), _decodedFrame.get()) };
            if (recvErr == AVERROR(EAGAIN))
            {
                if (!_eof)
                    continue; // need more input

                _draining = true;
            }
            else if (recvErr == AVERROR_EOF)
            {
                _draining = true;
            }
            else if (recvErr < 0)
            {
                throw FFmpegException{ "avcodec_receive_frame failed", recvErr };
            }
            else
            {
                std::array<uint8_t*, AV_NUM_DATA_POINTERS> outData{};
                for (std::size_t i{}; i < outputChannelBuffers.size(); ++i)
                    outData[i] = reinterpret_cast<uint8_t*>(outputChannelBuffers[i].data());

                // Resample decoded audio
                const int outSampleCount{ ::swr_convert(
                    _resampleContext.get(),
                    outData.data(),
                    static_cast<int>(maxSamplesPerChannel),
                    (const uint8_t**)_decodedFrame->data,
                    _decodedFrame->nb_samples) };
                ::av_frame_unref(_decodedFrame.get());

                if (outSampleCount < 0)
                    throw FFmpegException{ "swr_convert failed", outSampleCount };

                if (outSampleCount > 0)
                    return static_cast<std::size_t>(outSampleCount);

                continue; // Rare but legal: frame produced no output (delay accumulation)
            }

            // Drain resampler once decoder is drained
            if (_draining)
            {
                const std::size_t outSampleCount{ drainResampler(outputChannelBuffers, maxSamplesPerChannel) };
                if (outSampleCount > 0)
                    return outSampleCount;

                _finished = true;
                break;
            }
        }

        return 0;
    }

    bool PcmDecoder::finished() const
    {
        return _finished;
    }

    std::chrono::milliseconds PcmDecoder::getEstimatedDuration() const
    {
        return _estimatedDuration;
    }

    std::size_t PcmDecoder::computeSampleCountPerChannel(std::span<WritableBuffer> outputChannelBuffers) const
    {
        if (_parameters.planar)
        {
            if (outputChannelBuffers.size() != _parameters.channelCount)
                throw Exception{ "Expected " + std::to_string(_parameters.channelCount) + " buffers for planar output" };

            // Each planar buffer holds samples for one channel only
            const int bytesPerSample{ av_get_bytes_per_sample(toAvSampleFormat(_parameters.sampleType, true)) };
            if (bytesPerSample <= 0)
                throw Exception{ "Invalid bytes per sample for output format" };

            const std::size_t sampleCount{ outputChannelBuffers[0].size() / bytesPerSample };
            if (!std::all_of(std::cbegin(outputChannelBuffers), std::cend(outputChannelBuffers), [&](const WritableBuffer& buffer) { return buffer.size() == outputChannelBuffers[0].size(); }))
                throw Exception{ "All planar channel buffers must have the same size" };

            return sampleCount;
        }

        // interleaved
        if (outputChannelBuffers.size() != 1)
            throw Exception{ "Expected a single buffer for interleaved output" };

        const int bytesPerSample = av_get_bytes_per_sample(toAvSampleFormat(_parameters.sampleType, false));
        if (bytesPerSample <= 0)
            throw Exception{ "Invalid bytes per sample for output format" };

        // Divide by (bytes per sample * number of channels) for interleaved
        const std::size_t sampleCount = outputChannelBuffers[0].size() / (bytesPerSample * _parameters.channelCount);

        return sampleCount;
    }

    void PcmDecoder::feedDecoder()
    {
        assert(!_eof);

        const int readError{ ::av_read_frame(_context.get(), _inputPacket.get()) };
        if (readError == AVERROR_EOF)
        {
            _eof = true;
            // flush decoder
            ::avcodec_send_packet(_decoderContext.get(), nullptr);
        }
        else if (readError < 0)
        {
            throw FFmpegException{ "av_read_frame failed", readError };
        }
        else
        {
            if (_inputPacket->stream_index == _inputStreamIndex)
            {
                int sendError{ ::avcodec_send_packet(_decoderContext.get(), _inputPacket.get()) };
                ::av_packet_unref(_inputPacket.get());
                if (sendError == AVERROR_INVALIDDATA)
                {
                    // we may be close to the end of the file, abort gracefully *only* if next packet is EOF
                    const int peekError{ ::av_read_frame(_context.get(), _inputPacket.get()) };
                    ::av_packet_unref(_inputPacket.get());
                    if (peekError == AVERROR_EOF)
                    {
                        LMS_LOG(AUDIO, DEBUG, "Invalid data in packet at end of file");
                        _eof = true;
                        sendError = 0;
                    }
                }
                if (sendError < 0)
                    throw FFmpegException{ "avcodec_send_packet failed", sendError };
            }
            else
                ::av_packet_unref(_inputPacket.get());
        }
    }

    std::size_t PcmDecoder::drainResampler(std::span<WritableBuffer> outputChannelBuffers, std::size_t maxSamplesPerChannel)
    {
        std::array<uint8_t*, AV_NUM_DATA_POINTERS> outData{};
        for (std::size_t i{}; i < outputChannelBuffers.size(); ++i)
            outData[i] = reinterpret_cast<uint8_t*>(outputChannelBuffers[i].data());

        const int outSampleCount{ ::swr_convert(_resampleContext.get(),
                                                outData.data(),
                                                static_cast<int>(maxSamplesPerChannel),
                                                nullptr,
                                                0) };

        if (outSampleCount < 0)
            throw FFmpegException{ "swr_convert (drain) failed", outSampleCount };

        return outSampleCount;
    }

    std::size_t PcmDecoder::getEstimatedResamplerAvailableSamples() const
    {
        const int64_t delayedInputSampleCount{ ::swr_get_delay(_resampleContext.get(), _decoderContext->sample_rate) };
        const int64_t sampleCount{ av_rescale_rnd(delayedInputSampleCount, _parameters.sampleRate, _decoderContext->sample_rate, AV_ROUND_UP) };

        return sampleCount;
    }
} // namespace lms::audio::ffmpeg