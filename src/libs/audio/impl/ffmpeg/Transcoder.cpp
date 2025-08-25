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
#include <iomanip>

#include "core/IChildProcessManager.hpp"
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/PseudoProtocols.hpp"
#include "core/Service.hpp"
#include "core/media/MimeType.hpp"

#include "audio/Exception.hpp"
#include "audio/TranscodeTypes.hpp"

namespace lms::audio
{
    std::unique_ptr<ITranscoder> createTranscoder(const TranscodeParameters& parameters)
    {
        return std::make_unique<ffmpeg::Transcoder>(parameters);
    }
} // namespace lms::audio

namespace lms::audio::ffmpeg
{
#define LOG(severity, message) LMS_LOG(TRANSCODING, severity, "[" << _debugId << "] - " << message)

    namespace
    {
        class FFmpegPath
        {
        public:
            FFmpegPath()
            {
                path = core::Service<core::IConfig>::get()->getPath("ffmpeg-file", "/usr/bin/ffmpeg");
                if (!std::filesystem::exists(path))
                    throw Exception{ "File '" + path.string() + "' does not exist!" };
            }

            const std::filesystem::path& get() const { return path; }

        private:
            std::filesystem::path path;
        };
    } // namespace

    std::atomic<std::size_t> Transcoder::_nextDebugId;

    Transcoder::Transcoder(const TranscodeParameters& parameters)
        : _debugId{ _nextDebugId++ }
        , _inputParams{ parameters.inputParameters }
        , _outputParams{ parameters.outputParameters }
    {
        start();
    }

    Transcoder::~Transcoder() = default;

