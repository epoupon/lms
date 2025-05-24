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

#pragma once

#include <array>
#include <optional>

#include "av/TranscodingParameters.hpp"
#include "core/IResourceHandler.hpp"

#include "Transcoder.hpp"

namespace lms::av::transcoding
{
    class TranscodingResourceHandler final : public IResourceHandler
    {
    public:
        TranscodingResourceHandler(const InputParameters& inputParameters, const OutputParameters& outputParameters, bool estimateContentLength);
        ~TranscodingResourceHandler() override = default;

        TranscodingResourceHandler(const TranscodingResourceHandler&) = delete;
        TranscodingResourceHandler& operator=(const TranscodingResourceHandler&) = delete;

    private:
        Wt::Http::ResponseContinuation* processRequest(const Wt::Http::Request& request, Wt::Http::Response& response) override;
        void abort() override{};

        static constexpr std::size_t _chunkSize{ 262'144 };
        std::optional<std::size_t> _estimatedContentLength;
        std::array<std::byte, _chunkSize> _buffer;
        std::size_t _bytesReadyCount{};
        std::size_t _totalServedByteCount{};
        Transcoder _transcoder;
    };
} // namespace lms::av::transcoding
