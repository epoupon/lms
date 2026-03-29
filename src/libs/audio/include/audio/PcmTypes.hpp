/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <bit>
#include <chrono>

namespace lms::audio
{
    enum class PcmSampleType
    {
        Signed16,
        Signed32,
        Float32,
        Float64,
    };

    std::size_t getSampleSize(PcmSampleType type);

    struct PcmParameters
    {
        unsigned channelCount;
        unsigned sampleRate;
        PcmSampleType sampleType;
        std::endian byteOrder;
        bool planar;
    };

    namespace helpers
    {
        template<typename Rep, typename Period>
        std::size_t durationToSampleCount(std::chrono::duration<Rep, Period> duration, unsigned sampleRate)
        {
            return static_cast<std::size_t>(duration.count() * sampleRate * Period::num / Period::den);
        }

        template<typename Duration>
        Duration sampleCountToDuration(std::size_t sampleCount, unsigned sampleRate)
        {
            return std::chrono::duration_cast<Duration>(std::chrono::duration<double>(sampleCount) / sampleRate);
        }

        std::size_t sampleCountToByteCount(std::size_t sampleCount, PcmSampleType sampleType, unsigned channelCount);
    } // namespace helpers
} // namespace lms::audio