/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "audio/AudioFeatures.hpp"

#include <bit>
#include <cstring>

#include "audio/Exception.hpp"

namespace lms::audio
{
    static_assert(sizeof(FeatureValue) == 4);
    static_assert(std::numeric_limits<FeatureValue>::is_iec559);

    namespace detail
    {
        constexpr uint32_t byteswap32(uint32_t x)
        {
            return (x >> 24) | ((x >> 8) & 0x0000FF00) | ((x << 8) & 0x00FF0000) | (x << 24);
        }

        void writeFloats(std::span<const FeatureValue> data, std::span<std::byte> blob)
        {
            if (blob.size() < data.size() * sizeof(uint32_t))
                throw Exception{ "Buffer too small to write audio features" };

            for (std::size_t i{}; i < data.size(); ++i)
            {
                uint32_t bits{ std::bit_cast<uint32_t>(data[i]) };
                if constexpr (std::endian::native == std::endian::little)
                    bits = byteswap32(bits);

                std::memcpy(blob.data() + i * 4, &bits, 4);
            }
        }

        void readFloats(std::span<const std::byte> blob, std::span<FeatureValue> data)
        {
            if (data.size() * sizeof(uint32_t) < blob.size())
                throw Exception{ "Buffer too small to read audio features" };

            for (std::size_t i{}; i < data.size(); ++i)
            {
                std::uint32_t bits;
                std::memcpy(&bits, blob.data() + i * 4, 4);
                if constexpr (std::endian::native == std::endian::little)
                    bits = byteswap32(bits);

                data[i] = std::bit_cast<FeatureValue>(bits);
            }
        }
    } // namespace detail

    void audioFeaturesToBlob(const AudioFeatures& features, std::span<std::byte> buffer)
    {
        if (buffer.size() < sizeof(AudioFeatures))
            throw Exception{ "Buffer must too small to write audio features " };

        detail::writeFloats(features.logMelEnergyMean, buffer.subspan(0, AudioFeatures::melBandCount * sizeof(FeatureValue)));
        detail::writeFloats(features.logMelEnergyStdDev, buffer.subspan(AudioFeatures::melBandCount * sizeof(FeatureValue), AudioFeatures::melBandCount * sizeof(FeatureValue)));
        detail::writeFloats(features.logMelEnergySkewness, buffer.subspan(2 * AudioFeatures::melBandCount * sizeof(FeatureValue), AudioFeatures::melBandCount * sizeof(FeatureValue)));
        detail::writeFloats(features.logMelEnergyDeltaStdDev, buffer.subspan(3 * AudioFeatures::melBandCount * sizeof(FeatureValue), AudioFeatures::melBandCount * sizeof(FeatureValue)));
    }

    void audioFeaturesFromBlob(std::span<const std::byte> buffer, AudioFeatures& features)
    {
        if (buffer.size() < sizeof(AudioFeatures))
            throw Exception{ "Buffer must too small to read audio features " };

        detail::readFloats(buffer.subspan(0, AudioFeatures::melBandCount * sizeof(FeatureValue)), features.logMelEnergyMean);
        detail::readFloats(buffer.subspan(AudioFeatures::melBandCount * sizeof(FeatureValue), AudioFeatures::melBandCount * sizeof(FeatureValue)), features.logMelEnergyStdDev);
        detail::readFloats(buffer.subspan(2 * AudioFeatures::melBandCount * sizeof(FeatureValue), AudioFeatures::melBandCount * sizeof(FeatureValue)), features.logMelEnergySkewness);
        detail::readFloats(buffer.subspan(3 * AudioFeatures::melBandCount * sizeof(FeatureValue), AudioFeatures::melBandCount * sizeof(FeatureValue)), features.logMelEnergyDeltaStdDev);
    }
} // namespace lms::audio