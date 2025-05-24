/*
 * Copyright (C) 2025 Simon Rettberg
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

#include "CachingTranscoderSession.hpp"
#include "CachingTranscoderClientHandler.hpp"
#include "TranscodingResourceHandler.hpp"
#include "av/RawResourceHandlerCreator.hpp"
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"

namespace lms::av::transcoding
{
    namespace
    {
        const std::vector<std::size_t> ALLOWED_BITRATES{320'000, 256'000, 192'000, 160'000, 128'000, 96'000, 64'000, 32'000};

        std::atomic<size_t> instCount{};

        std::unordered_map<uint64_t, std::shared_ptr<CachingTranscoderSession>> jobs{};
        std::mutex jobMutex{};

        std::uint64_t doEstimateContentLength(const InputParameters& inputParameters, const OutputParameters& outputParameters)
        {
            return outputParameters.bitrate / 8 * static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(inputParameters.duration).count()) / 1000;
        }

        std::filesystem::path getCacheFile(const std::filesystem::path &cachePath, uint64_t hash)
        {
            std::ostringstream oss;
            oss << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << hash;
            auto subDir = cachePath / oss.str().substr(0, 1);
            std::error_code ec;
            std::filesystem::create_directories(subDir, ec);
            if (ec)
            {
                LMS_LOG(TRANSCODING, WARNING, "Error creating cache sub directory");
                return {};
            }
            return subDir / oss.str();
        }

        void removeJobFromMap(uint64_t hash)
        {
            std::lock_guard<std::mutex> guard{ jobMutex };
            if (!jobs.erase(hash))
                LMS_LOG(TRANSCODING, DEBUG, "remove Job: Not found!");
        }
    } // namespace

    std::shared_ptr<IResourceHandler> createCachingResourceHandler(const std::filesystem::path& cachePath, const InputParameters& inputParameters, const OutputParameters& outputParameters, bool estimateContentLength)
    {
        assert(outputParameters.offset.count() == 0);
        // Snap to predefined values, to increase chances of cache hit
        OutputParameters copy = outputParameters;
        for (auto rate : ALLOWED_BITRATES)
        {
            if (copy.bitrate > rate)
                copy.bitrate = rate;
        }
        // Enforce minimum
        copy.bitrate = std::max(copy.bitrate, ALLOWED_BITRATES.back());
        // Lookup in cache
        auto hash = inputParameters.hash() ^ outputParameters.hash();
        std::lock_guard<std::mutex> guard{ jobMutex };
        auto it = jobs.find(hash);
        std::shared_ptr<CachingTranscoderSession> job{};
        if (it != jobs.end())
        {
            // Ongoing transcode, attach
            LMS_LOG(TRANSCODING, DEBUG, "Existing transcode job, attaching");
            job = it->second;
        }
        else
        {
            // No active job, check cache
            auto filePath = getCacheFile(cachePath, hash);
            std::error_code ec;
            if (!filePath.empty() && is_regular_file(filePath, ec))
            {
                LMS_LOG(TRANSCODING, DEBUG, "Transcoded file already in cache");
                auto handler = av::createRawResourceHandler(filePath, formatToMimetype(outputParameters.format));
                if (handler->sourceGood()) // File is readable, return raw handler
                    return handler;
                LMS_LOG(TRANSCODING, DEBUG, "File access error, transcoding file again");
            }
            LMS_LOG(TRANSCODING, DEBUG, "Starting shared caching transcoder");
            job = std::make_shared<CachingTranscoderSession>(hash, filePath, inputParameters, outputParameters);
            jobs.emplace(hash, job);
        }
        if (job)
            return job->newClient(estimateContentLength);
        // Something went wrong; fall back to plain old transcoding handler
        LMS_LOG(TRANSCODING, DEBUG, "Falling back to simple transcoder");
        return std::make_unique<TranscodingResourceHandler>(inputParameters, outputParameters, estimateContentLength);
    }

    std::shared_ptr<CachingTranscoderClientHandler> CachingTranscoderSession::newClient(bool estimateContentLength)
    {
        std::shared_ptr<CachingTranscoderClientHandler> ptr{ std::make_shared<CachingTranscoderClientHandler>(shared_from_this(), estimateContentLength) };
        {
            std::lock_guard<std::mutex> guard{ _clientMutex };
            _clients.emplace_back(ptr);
        }
        ptr->update(_currentFileLength, false);
        return ptr;
    }

    CachingTranscoderSession::CachingTranscoderSession(uint64_t hash, const std::filesystem::path &file, const InputParameters& inputParameters, const OutputParameters& outputParameters)
        : _estimatedContentLength{ doEstimateContentLength(inputParameters, outputParameters) }
        , _fs{ file, std::ios::in | std::ios::out | std::ios::binary }
        , _transcoder{ inputParameters, outputParameters }
        , _jobHash{ hash }
    {
        LMS_LOG(TRANSCODING, DEBUG, "CachingTranscoderSession instances: " << ++instCount);
        LMS_LOG(TRANSCODING, DEBUG, "Estimated content length = " << _estimatedContentLength);
    }

    CachingTranscoderSession::~CachingTranscoderSession()
    {
        LMS_LOG(TRANSCODING, DEBUG, "CachingTranscoderSession instances: " << --instCount);
    }

    int64_t CachingTranscoderSession::serveBytes(std::ostream& stream, uint64_t offset, int64_t len)
    {
        std::lock_guard<std::mutex> guard{ _fsMutex };
        if (offset <= _currentFileLength || len <= 0)
            return 0;
        std::vector<char> buffer(std::min<std::size_t>(len, CHUNK_SIZE));
        _fs.seekg(offset);
        _fs.read(buffer.data(), buffer.size());
        if (!_fs.good())
        {
            LMS_LOG(TRANSCODING, WARNING, "Error on cached file fstream while serving a client");
            return -1;
        }
        stream.write(buffer.data(), _fs.gcount());
        return _fs.gcount();
    }

    void CachingTranscoderSession::keepReading()
    {
        if (_transcoder.finished())
        {
            {
                std::lock_guard<std::mutex> guard{ _clientMutex };
                LMS_LOG(TRANSCODING, DEBUG, "Caching transcoder job finished, clients left: " << _clients.size());
                for (const auto& ptr : _clients)
                {
                    ptr->update(_currentFileLength, true);
                }
                _clients.clear();
            }
            removeJobFromMap(_jobHash);
            return;
        }

        _transcoder.asyncRead(_buffer.data(), _buffer.size(), [this](std::size_t nbBytesRead) {
            LMS_LOG(TRANSCODING, DEBUG, "Have " << nbBytesRead << " more bytes to send back and cache");
            if (nbBytesRead)
            {
                std::lock_guard<std::mutex> guard{ _fsMutex };
                _fs.write(reinterpret_cast<const char*>(_buffer.data()), nbBytesRead);
                if (_fs.good())
                    _currentFileLength += nbBytesRead;
                else
                    LMS_LOG(TRANSCODING, WARNING, "Error writing to transcoded cache file");
            }
            int done{};
            std::size_t remaining{};
            {
                std::lock_guard<std::mutex> guard{ _clientMutex };
                for (int i = static_cast<int>(_clients.size()) - 1; i >= 0; --i)
                {
                    if (!_clients[i]->update(_currentFileLength, false))
                    {
                        done++;
                        _clients[i] = std::move(_clients.back());
                        _clients.pop_back();
                    }
                }
                remaining = _clients.size();
            }
            if (done)
                LMS_LOG(TRANSCODING, DEBUG, "Clients done with shared transcoder: " << done << ", remaining: " << remaining);
            this->keepReading();
        });

    }
} // namespace lms::av::transcoding
