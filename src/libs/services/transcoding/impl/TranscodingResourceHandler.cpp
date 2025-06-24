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

#include "av/Exception.hpp"
#include "av/ITranscoder.hpp"
#include "core/ILogger.hpp"

namespace lms::transcoding
{
    std::unique_ptr<core::IResourceHandler> createResourceHandler(const av::InputParameters& inputParameters, const av::OutputParameters& outputParameters, bool estimateContentLength)
    {
        return std::make_unique<TranscodingResourceHandler>(inputParameters, outputParameters, estimateContentLength);
    }

    // TODO set some nice HTTP return code

    TranscodingResourceHandler::TranscodingResourceHandler(const av::InputParameters& inputParameters, const av::OutputParameters& outputParameters, std::optional<std::size_t> estimatedContentLength)
        : _estimatedContentLength{ estimatedContentLength }
    {
        try
        {
            _transcoder = av::createTranscoder(inputParameters, outputParameters);

            if (_estimatedContentLength)
                LMS_LOG(TRANSCODING, DEBUG, "Estimated content length = " << *_estimatedContentLength);
            else
                LMS_LOG(TRANSCODING, DEBUG, "Not using estimated content length");
        }
        catch (av::Exception& e)
        {
            LMS_LOG(TRANSCODING, ERROR, "Failed to create transcoder: " << e.what());
        }
    }

    TranscodingResourceHandler::~TranscodingResourceHandler() = default;

    Wt::Http::ResponseContinuation* TranscodingResourceHandler::processRequest(const Wt::Http::Request& /*request*/, Wt::Http::Response& response)
    {
        if (!_transcoder)
        {
            response.setStatus(404);
            return {};
        }

        if (_estimatedContentLength)
            response.setContentLength(*_estimatedContentLength);
        response.setMimeType(std::string{ _transcoder->getOutputMimeType() });
        LMS_LOG(TRANSCODING, DEBUG, "Transcoder finished = " << _transcoder->finished() << ", total served bytes = " << _totalServedByteCount << ", mime type = " << _transcoder->getOutputMimeType());

        if (_bytesReadyCount > 0)
        {
            LMS_LOG(TRANSCODING, DEBUG, "Writing " << _bytesReadyCount << " bytes back to client");

            response.out().write(reinterpret_cast<const char*>(_buffer.data()), _bytesReadyCount);
            _totalServedByteCount += _bytesReadyCount;
            _bytesReadyCount = 0;
        }

        if (!_transcoder->finished())
        {
            Wt::Http::ResponseContinuation* continuation{ response.createContinuation() };
            continuation->waitForMoreData();
            _transcoder->asyncRead(_buffer.data(), _buffer.size(), [this, continuation](std::size_t nbBytesRead) {
                LMS_LOG(TRANSCODING, DEBUG, "Have " << nbBytesRead << " more bytes to send back");

                assert(_bytesReadyCount == 0);
                _bytesReadyCount = nbBytesRead;
                continuation->haveMoreData();
            });

            return continuation;
        }

        // pad with 0 if necessary as duration may not be accurate
        if (_estimatedContentLength && *_estimatedContentLength > _totalServedByteCount)
        {
            const std::size_t padSize{ *_estimatedContentLength - _totalServedByteCount };

            LMS_LOG(TRANSCODING, DEBUG, "Adding " << padSize << " padding bytes");

            for (std::size_t i{}; i < padSize; ++i)
                response.out().put(0);

            _totalServedByteCount += padSize;
        }

        LMS_LOG(TRANSCODING, DEBUG, "Transcoding finished. Total served byte count = " << _totalServedByteCount);

        return {};
    }
} // namespace lms::transcoding
