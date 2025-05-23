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

#include "TranscodingResourceHandler.hpp"
#include "core/ILogger.hpp"

namespace lms::av::transcoding
{
    namespace
    {

        std::unordered_map<uint64_t, uint64_t> SIZE_CACHE{};
        std::mutex CACHE_LOCK{};

        uint64_t getOutputSize(const uint64_t hash)
        {
            std::lock_guard<std::mutex> lock(CACHE_LOCK);
            auto it = SIZE_CACHE.find(hash);
            return (it != SIZE_CACHE.end()) ? it->second : 0;
        }

        void storeOutputSize(const uint64_t hash, uint64_t size)
        {
            std::lock_guard<std::mutex> lock(CACHE_LOCK);
            SIZE_CACHE.emplace(hash, size);
        }

        std::size_t doEstimateContentLength(const InputParameters& inputParameters, const OutputParameters& outputParameters)
        {
            const std::size_t estimatedContentLength{ outputParameters.bitrate / 8 * static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(inputParameters.duration).count()) / 1000 };
            return estimatedContentLength;
        }
    } // namespace

    std::unique_ptr<IResourceHandler> createResourceHandler(const InputParameters& inputParameters, const OutputParameters& outputParameters, bool estimateContentLength)
    {
        return std::make_unique<TranscodingResourceHandler>(inputParameters, outputParameters, estimateContentLength);
    }

    // TODO set some nice HTTP return code

    TranscodingResourceHandler::TranscodingResourceHandler(const InputParameters& inputParameters, const OutputParameters& outputParameters, bool estimateContentLength)
        : _jobHash{ inputParameters.hash() ^ outputParameters.hash() }
        , _transcoder{ inputParameters, outputParameters }
    {
        auto size = getOutputSize(_jobHash);
        if (size != 0)
        {
            _estimatedContentLength = std::make_optional(static_cast<size_t>(size));
        }
        else if (estimateContentLength)
        {
            _estimatedContentLength = std::make_optional(doEstimateContentLength(inputParameters, outputParameters));
        }
        else
        {
            _estimatedContentLength = std::nullopt;
        }
        if (_estimatedContentLength)
            LMS_LOG(TRANSCODING, DEBUG, "Estimated content length = " << *_estimatedContentLength);
        else
            LMS_LOG(TRANSCODING, DEBUG, "Not using estimated content length");
    }

    Wt::Http::ResponseContinuation* TranscodingResourceHandler::processRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        if (!request.continuation())
        {
            response.addHeader("Accept-Ranges", "bytes");
            if (_estimatedContentLength)
            {
                response.setContentLength(*_estimatedContentLength);
                _remainingBytes = *_estimatedContentLength;
            }
            else
            {
                _remainingBytes = UINT64_MAX;
            }
            response.setMimeType(_transcoder.getOutputMimeType());
            const Wt::Http::Request::ByteRangeSpecifier ranges{ request.getRanges(_estimatedContentLength ? *_estimatedContentLength : -1) };
            if (!ranges.isSatisfiable())
            {
                std::ostringstream contentRange;
                response.setStatus(416); // Requested range not satisfiable
                response.addHeader("Content-Range", "bytes */*");

                LMS_LOG(TRANSCODING, DEBUG, "Range not satisfiable");
                return {};
            }
            if (ranges.size() == 1)
            {
                LMS_LOG(TRANSCODING, DEBUG, "Range requested = " << ranges[0].firstByte() << "-" << ranges[0].lastByte());

                response.setStatus(206);
                _firstByte = ranges[0].firstByte();
                _remainingBytes = ranges[0].lastByte() - _firstByte + 1;

                std::ostringstream contentRange;
                contentRange << "bytes " << ranges[0].firstByte() << "-"
                             << ranges[0].lastByte() << "/*";

                response.addHeader("Content-Range", contentRange.str());
                response.setContentLength(_remainingBytes);
            }
            else
            {
                LMS_LOG(TRANSCODING, DEBUG, "No/multiple range requested");

                response.setStatus(200);
                _firstByte = 0;
            }
        }
        LMS_LOG(TRANSCODING, DEBUG, "Transcoder finished = " << _transcoder.finished() << ", total served bytes = " << _totalServedByteCount << ", mime type = " << _transcoder.getOutputMimeType() << ", remaining " << _remainingBytes);

        if (_bytesReadyCount > 0)
        {
            uint64_t byteCount = 0;
            if (_bytesReadyCount + _totalServedByteCount <= _firstByte)
            {
                LMS_LOG(TRANSCODING, DEBUG, "Throwing " << _bytesReadyCount << " bytes away");
            }
            else if (_totalServedByteCount >= _firstByte)
            {
                byteCount = std::min(_bytesReadyCount, _remainingBytes);
                LMS_LOG(TRANSCODING, DEBUG, "Writing " << byteCount << " bytes back to client");
                response.out().write(reinterpret_cast<const char*>(_buffer.data()), byteCount);
            } else {
                // Overlap
                const uint64_t offset = _firstByte - _totalServedByteCount;
                byteCount = std::min(_bytesReadyCount - offset, _remainingBytes);
                LMS_LOG(TRANSCODING, DEBUG, "Writing " << byteCount << " bytes  back to client while skipping " << offset);
                response.out().write(reinterpret_cast<const char*>(_buffer.data()) + offset, byteCount);
            }

            if (_remainingBytes != UINT64_MAX)
            {
                _remainingBytes -= byteCount;
            }
            _totalServedByteCount += _bytesReadyCount;
            _bytesReadyCount = 0;
        }

        if (!_transcoder.finished() && _remainingBytes != 0)
        {
            Wt::Http::ResponseContinuation* continuation{ response.createContinuation() };
            continuation->waitForMoreData();
            _transcoder.asyncRead(_buffer.data(), _buffer.size(), [this, continuation](std::size_t nbBytesRead) {
                LMS_LOG(TRANSCODING, DEBUG, "Have " << nbBytesRead << " more bytes to send back");

                assert(_bytesReadyCount == 0);
                _bytesReadyCount = nbBytesRead;
                continuation->haveMoreData();
            });

            return continuation;
        }

        storeOutputSize(_jobHash, _totalServedByteCount);

        // pad with 0 if necessary as duration may not be accurate
        if (_remainingBytes != UINT64_MAX && _remainingBytes != 0)
        {
            LMS_LOG(TRANSCODING, DEBUG, "Adding " << _remainingBytes << " padding bytes");
            _buffer.fill(std::byte(0));

            while (_remainingBytes > 0) {
                const auto num = std::min(_remainingBytes, _buffer.size());
                response.out().write(reinterpret_cast<const char*>(_buffer.data()), num);
                _remainingBytes -= num;
            }

            _totalServedByteCount += _remainingBytes;
        }

        LMS_LOG(TRANSCODING, DEBUG, "Transcoding finished. Total served byte count = " << _totalServedByteCount);

        return {};
    }
} // namespace lms::av::transcoding
