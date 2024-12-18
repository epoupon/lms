/*
 * Copyright (C) 2015 Emeric Poupon
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

#include <functional>

#include "av/TranscodingParameters.hpp"

namespace lms::core
{
    class IChildProcess;
}

namespace lms::av::transcoding
{
    class Transcoder
    {
    public:
        Transcoder(const InputParameters& inputParameters, const OutputParameters& outputParameters);
        ~Transcoder();
        Transcoder(const Transcoder&) = delete;
        Transcoder& operator=(const Transcoder&) = delete;
        Transcoder(Transcoder&&) = delete;
        Transcoder& operator=(Transcoder&&) = delete;

        // non blocking calls
        using ReadCallback = std::function<void(std::size_t nbReadBytes)>;
        void asyncRead(std::byte* buffer, std::size_t bufferSize, ReadCallback);
        std::size_t readSome(std::byte* buffer, std::size_t bufferSize);

        const std::string& getOutputMimeType() const { return _outputMimeType; }
        const OutputParameters& getOutputParameters() const { return _outputParameters; }

        bool finished() const;

    private:
        static void init();

        void start();

        const std::size_t _debugId{};
        const InputParameters _inputParameters;
        const OutputParameters _outputParameters;
        std::string _outputMimeType;

        std::unique_ptr<core::IChildProcess> _childProcess;
    };
} // namespace lms::av::transcoding