/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "Transcoder.hpp"

#include <filesystem>
#include <span>
#include <vector>

#include <boost/asio/post.hpp>

#include "core/ILogger.hpp"
#include "core/media/MimeType.hpp"

#include "audio/Exception.hpp"
#include "audio/IAudioDecoder.hpp"
#include "audio/TranscodeTypes.hpp"

#include "AudioEncoder.hpp"
#include "AudioFile.hpp"
#include "Utils.hpp"

namespace lms::audio
{
    std::unique_ptr<ITranscoder> createTranscoder(boost::asio::io_context& ioContext, const TranscodeParameters& parameters)
    {
        return std::make_unique<ffmpeg::Transcoder>(ioContext, parameters);
    }

    bool isEncodingSupported(core::media::Container container, core::media::Codec codec)
    {
        return ffmpeg::utils::isCodecMuxingSupported(container, codec);
    }
} // namespace lms::audio

namespace lms::audio::ffmpeg
{
#define LOG(severity, message) LMS_LOG(TRANSCODING, severity, "[" << _debugId << "] - " << message)

    namespace
    {
        // How many samples per channel are pulled out of the decoder at once
        constexpr std::size_t decodeSampleCount{ 8'192 };
    } // namespace

    class Transcoder::Engine
    {
    public:
        Engine(std::size_t debugId, const TranscodeParameters& parameters);

        std::size_t read(std::byte* buffer, std::size_t bufferSize);
        bool finished() const;

        void abort() { _aborted = true; }
        bool aborted() const { return _aborted; }
        void setFailed() { _failed = true; }

    private:
        void allocatePcmBuffers();

        const std::size_t _debugId;
        std::atomic<bool> _failed{};
        std::atomic<bool> _aborted{};

        std::unique_ptr<AudioEncoder> _encoder;
        std::unique_ptr<IAudioDecoder> _decoder;
        std::vector<std::byte> _pcmBuffer;
        std::vector<IAudioDecoder::WritableBuffer> _pcmWritableBuffers;
        std::vector<AudioEncoder::ReadableBuffer> _pcmReadableBuffers;
    };

    Transcoder::Engine::Engine(std::size_t debugId, const TranscodeParameters& parameters)
        : _debugId{ debugId }
    {
        const TranscodeInputParameters& inputParams{ parameters.inputParameters };
        const TranscodeOutputParameters& outputParams{ parameters.outputParameters };

        if (!outputParams.format)
            throw Exception{ "No output format specified" };

        try
        {
            if (!std::filesystem::exists(inputParams.filePath))
                throw Exception{ "File " + inputParams.filePath.string() + " does not exist!" };
            if (!std::filesystem::is_regular_file(inputParams.filePath))
                throw Exception{ "File " + inputParams.filePath.string() + " is not regular!" };
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            throw IOFileException{ inputParams.filePath, "Failed to check if file exists", e.code() };
        }

        LOG(INFO, "Transcoding file " << inputParams.filePath);

        EncodeParameters encodeParameters{
            .container = outputParams.format->container,
            .codec = outputParams.format->codec,
            .bitrate = outputParams.bitrate,
            .bitsPerSample = outputParams.bitsPerSample ? outputParams.bitsPerSample : inputParams.audioProperties.bitsPerSample,
            .channelCount = outputParams.channelCount.value_or(inputParams.audioProperties.channelCount),
            .sampleRate = outputParams.sampleRate.value_or(inputParams.audioProperties.sampleRate),
            .metadata = {},
        };

        LOG(INFO, "Output: container = " << core::media::containerToString(encodeParameters.container) << ", codec = " << core::media::getCodecDesc(encodeParameters.codec).name
                                         << ", bitrate = " << (encodeParameters.bitrate ? std::to_string(*encodeParameters.bitrate) + " bps" : "n/a")
                                         << ", bits per sample = " << (encodeParameters.bitsPerSample ? std::to_string(*encodeParameters.bitsPerSample) : "n/a")
                                         << ", channel count = " << encodeParameters.channelCount
                                         << ", sample rate = " << encodeParameters.sampleRate);

        if (!outputParams.stripMetadata)
        {
            // Not being able to read the tags must not prevent the audio from being transcoded
            try
            {
                encodeParameters.metadata = AudioFile{ inputParams.filePath }.extractMetaData();
            }
            catch (const Exception& e)
            {
                LOG(WARNING, "Cannot extract metadata from " << inputParams.filePath << ": " << e.what());
            }
        }

        _encoder = std::make_unique<AudioEncoder>(encodeParameters);
        _decoder = createAudioDecoder(inputParams.filePath, inputParams.offset, _encoder->getInputParameters());

        allocatePcmBuffers();
    }

