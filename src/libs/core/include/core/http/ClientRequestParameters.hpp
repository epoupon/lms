/*
 * Copyright (C) 2021 Emeric Poupon
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

#include <cstddef>
#include <functional>
#include <span>
#include <vector>

#include <Wt/Http/Message.h>

namespace lms::core::http
{
    struct ClientRequestParameters
    {
        enum class Priority
        {
            High,
            Normal,
            Low,
        };

        Priority priority{ Priority::Normal };
        std::string relativeUrl;                            // relative to baseUrl used by the client
        std::size_t responseBufferSize{ 10 * 1024 * 1024 }; // only used if onChunkReceived is not set

        // If `onChunkReceived` is set, the response will be streamed in chunks.
        // In that case, `onSuccessFunc` is still called at the end (with an empty msgBody).
        // If `onChunkReceived` is not set, the response will be fully buffered and passed to `onSuccessFunc`.
        enum class ChunckReceivedResult
        {
            Continue,
            Abort,
        };
        using OnChunkReceived = std::function<ChunckReceivedResult(std::span<const std::byte> chunk)>; // return false to stop (onFailureFunc callback will be called)
        OnChunkReceived onChunkReceived;

        using OnSuccessFunc = std::function<void(const Wt::Http::Message& msg)>;
        OnSuccessFunc onSuccessFunc;

        using OnFailureFunc = std::function<void()>;
        OnFailureFunc onFailureFunc;

        using OnAbortFunc = std::function<void()>;
        OnAbortFunc onAbortFunc;
    };

    struct ClientGETRequestParameters final : public ClientRequestParameters
    {
        std::vector<Wt::Http::Message::Header> headers;
    };

    struct ClientPOSTRequestParameters final : public ClientRequestParameters
    {
        Wt::Http::Message message;
    };

} // namespace lms::core::http