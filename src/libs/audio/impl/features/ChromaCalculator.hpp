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

#include <array>
#include <cmath>
#include <span>

namespace lms::audio::features
{
    template<std::size_t N, typename FloatType = float>
    class ChromaCalculator
    {
    public:
        static constexpr std::size_t chromaCount{ 12 };

        using Input = std::span<const FloatType, N>;
        using Output = std::array<FloatType, chromaCount>;

        explicit ChromaCalculator(FloatType sampleRate)
            : _sampleRate{ sampleRate }
            , _refFreq{ 16.35 } // C0
            , _binToChroma{ computeBinToChroma(_sampleRate, _refFreq) }
        {
        }

        static constexpr FloatType silenceThreshold{ FloatType{ 1e-12 } };

        [[nodiscard]] Output apply(Input spectrum) const
        {
            Output chroma{};
            for (std::size_t k = 1; k < N; ++k)
            {
                const FloatType p{ spectrum[k] };
                if (p <= silenceThreshold)
                    continue;

                chroma[_binToChroma[k]] += p;
            }

            // normalize
            FloatType sum{};
            for (auto v : chroma)
                sum += v;

            if (sum > silenceThreshold)
            {
                const FloatType inv{ FloatType{ 1 } / sum };
                for (auto& v : chroma)
                    v *= inv;
            }
            else
                chroma.fill(FloatType{ 0 });

            return chroma;
        }

    private:
        static constexpr std::array<std::size_t, N> computeBinToChroma(const FloatType sampleRate, const FloatType refFreq)
        {
            std::array<std::size_t, N> binToChroma;

            const FloatType binWidth{ sampleRate / (FloatType{ 2 } * FloatType{ N }) };
            for (std::size_t k{}; k < N; ++k)
            {
                const FloatType freq{ static_cast<FloatType>(k) * binWidth };

                if (k == 0 || freq <= FloatType{ 0 })
                {
                    binToChroma[k] = 0;
                    continue;
                }

                const FloatType midi{ FloatType{ 12 } * std::log2(freq / refFreq) };
                int c{ static_cast<int>(std::floor(midi)) % 12 };
                if (c < 0)
                    c += 12;

                binToChroma[k] = static_cast<std::size_t>(c);
            }

            return binToChroma;
        }

        FloatType _sampleRate;
        FloatType _refFreq;
        std::array<std::size_t, N> _binToChroma;
    };
} // namespace lms::audio::features