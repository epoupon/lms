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
                std::uint32_t bits{};
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

            uint32_t bits{};
            std::memcpy(&bits, blob.data(), 4);
            if constexpr (std::endian::native == std::endian::little)
                bits = byteswap32(bits);

            value = std::bit_cast<FeatureValue>(bits);
        }

        void writeAudioFeaturesPatchStatsFields(const AudioFeaturesPatchStats& features, std::span<std::byte> buffer)
        {
            std::size_t offset{};

            writeFloats(features.logMelMean, buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)));
            offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

            writeFloats(features.logMelStdDev, buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)));
            offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

            writeFloats(features.mfccMean, buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)));
            offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

            writeFloats(features.mfccStdDev, buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)));
            offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

            writeFloat(features.spectralCentroidMean, buffer.subspan(offset, sizeof(FeatureValue)));
            offset += sizeof(FeatureValue);

            writeFloat(features.spectralCentroidStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
            offset += sizeof(FeatureValue);

            writeFloat(features.spectralRolloffMean, buffer.subspan(offset, sizeof(FeatureValue)));
            offset += sizeof(FeatureValue);

            writeFloat(features.spectralRolloffStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
            offset += sizeof(FeatureValue);

            writeFloat(features.spectralFluxMean, buffer.subspan(offset, sizeof(FeatureValue)));
            offset += sizeof(FeatureValue);

            writeFloat(features.spectralFluxStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
            offset += sizeof(FeatureValue);

            writeFloat(features.onsetStrengthMean, buffer.subspan(offset, sizeof(FeatureValue)));
            offset += sizeof(FeatureValue);

            writeFloat(features.onsetStrengthStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
            offset += sizeof(FeatureValue);

            writeFloats(features.chromaMean, buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)));
            offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

            writeFloats(features.chromaStdDev, buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)));
            offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

            writeFloat(features.zeroCrossingRateMean, buffer.subspan(offset, sizeof(FeatureValue)));
            offset += sizeof(FeatureValue);

            writeFloat(features.zeroCrossingRateStdDev, buffer.subspan(offset, sizeof(FeatureValue)));
            offset += sizeof(FeatureValue);
        }

        void readAudioFeaturesPatchStatsFields(std::span<const std::byte> buffer, AudioFeaturesPatchStats& features)
        {
            std::size_t offset{};

            readFloats(buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)), features.logMelMean);
            offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

            readFloats(buffer.subspan(offset, AudioFeatures::melBandCount * sizeof(FeatureValue)), features.logMelStdDev);
            offset += AudioFeatures::melBandCount * sizeof(FeatureValue);

            readFloats(buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)), features.mfccMean);
            offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

            readFloats(buffer.subspan(offset, AudioFeatures::mfccCount * sizeof(FeatureValue)), features.mfccStdDev);
            offset += AudioFeatures::mfccCount * sizeof(FeatureValue);

            readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralCentroidMean);
            offset += sizeof(FeatureValue);

            readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralCentroidStdDev);
            offset += sizeof(FeatureValue);

            readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralRolloffMean);
            offset += sizeof(FeatureValue);

            readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralRolloffStdDev);
            offset += sizeof(FeatureValue);

            readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralFluxMean);
            offset += sizeof(FeatureValue);

            readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.spectralFluxStdDev);
            offset += sizeof(FeatureValue);

            readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.onsetStrengthMean);
            offset += sizeof(FeatureValue);

            readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.onsetStrengthStdDev);
            offset += sizeof(FeatureValue);

            readFloats(buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)), features.chromaMean);
            offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

            readFloats(buffer.subspan(offset, AudioFeatures::chromaCount * sizeof(FeatureValue)), features.chromaStdDev);
            offset += AudioFeatures::chromaCount * sizeof(FeatureValue);

            readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.zeroCrossingRateMean);
            offset += sizeof(FeatureValue);

            readFloat(buffer.subspan(offset, sizeof(FeatureValue)), features.zeroCrossingRateStdDev);
            offset += sizeof(FeatureValue);
        }
    } // namespace detail

    void trackAudioFeaturesToBlob(const TrackAudioFeatures& features, std::span<std::byte> buffer)
    {
        if (buffer.size() < sizeof(TrackAudioFeatures))
            throw Exception{ "Buffer must too small to write track audio features " };

        detail::writeAudioFeaturesPatchStatsFields(features.mean, buffer.subspan(0, sizeof(AudioFeaturesPatchStats)));
    }

    void trackAudioFeaturesFromBlob(std::span<const std::byte> buffer, TrackAudioFeatures& features)
    {
        if (buffer.size() < sizeof(TrackAudioFeatures))
            throw Exception{ "Buffer must too small to read track audio features " };

        detail::readAudioFeaturesPatchStatsFields(buffer.subspan(0, sizeof(AudioFeaturesPatchStats)), features.mean);
    }

    std::ostream& operator<<(std::ostream& os, const AudioFeatures& features)
    {
        for (std::size_t m{}; m < audio::AudioFeatures::melBandCount; ++m)
        {
            os << "Log mel filter " << m << '\n';
            os << "\tvalue = " << features.logMel[m] << '\n';
        }

        for (std::size_t k{}; k < audio::AudioFeatures::mfccCount; ++k)
        {
            os << "MFCC " << k << '\n';
            os << "\tvalue = " << features.mfcc[k] << '\n';
        }

        os << "Spectral centroid = " << features.spectralCentroid << '\n';
        os << "Spectral rolloff = " << features.spectralRolloff << '\n';
        os << "Spectral flux = " << features.spectralFlux << '\n';
        os << "Onset strength = " << features.onsetStrength << '\n';

        for (std::size_t c{}; c < audio::AudioFeatures::chromaCount; ++c)
        {
            os << "Chroma " << c << '\n';
            os << "\tvalue = " << features.chroma[c] << '\n';
        }

        os << "Zero crossing rate = " << features.zeroCrossingRate << '\n';

        return os;
    }

    std::ostream& operator<<(std::ostream& os, const AudioFeaturesPatchStats& features)
    {
        for (std::size_t m{}; m < audio::AudioFeatures::melBandCount; ++m)
        {
            os << "Log mel filter " << m << '\n';
            os << "\tmean = " << features.logMelMean[m] << ", stddev = " << features.logMelStdDev[m] << '\n';
        }

        for (std::size_t k{}; k < audio::AudioFeatures::mfccCount; ++k)
        {
            os << "MFCC " << k << '\n';
            os << "\tmean = " << features.mfccMean[k] << ", stddev = " << features.mfccStdDev[k] << '\n';
        }

        os << "Spectral centroid. Mean = " << features.spectralCentroidMean << ", stddev = " << features.spectralCentroidStdDev << '\n';
        os << "Spectral rolloff. Mean = " << features.spectralRolloffMean << ", stddev = " << features.spectralRolloffStdDev << '\n';
        os << "Spectral flux. Mean = " << features.spectralFluxMean << ", stddev = " << features.spectralFluxStdDev << '\n';
        os << "Onset strength. Mean = " << features.onsetStrengthMean << ", stddev = " << features.onsetStrengthStdDev << '\n';

        for (std::size_t c{}; c < audio::AudioFeatures::chromaCount; ++c)
        {
            os << "Chroma " << c << '\n';
            os << "\tmean = " << features.chromaMean[c] << ", stddev = " << features.chromaStdDev[c] << '\n';
        }

        os << "Zero crossing rate. Mean = " << features.zeroCrossingRateMean << ", stddev = " << features.zeroCrossingRateStdDev << '\n';

        return os;
    }

    std::ostream& operator<<(std::ostream& os, const TrackAudioFeatures& features)
    {
        os << "Track audio features mean patch stats\n"
           << features.mean;

        return os;
    }
} // namespace lms::audio