    void Transcoder::Engine::allocatePcmBuffers()
    {
        const PcmParameters& pcmParameters{ _encoder->getInputParameters() };
        const std::size_t bufferCount{ pcmParameters.planar ? pcmParameters.channelCount : 1 };
        const std::size_t sampleSize{ getSampleSize(pcmParameters.sampleType) };
        const std::size_t bufferSize{ decodeSampleCount * sampleSize * (pcmParameters.planar ? 1 : pcmParameters.channelCount) };

        _pcmBuffer.resize(bufferCount * bufferSize);

        for (std::size_t i{}; i < bufferCount; ++i)
        {
            const std::span<std::byte> bufferView{ std::span{ _pcmBuffer }.subspan(i * bufferSize, bufferSize) };
            _pcmWritableBuffers.emplace_back(bufferView);
            _pcmReadableBuffers.emplace_back(bufferView);
        }
    }

    std::size_t Transcoder::Engine::read(std::byte* buffer, std::size_t bufferSize)
    {
        std::size_t writtenByteCount{};

        while (writtenByteCount < bufferSize)
        {
            const std::size_t byteCount{ _encoder->readBytes(std::span<std::byte>{ buffer, bufferSize }.subspan(writtenByteCount)) };
            if (byteCount > 0)
            {
                writtenByteCount += byteCount;
                continue;
            }

            if (_encoder->finished())
                break;

            const std::size_t sampleCount{ _decoder->readSamples(_pcmWritableBuffers) };
            if (sampleCount == 0)
                _encoder->flush();
            else
                _encoder->writeSamples(_pcmReadableBuffers, sampleCount);
        }

        return writtenByteCount;
    }

    bool Transcoder::Engine::finished() const
    {
        return _failed || _encoder->finished();
    }

    std::atomic<std::size_t> Transcoder::_nextDebugId;

    Transcoder::Transcoder(boost::asio::io_context& ioContext, const TranscodeParameters& parameters)
        : _debugId{ _nextDebugId++ }
        , _inputParams{ parameters.inputParameters }
        , _outputParams{ parameters.outputParameters }
        , _strand{ boost::asio::make_strand(ioContext) }
        , _engine{ std::make_shared<Engine>(_debugId, parameters) }
    {
    }

    Transcoder::~Transcoder()
    {
        _engine->abort();
    }

    void Transcoder::asyncRead(std::byte* buffer, std::size_t bufferSize, ReadCallback readCallback)
    {
        boost::asio::post(_strand, [debugId{ _debugId }, engine{ _engine }, buffer, bufferSize, readCallback{ std::move(readCallback) }] {
            if (engine->aborted())
                return;

            std::size_t readByteCount{};

            try
            {
                readByteCount = engine->read(buffer, bufferSize);
            }
            catch (const Exception& e)
            {
                LMS_LOG(TRANSCODING, ERROR, "[" << debugId << "] - Transcoding failed: " << e.what());
                engine->setFailed();
            }

            // Forbidden to use the callback if the transcoder has been destroyed in the meantime
            if (engine->aborted())
                return;

            readCallback(readByteCount);
        });
    }

    std::size_t Transcoder::readSome(std::byte* buffer, std::size_t bufferSize)
    {
        return _engine->read(buffer, bufferSize);
    }

    std::string_view Transcoder::getOutputMimeType() const
    {
        if (_outputParams.format)
            return core::media::getMimeType(_outputParams.format->container, _outputParams.format->codec).str();

        return core::media::getMimeType(_inputParams.audioProperties.container, _inputParams.audioProperties.codec).str();
    }

    bool Transcoder::finished() const
    {
        return _engine->finished();
    }
} // namespace lms::audio::ffmpeg
