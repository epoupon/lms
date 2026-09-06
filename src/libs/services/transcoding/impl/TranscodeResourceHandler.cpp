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

#include "TranscodeResourceHandler.hpp"

#include "core/ILogger.hpp"

#include "audio/Exception.hpp"
#include "audio/ITranscoder.hpp"

namespace lms::transcoding
{
    // TODO set some nice HTTP return code

#define LOG(severity, message) LMS_LOG(TRANSCODING, severity, "[" << _transcoder->getDebugId() << "] - " << message)

    ResourceHandler::ResourceHandler(boost::asio::io_context& ioContext, const audio::TranscodeParameters& parameters, std::optional<std::size_t> estimatedContentLength)
        : _estimatedContentLength{ estimatedContentLength }
    {
        try
        {
            _transcoder = createTranscoder(ioContext, parameters);

            if (_estimatedContentLength)
                LOG(DEBUG, "Estimated content length = " << *_estimatedContentLength);
            else
                LOG(DEBUG, "Not using estimated content length");
        }
        catch (audio::Exception& e)
        {
            LMS_LOG(TRANSCODING, ERROR, "Failed to create transcoder: " << e.what());
        }
    }

    ResourceHandler::~ResourceHandler() = default;

    Wt::Http::ResponseContinuation* ResourceHandler::processRequest(const Wt::Http::Request& /*request*/, Wt::Http::Response& response)
    {
        if (!_transcoder)
        {
            response.setStatus(404);
            return {};
        }

        if (_estimatedContentLength)
            response.setContentLength(*_estimatedContentLength);
        response.setMimeType(std::string{ _transcoder->getOutputMimeType() });
        LOG(DEBUG, "Transcoder finished = " << _transcoder->finished() << ", total served bytes = " << _totalServedByteCount << ", mime type = " << _transcoder->getOutputMimeType());

        if (_bytesReadyCount > 0)
        {
            LOG(DEBUG, "Writing " << _bytesReadyCount << " bytes back to client");

            response.out().write(reinterpret_cast<const char*>(_buffer.data()), _bytesReadyCount);
            _totalServedByteCount += _bytesReadyCount;
            _bytesReadyCount = 0;
        }

        if (!_transcoder->finished())
        {
            Wt::Http::ResponseContinuation* continuation{ response.createContinuation() };
            continuation->waitForMoreData();

            const std::size_t debugId{ _transcoder->getDebugId() };
            _transcoder->asyncRead(_buffer.data(), _buffer.size(), [self{ shared_from_this() }, continuation{ continuation->shared_from_this() }, debugId](std::size_t nbBytesRead) {
                LMS_LOG(TRANSCODING, DEBUG, "[" << debugId << "] - Have " << nbBytesRead << " more bytes to send back");

                assert(self->_bytesReadyCount == 0);
                self->_bytesReadyCount = nbBytesRead;
                continuation->haveMoreData();
            });

            return continuation;
        }

        // pad with 0 if necessary as duration may not be accurate
        if (_estimatedContentLength && *_estimatedContentLength > _totalServedByteCount)
        {
            const std::size_t padSize{ *_estimatedContentLength - _totalServedByteCount };

            LOG(DEBUG, "Adding " << padSize << " padding bytes");

            for (std::size_t i{}; i < padSize; ++i)
                response.out().put(0);

            _totalServedByteCount += padSize;
        }

        LOG(DEBUG, "Transcoding finished. Total served byte count = " << _totalServedByteCount);

        return {};
    }

    void ResourceHandler::abort()
    {
        if (_transcoder)
            LOG(DEBUG, "Aborted, total served bytes = " << _totalServedByteCount);

        _transcoder.reset();
    }
} // namespace lms::transcoding
