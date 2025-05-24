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

#include "core/IResourceHandler.hpp"

#include <optional>

namespace Wt::Http
{
    class ResponseContinuation;
}

namespace lms::av::transcoding
{
    class CachingTranscoderSession;

    class CachingTranscoderClientHandler : public IResourceHandler
    {
    public:
        CachingTranscoderClientHandler(const std::shared_ptr<CachingTranscoderSession>& transcoder, bool estimateContentLength);
        ~CachingTranscoderClientHandler() override;

        CachingTranscoderClientHandler(const CachingTranscoderClientHandler&) = delete;
        CachingTranscoderClientHandler& operator=(const CachingTranscoderClientHandler&) = delete;

        bool update(std::uint64_t currentFileLength, bool done);

    private:
        Wt::Http::ResponseContinuation* processRequest(const Wt::Http::Request& request, Wt::Http::Response& response) override;
        void abort() override { _dead = true; }

        std::shared_ptr<CachingTranscoderSession> _transcoder;
        bool _dead{};
        bool _estimateContentLength;
        std::uint64_t _currentFileLength{};
        Wt::Http::ResponseContinuation* _continuation{};
        std::mutex _lock{};
        std::uint64_t _nextOffset{};
        std::uint64_t _endOffset{ UINT64_MAX };
    };

} // namespace lms::av::transcoding