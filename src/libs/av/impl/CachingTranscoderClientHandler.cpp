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

#include "CachingTranscoderClientHandler.hpp"
#include "CachingTranscoderSession.hpp"
#include "core/ILogger.hpp"

namespace lms::av::transcoding
{

    static std::atomic<size_t> instCount{};

    CachingTranscoderClientHandler::CachingTranscoderClientHandler(const std::shared_ptr<CachingTranscoderSession>& transcoder, bool estimateContentLength)
        : _transcoder{ transcoder }
        , _estimateContentLength{ estimateContentLength }
    {
        LMS_LOG(TRANSCODING, DEBUG, "CachingTranscoderClientHandler instances: " << ++instCount);
    }

    CachingTranscoderClientHandler::~CachingTranscoderClientHandler()
    {
        LMS_LOG(TRANSCODING, DEBUG, "CachingTranscoderClientHandler instances: " << --instCount);
    }

    bool CachingTranscoderClientHandler::update(std::uint64_t currentFileLength, bool done)
    {
        if (_dead)
            return false;
        std::lock_guard<std::mutex> guard{ _lock };
        _currentFileLength = currentFileLength;
        if (done && _endOffset > currentFileLength)
            _endOffset = currentFileLength;
        if (_continuation != nullptr && _currentFileLength > _nextOffset)
        {
            auto *tmp = _continuation;
            _continuation = nullptr;
            tmp->haveMoreData();
        }
        return true;
    }

    Wt::Http::ResponseContinuation* CachingTranscoderClientHandler::processRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        if (!request.continuation())
        {
            response.addHeader("Accept-Ranges", "bytes");
            if (_estimateContentLength && _endOffset == UINT64_MAX)
                _endOffset = _transcoder->estimatedContentLength();

            const Wt::Http::Request::ByteRangeSpecifier ranges{ request.getRanges(static_cast<int64_t>(_endOffset)) };
            if (!ranges.isSatisfiable())
            {
                response.setStatus(416); // Requested range not satisfiable
                LMS_LOG(TRANSCODING, DEBUG, "Range not satisfiable");
                return {};
            }
            if (ranges.size() == 1)
            {
                LMS_LOG(TRANSCODING, DEBUG, "Range requested = " << ranges[0].firstByte() << "-" << ranges[0].lastByte());

                response.setStatus(206);
                _nextOffset = ranges[0].firstByte();
                _endOffset = ranges[0].lastByte() + 1;

                std::ostringstream contentRange;
                contentRange << "bytes " << ranges[0].firstByte() << "-"
                             << ranges[0].lastByte() << "/*";

                response.addHeader("Content-Range", contentRange.str());
            }
            else
            {
                LMS_LOG(TRANSCODING, DEBUG, "No/multiple range requested");
                response.setStatus(200);
            }
            if (_endOffset != UINT64_MAX)
                response.setContentLength(_endOffset - _nextOffset);

            response.setMimeType(_transcoder->getOutputMimeType());
        } // end init

        std::lock_guard<std::mutex> guard{ _lock };
        if (_nextOffset >= _endOffset)
        {
            LMS_LOG(TRANSCODING, DEBUG, "Client request already done after transcoding finished and size is known");
            _dead = true;
            return nullptr;
        }
        int64_t bytesToRead = static_cast<std::int64_t>(std::min(_endOffset, _currentFileLength)) - _nextOffset;
        if (bytesToRead > 0)
        {
            int64_t bytesWritten = _transcoder->serveBytes(response.out(), _nextOffset, bytesToRead);
            if (bytesWritten > 0)
            {
                LMS_LOG(TRANSCODING, DEBUG, "Wrote " << bytesWritten << " cached bytes back to client");
                _nextOffset += bytesWritten;
            }
        }
        if (_nextOffset >= _endOffset)
        {
            LMS_LOG(TRANSCODING, DEBUG, "Range request of client fully satisfied");
            _dead = true;
            return nullptr;
        }
        _continuation = response.createContinuation();
        _continuation->waitForMoreData();
        return _continuation;
    }

} // namespace lms::av::transcoding