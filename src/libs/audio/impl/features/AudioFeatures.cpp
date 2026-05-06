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
#include <ostream>

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

        void writeFloat(FeatureValue value, std::span<std::byte> blob)
        {
            if (blob.size() < sizeof(uint32_t))
                throw Exception{ "Buffer too small to write audio feature" };

            uint32_t bits{ std::bit_cast<uint32_t>(value) };
            if constexpr (std::endian::native == std::endian::little)
                bits = byteswap32(bits);

            std::memcpy(blob.data(), &bits, 4);
        }

        void readFloat(std::span<const std::byte> blob, FeatureValue& value)
        {
            if (blob.size() < sizeof(uint32_t))
                throw Exception{ "Buffer too small to read audio feature" };

            uint32_t bits;
            std::memcpy(&bits, blob.data(), 4);
            if constexpr (std::endian::native == std::endian::little)
                bits = byteswap32(bits);

            value = std::bit_cast<FeatureValue>(bits);
        }
    } // namespace detail

    void audioFeaturesToBlob(const AudioFeatures& features, std::span<std::byte> buffer)
    {
        if (buffer.size() < sizeof(AudioFeatures))
            throw Exception{ "Buffer must too small to write audio features " };

        std::size_t offset{};

        detail::writeFloats(features.logMelEnergyMean, buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)));
        offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

        detail::writeFloats(features.logMelEnergyStdDev, buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)));
        offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

        detail::writeFloats(features.logMelEnergyDeltaStdDev, buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)));
        offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

        detail::writeFloats(features.logMelEnergyDeltaMeanAbs, buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)));
        offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

        detail::writeFloats(features.mfccMean, buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)));
        offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

        detail::writeFloats(features.mfccStdDev, buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)));
        offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

        detail::writeFloats(features.mfccDeltaStdDev, buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)));
        offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

        detail::writeFloats(features.mfccDeltaMeanAbs, buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)));
        offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

        detail::writeFloat(features.spectralCentroidMean, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.spectralCentroidStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.spectralCentroidDeltaMeanAbs, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.spectralCentroidDeltaStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.spectralRolloffMean, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.spectralRolloffStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.spectralRolloffDeltaMeanAbs, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.spectralRolloffDeltaStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.spectralFluxMean, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.spectralFluxStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.spectralFluxDeltaMeanAbs, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloats(features.chromaMean, buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)));
        offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

        detail::writeFloats(features.chromaStdDev, buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)));
        offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

        detail::writeFloats(features.chromaDeltaStdDev, buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)));
        offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

        detail::writeFloats(features.chromaDeltaMeanAbs, buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)));
        offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

        detail::writeFloat(features.zeroCrossingRateMean, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);

        detail::writeFloat(features.zeroCrossingRateStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
        offset += sizeof(FeatureValue);
    }

    void audioFeaturesFromBlob(std::span<const std::byte> buffer, AudioFeatures& features)
    {
        if (buffer.size() < sizeof(AudioFeatures))
            throw Exception{ "Buffer must too small to read audio features " };

        std::size_t offset{};

        detail::readFloats(buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)), features.logMelEnergyMean);
        offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)), features.logMelEnergyStdDev);
        offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)), features.logMelEnergyDeltaStdDev);
        offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)), features.logMelEnergyDeltaMeanAbs);
        offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)), features.mfccMean);
        offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)), features.mfccStdDev);
        offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)), features.mfccDeltaStdDev);
        offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)), features.mfccDeltaMeanAbs);
        offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralCentroidMean);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralCentroidStdDev);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralCentroidDeltaMeanAbs);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralCentroidDeltaStdDev);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralRolloffMean);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralRolloffStdDev);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralRolloffDeltaMeanAbs);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralRolloffDeltaStdDev);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralFluxMean);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralFluxStdDev);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralFluxDeltaMeanAbs);
        offset += sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)), features.chromaMean);
        offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)), features.chromaStdDev);
        offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)), features.chromaDeltaStdDev);
        offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

        detail::readFloats(buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)), features.chromaDeltaMeanAbs);
        offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.zeroCrossingRateMean);
        offset += sizeof(FeatureValue);

        detail::readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.zeroCrossingRateStdDev);
        offset += sizeof(FeatureValue);
    }

    std::ostream& operator<<(std::ostream& os, const AudioFeatures& features)
    {
        for (std::size_t m{}; m < audio::AudioFeatures::melBandCount; ++m)
        {
            os << "Log mel filter " << m << '\n';
            os << "\tmean = " << features.logMelEnergyMean[m] << ", stddev = " << features.logMelEnergyStdDev[m] << '\n';
            os << "\tDelta. stddev = " << features.logMelEnergyDeltaStdDev[m] << ", mean abs = " << features.logMelEnergyDeltaMeanAbs[m] << '\n';
        }

        for (std::size_t k{}; k < audio::AudioFeatures::mfccCount; ++k)
        {
            os << "MFCC " << k << '\n';
            os << "\tmean = " << features.mfccMean[k] << ", stddev = " << features.mfccStdDev[k] << '\n';
            os << "\tDelta. stddev = " << features.mfccDeltaStdDev[k] << ", mean abs = " << features.mfccDeltaMeanAbs[k] << '\n';
        }

        os << "Spectral centroid. Mean = " << features.spectralCentroidMean << ", stddev = " << features.spectralCentroidStdDev << '\n';
        os << "Spectral centroid delta. Mean abs = " << features.spectralCentroidDeltaMeanAbs << ", stddev = " << features.spectralCentroidDeltaStdDev << '\n';
        os << "Spectral rolloff. Mean = " << features.spectralRolloffMean << ", stddev = " << features.spectralRolloffStdDev << '\n';
        os << "Spectral rolloff delta. Mean abs = " << features.spectralRolloffDeltaMeanAbs << ", stddev = " << features.spectralRolloffDeltaStdDev << '\n';
        os << "Spectral flux. Mean = " << features.spectralFluxMean << ", stddev = " << features.spectralFluxStdDev << '\n';
        os << "Spectral flux delta. Mean abs = " << features.spectralFluxDeltaMeanAbs << '\n';

        for (std::size_t c{}; c < audio::AudioFeatures::chromaCount; ++c)
        {
            os << "Chroma " << c << '\n';
            os << "\tmean = " << features.chromaMean[c] << ", stddev = " << features.chromaStdDev[c] << '\n';
            os << "\tDelta. stddev = " << features.chromaDeltaStdDev[c] << ", mean abs = " << features.chromaDeltaMeanAbs[c] << '\n';
        }

        os << "Zero crossing rate. Mean = " << features.zeroCrossingRateMean << ", stddev = " << features.zeroCrossingRateStdDev << '\n';

        return os;
    }
} // namespace lms::audio