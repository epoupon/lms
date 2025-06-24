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

#include <atomic>
#include <iomanip>

#include "core/IChildProcessManager.hpp"
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Service.hpp"

#include "av/Exception.hpp"

namespace lms::av
{
#define LOG(severity, message) LMS_LOG(TRANSCODING, severity, "[" << _debugId << "] - " << message)

    std::unique_ptr<ITranscoder> createTranscoder(const InputParameters& inputParameters, const OutputParameters& outputParameters)
    {
        return std::make_unique<Transcoder>(inputParameters, outputParameters);
    }

    static std::atomic<size_t> globalId{};
    static std::filesystem::path ffmpegPath;

    void Transcoder::init()
    {
        ffmpegPath = core::Service<core::IConfig>::get()->getPath("ffmpeg-file", "/usr/bin/ffmpeg");
        if (!std::filesystem::exists(ffmpegPath))
            throw Exception{ "File '" + ffmpegPath.string() + "' does not exist!" };
    }

    Transcoder::Transcoder(const InputParameters& inputParams, const OutputParameters& outputParams)
        : _debugId{ globalId++ }
        , _inputParams{ inputParams }
        , _outputParams{ outputParams }
    {
        start();
    }

    Transcoder::~Transcoder() = default;

    void Transcoder::start()
    {
        if (ffmpegPath.empty())
            init();

        try
        {
            if (!std::filesystem::exists(_inputParams.file))
                throw Exception{ "File " + _inputParams.file.string() + " does not exist!" };
            if (!std::filesystem::is_regular_file(_inputParams.file))
                throw Exception{ "File " + _inputParams.file.string() + " is not regular!" };
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            // TODO store/raise e.code()
            throw Exception{ "File error '" + _inputParams.file.string() + "': " + e.what() };
        }

        LOG(INFO, "Transcoding file " << _inputParams.file);

        std::vector<std::string> args;

        args.emplace_back(ffmpegPath.string());

        // Make sure:
        // - we do not produce anything in the stderr output
        // - we do not rely on input
        // in order not to block the whole forked process
        args.emplace_back("-loglevel");
        args.emplace_back("quiet");
        args.emplace_back("-nostdin");

        // input Offset
        {
            args.emplace_back("-ss");

            std::ostringstream oss;
            oss << std::fixed << std::showpoint << std::setprecision(3) << (_inputParams.offset.count() / float{ 1'000 });
            args.emplace_back(oss.str());
        }

        // Input file
        args.emplace_back("-i");
        args.emplace_back(_inputParams.file.string());

        // Stream mapping, if set
        if (_inputParams.streamIndex)
        {
            args.emplace_back("-map");
            args.emplace_back("0:" + std::to_string(*_inputParams.streamIndex));
        }

        if (_outputParams.stripMetadata)
        {
            // Strip metadata
            args.emplace_back("-map_metadata");
            args.emplace_back("-1");
        }

        // Skip video flows (including covers)
        args.emplace_back("-vn");

        // Output bitrates
        args.emplace_back("-b:a");
        args.emplace_back(std::to_string(_outputParams.bitrate));

        // Codecs and formats
        switch (_outputParams.format)
        {
        case OutputFormat::MP3:
            args.emplace_back("-f");
            args.emplace_back("mp3");
            break;

        case OutputFormat::OGG_OPUS:
            args.emplace_back("-acodec");
            args.emplace_back("libopus");
            args.emplace_back("-f");
            args.emplace_back("ogg");
            break;

        case OutputFormat::MATROSKA_OPUS:
            args.emplace_back("-acodec");
            args.emplace_back("libopus");
            args.emplace_back("-f");
            args.emplace_back("matroska");
            break;

        case OutputFormat::OGG_VORBIS:
            args.emplace_back("-acodec");
            args.emplace_back("libvorbis");
            args.emplace_back("-f");
            args.emplace_back("ogg");
            break;

        case OutputFormat::WEBM_VORBIS:
            args.emplace_back("-acodec");
            args.emplace_back("libvorbis");
            args.emplace_back("-f");
            args.emplace_back("webm");
            break;

        default:
            throw Exception{ "Unhandled format (" + std::to_string(static_cast<int>(_outputParams.format)) + ")" };
        }

        args.emplace_back("pipe:1");

        LOG(DEBUG, "Dumping args (" << args.size() << ")");
        for (const std::string& arg : args)
            LOG(DEBUG, "Arg = '" << arg << "'");

        // Caution: stdin must have been closed before
        try
        {
            _childProcess = core::Service<core::IChildProcessManager>::get()->spawnChildProcess(ffmpegPath, args);
        }
        catch (core::ChildProcessException& exception)
        {
            throw Exception{ "Cannot execute '" + ffmpegPath.string() + "': " + exception.what() };
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
        switch (_outputParams.format)
        {
        case OutputFormat::MP3:
            return "audio/mpeg";
        case OutputFormat::OGG_OPUS:
            return "audio/opus";
        case OutputFormat::MATROSKA_OPUS:
            return "audio/x-matroska";
        case OutputFormat::OGG_VORBIS:
            return "audio/ogg";
        case OutputFormat::WEBM_VORBIS:
            return "audio/webm";
        }

        return "application/octet-stream"; // default, should not happen
    }

    bool Transcoder::finished() const
    {
        assert(_childProcess);

        return _childProcess->finished();
    }

} // namespace lms::av
