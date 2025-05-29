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

#include <boost/asio.hpp>

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
        enum UpdateStatus
        {
            WORKING,
            DONE,
            ERROR,
        };
        CachingTranscoderClientHandler(const std::shared_ptr<CachingTranscoderSession>& transcoder, bool estimateContentLength);
        ~CachingTranscoderClientHandler() override;

        CachingTranscoderClientHandler(const CachingTranscoderClientHandler&) = delete;
        CachingTranscoderClientHandler& operator=(const CachingTranscoderClientHandler&) = delete;
        CachingTranscoderClientHandler(CachingTranscoderClientHandler&&) = delete;
        CachingTranscoderClientHandler& operator=(CachingTranscoderClientHandler&&) = delete;

        bool update(std::uint64_t currentFileLength, UpdateStatus status);

        void abort() override { _dead = true; }

    private:
        Wt::Http::ResponseContinuation* processRequest(const Wt::Http::Request& request, Wt::Http::Response& response) override;

        std::shared_ptr<CachingTranscoderSession> _transcoder;
        bool _dead{};
        bool _estimateContentLength;
        bool _headerSet{};
        std::atomic<std::uint64_t> _currentFileLength{};
        std::atomic<std::uint64_t> _finalFileLength{}; // Zero if not known yet
        Wt::Http::ResponseContinuation* _continuation{};
        std::uint64_t _nextOffset{};
        std::uint64_t _endOffset{ UINT64_MAX };
        boost::asio::steady_timer _signal;
    };

} // namespace lms::av::transcoding