    void Transcoder::start()
    {
        static FFmpegPath ffmpegPath;

        auto inputFile = _inputParams.filePath;
        std::chrono::milliseconds offset   = std::chrono::milliseconds::zero();
        std::chrono::milliseconds duration = std::chrono::milliseconds::zero();
        if (core::track_on.matches(inputFile)) {
            auto parsed = core::track_on.parseUri(inputFile);

            if (const auto* err = std::get_if<std::string>(&parsed); err) {
                throw Exception{ "Path claims to be a track but: " + *err };
            }

            auto&& res = std::get<core::TrackOn::DecipheredURI>(std::move(parsed));

            inputFile = std::move(res.path);
            offset    = res.start;
            duration  = res.duration;
            LOG(DEBUG, "Resolved track path " << _inputParams.filePath << " to file path " << inputFile);
        }

        try
        {
            if (!std::filesystem::exists(inputFile))
                throw Exception{ "File " + inputFile.string() + " does not exist!" };
            if (!std::filesystem::is_regular_file(inputFile))
                throw Exception{ "File " + inputFile.string() + " is not regular!" };
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            throw IOFileException{ inputFile, "Failed to check if file exists", e.code() };
        }

        LOG(INFO, "Transcoding file " << inputFile);

        std::vector<std::string> args;

        args.emplace_back(ffmpegPath.get().string());

        // TODO some codecs have restrictions, take them into account (channel count, sample rate, etc.)

        // Make sure:
        // - we do not produce anything in the stderr output
        // - we do not rely on input
        // in order not to block the whole forked process
        args.emplace_back("-loglevel");
        args.emplace_back("quiet");
        args.emplace_back("-nostdin");

        // input Offset and duration
        {
            args.emplace_back("-ss");

            std::ostringstream oss;
            oss << std::fixed << std::showpoint << std::setprecision(3) << ((offset + _inputParams.offset).count() / float{ 1'000 });
            args.emplace_back(oss.str());

            if (duration != std::chrono::milliseconds::zero()) {
                args.emplace_back("-t");
                oss.str(""); oss.clear();
                oss << std::fixed << std::showpoint << std::setprecision(3) << ((duration - _inputParams.offset).count() / float{ 1'000 });;
                args.emplace_back(oss.str());
            }
        }

        // Input file
        args.emplace_back("-i");
        args.emplace_back(inputFile);

        if (_outputParams.stripMetadata)
        {
            // Strip metadata
            args.emplace_back("-map_metadata");
            args.emplace_back("-1");
        }

        // Skip video flows (including covers!)
        args.emplace_back("-vn");

        // Output bitrates
        if (_outputParams.bitrate)
        {
            args.emplace_back("-b:a");
            args.emplace_back(std::to_string(*_outputParams.bitrate));
        }

        // -ac doc says: "For output streams it is set by default to the number of input audio channels".
        // But it looks like specifying it is required in case the original layout is not compatible with the output codec selected
        {
            args.emplace_back("-ac");
            args.emplace_back(std::to_string(_outputParams.channelCount ? *_outputParams.channelCount : _inputParams.audioProperties.channelCount));
        }

        if (_outputParams.sampleRate)
        {
            args.emplace_back("-ar");
            args.emplace_back(std::to_string(*_outputParams.sampleRate));
        }

        if (_outputParams.bitsPerSample)
        {
            args.emplace_back("-sample_fmt");
            args.emplace_back("s" + std::to_string(*_outputParams.bitsPerSample));
        }

        // Codecs and formats
        if (_outputParams.format)
        {
            args.emplace_back("-f");

            switch (_outputParams.format->container)
            {
            case core::media::Container::FLAC:
                args.emplace_back("flac");
                break;
            case core::media::Container::Ogg:
                args.emplace_back("ogg");
                break;
            case core::media::Container::MPEG:
                args.emplace_back("mp3");
                break;

            default:
                throw Exception{ "Unsupported container type " + std::string{ core::media::containerToString(_outputParams.format->container).str() } };
            }

            args.emplace_back("-acodec");

            switch (_outputParams.format->codec)
            {
            case core::media::Codec::MP3:
                args.emplace_back("libmp3lame");
                break;

            case core::media::Codec::Opus:
                args.emplace_back("libopus");
                break;

            case core::media::Codec::Vorbis:
                args.emplace_back("libvorbis");
                break;

            case core::media::Codec::FLAC:
                args.emplace_back("flac");
                break;

            default:
                throw Exception{ "Unhandled codec type " + std::string{ core::media::getCodecDesc(_outputParams.format->codec).name.str() } };
            }
        }

        args.emplace_back("pipe:1");

        if (core::Service<core::logging::ILogger>::get()->isSeverityActive(core::logging::Severity::DEBUG))
        {
            LOG(DEBUG, "Dumping args (" << args.size() << ")");
            for (const std::string& arg : args)
                LOG(DEBUG, "Arg = '" << arg << "'");
        }

        // Caution: stdin must have been closed before
        try
        {
            _childProcess = core::Service<core::IChildProcessManager>::get()->spawnChildProcess(ffmpegPath.get(), args);
        }
        catch (core::ChildProcessException& exception)
        {
            throw Exception{ "Cannot execute '" + ffmpegPath.get().string() + "': " + exception.what() };
        }
    }

    void Transcoder::asyncRead(std::byte* buffer, std::size_t bufferSize, ReadCallback readCallback)
    {
        assert(_childProcess);

        return _childProcess->asyncRead(buffer, bufferSize, [readCallback{ std::move(readCallback) }](core::IChildProcess::ReadResult /*res*/, std::size_t nbBytesRead) {
            readCallback(nbBytesRead);
        });
    }

    std::size_t Transcoder::readSome(std::byte* buffer, std::size_t bufferSize)
    {
        assert(_childProcess);

        return _childProcess->readSome(buffer, bufferSize);
    }

    std::string_view Transcoder::getOutputMimeType() const
    {
        if (_outputParams.format)
            return core::media::getMimeType(_outputParams.format->container, _outputParams.format->codec).str();

        return core::media::getMimeType(_inputParams.audioProperties.container, _inputParams.audioProperties.codec).str();
    }

    bool Transcoder::finished() const
    {
        assert(_childProcess);

        return _childProcess->finished();
    }
} // namespace lms::audio::ffmpeg
