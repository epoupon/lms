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

#pragma once

#include "av/TranscodingParameters.hpp"

#include "CachingTranscoderClientHandler.hpp"
#include "Transcoder.hpp"

#include <fstream>

namespace lms::av::transcoding
{

    class CachingTranscoderSession : public std::enable_shared_from_this<CachingTranscoderSession>
    {
    public:
        CachingTranscoderSession(uint64_t hash, const std::filesystem::path &file, const InputParameters& inputParameters, const OutputParameters& outputParameters);
        ~CachingTranscoderSession();

        CachingTranscoderSession(const CachingTranscoderSession&) = delete;
        CachingTranscoderSession& operator=(const CachingTranscoderSession&) = delete;

        std::shared_ptr<CachingTranscoderClientHandler> newClient(bool estimateContentLength);

        std::uint64_t estimatedContentLength() const { return _estimatedContentLength; }
        const std::string& getOutputMimeType() const { return _transcoder.getOutputMimeType(); }

        int64_t serveBytes(std::ostream &stream, uint64_t offset, int64_t len);

    private:
        bool sourceGood() const { return _fs && _fs.good(); }
        void keepReading();
        void notifyClients(CachingTranscoderClientHandler::UpdateStatus status);

        static constexpr std::size_t CHUNK_SIZE{ 262'144 };
        std::uint64_t _estimatedContentLength;
        std::array<std::byte, CHUNK_SIZE> _buffer{};
        std::uint64_t _currentFileLength{};
        std::fstream _fs;
        std::mutex _fsMutex{};
        Transcoder _transcoder;
        std::vector<std::shared_ptr<CachingTranscoderClientHandler>> _clients{};
        std::mutex _clientMutex{};
        uint64_t _jobHash;
    };

} // namespace lms::av::transcoding
