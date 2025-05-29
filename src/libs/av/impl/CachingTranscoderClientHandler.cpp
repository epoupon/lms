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

#include <utility> // XXX: Fixes compile on Debian 12 for me (See #681)

#include "CachingTranscoderSession.hpp"
#include "core/IChildProcessManager.hpp"
#include "core/ILogger.hpp"

namespace lms::av::transcoding
{

    static std::atomic<size_t> instCount{}; // XXX: Just during development to find memory leaks through dangling/cyclic references

    CachingTranscoderClientHandler::CachingTranscoderClientHandler(const std::shared_ptr<CachingTranscoderSession>& transcoder, bool estimateContentLength)
        : _transcoder{ transcoder }
        , _estimateContentLength{ estimateContentLength }
        , _signal{ core::Service<core::IChildProcessManager>::get()->ioContext() }
    {
        LMS_LOG(TRANSCODING, DEBUG, "CachingTranscoderClientHandler instances: " << ++instCount);
    }

    CachingTranscoderClientHandler::~CachingTranscoderClientHandler()
    {
        LMS_LOG(TRANSCODING, DEBUG, "CachingTranscoderClientHandler instances: " << --instCount);
    }

    bool CachingTranscoderClientHandler::update(std::uint64_t currentFileLength, UpdateStatus status)
    {
        LMS_LOG(TRANSCODING, WARNING, "Client->update() when " << (_dead ? "DEAD" : "alive"));
        if (_dead)
            return false;
        if (status == UpdateStatus::ERROR)
        {
            _signal.cancel();
            _dead = true;
            return false;
        }
        assert(currentFileLength >= _currentFileLength);
        _currentFileLength = currentFileLength;
        if (status == UpdateStatus::DONE)
            _finalFileLength = currentFileLength;
        if (_currentFileLength > _nextOffset || status != WORKING)
        {
            // This is being called from another thread (potentially), so use a timer for signalling,
            // as calling _continuation->haveMoreData() here will deadlock. Also something something io_context.
            _signal.cancel();
        }
        return true;
    }

    Wt::Http::ResponseContinuation* CachingTranscoderClientHandler::processRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        if (_dead)
            return nullptr;
        if (!_headerSet)
        {
            _headerSet = true;
            LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: Initial processRequest");
            response.addHeader("Accept-Ranges", "bytes");
            if (_estimateContentLength && _endOffset == UINT64_MAX)
                _endOffset = _transcoder->estimatedContentLength();

            const Wt::Http::Request::ByteRangeSpecifier ranges{ request.getRanges(static_cast<int64_t>(_endOffset)) };
            if (!ranges.isSatisfiable())
            {
                response.setStatus(416); // Requested range not satisfiable
                LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: Range not satisfiable");
                return {};
            }
            if (ranges.size() == 1)
            {
                LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: Range requested = " << ranges[0].firstByte() << "-" << ranges[0].lastByte());

                response.setStatus(206);
                _nextOffset = ranges[0].firstByte();
                _endOffset = ranges[0].lastByte() + 1;

                std::ostringstream contentRange;
                contentRange << "bytes " << ranges[0].firstByte() << "-"
                             << ranges[0].lastByte() << "/";
                if (_finalFileLength)
                    contentRange << _finalFileLength;
                else if (_endOffset != UINT64_MAX)
                    contentRange << _transcoder->estimatedContentLength();
                else
                    contentRange << "*";

                response.addHeader("Content-Range", contentRange.str());
            }
            else
            {
                LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: No/multiple ranges requested");
                response.setStatus(200);
            }
            if (_endOffset != UINT64_MAX)
                response.setContentLength(_endOffset - _nextOffset);

            response.setMimeType(_transcoder->getOutputMimeType());
        } // end init

        int64_t bytesReady = static_cast<std::int64_t>(std::min<std::uint64_t>(_endOffset, _currentFileLength)) - _nextOffset;
        int64_t bytesWritten{};
        if (bytesReady <= 0)
            LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: Still have to wait for " << -bytesReady << " more transcoded bytes before being able to handle client");
        else
        {
            bytesWritten = _transcoder->serveBytes(response.out(), _nextOffset, bytesReady);
            LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: Wrote " << bytesWritten << "/" << bytesReady << " bytes to client");
            if (bytesWritten >= 0)
                _nextOffset += bytesWritten;
            else
            {
                // I/O Error
                _dead = true;
                return nullptr;
            }
        }
        if (_finalFileLength && _nextOffset >= _finalFileLength && _nextOffset < _endOffset)
        {
            // Transcode finished, and read pos is past end of transcoded file
            if (_endOffset == UINT64_MAX)
            {
                // No content length was sent, no range requested - just end this request
                LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: End of file, no content-length, finished");
                _dead = true;
                return nullptr;
            }
            // We promised the client there would be more data than there actually is - pad with zeros
            const std::size_t padSize{ _endOffset - _nextOffset };
            LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: Adding " << padSize << " padding bytes");

            for (std::size_t i{}; i < padSize; ++i)
                response.out().put(0);

            _nextOffset += padSize;
        }
        if (_nextOffset >= _endOffset)
        {
            LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: Range request of client fully satisfied");
            _dead = true;
            return nullptr;
        }

        // Still some work to do

        if (bytesWritten > 0 && _currentFileLength > _nextOffset)
        {
            // We made progress, and there's still data ready in cache - can continue directly
            LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: Continue directly");
            _continuation = nullptr;
            return response.createContinuation();
        }

        // Need to wait for transcoder
        LMS_LOG(TRANSCODING, DEBUG, "CACHE PROCESSOR: Wait for more data");
        _continuation = response.createContinuation();
        _continuation->waitForMoreData();
        _signal.expires_after(std::chrono::seconds(60));
        _signal.async_wait([this](const boost::system::error_code& ec) {
            if (_dead)
                return;
            _signal.expires_after(std::chrono::seconds(60));
            if (ec != boost::asio::error::operation_aborted)
            {
                // This should never happen but let's see
                _dead = true;
                LMS_LOG(TRANSCODING, WARNING, "CACHE PROCESSOR: Client timer expired, this should not happen :>");
            }
            this->_continuation->haveMoreData(); // Will end the request if we set _dead above
        });
        return _continuation;
    }

} // namespace lms::av::transcoding