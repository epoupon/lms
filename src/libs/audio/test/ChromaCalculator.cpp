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

#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <numeric>

#include "math/Entropy.hpp"

#include "features/ChromaCalculator.hpp"

namespace lms::audio::features::chromaTests
{
    template<std::size_t N, typename FloatType = float>
    class SyntheticSpectrum
    {
    public:
        explicit SyntheticSpectrum(FloatType sampleRate)
            : _binWidth{ sampleRate / (FloatType{ 2 } * FloatType{ N }) }
        {
        }

        void addTone(FloatType freq, FloatType amplitude = FloatType{ 1 })
        {
            const FloatType logCenter = std::log2(freq);

            for (std::size_t i{}; i < N; ++i)
            {
                const FloatType binFreq = std::log2(static_cast<FloatType>(i + 1));
                const FloatType d = binFreq - logCenter;
                const FloatType w = std::exp(-(d * d) * FloatType{ 12 });

                _spectrum[i] += amplitude * w;
            }
        }

        void clear()
        {
            _spectrum.fill(FloatType{ 0 });
        }

        void scale(FloatType f)
        {
            for (auto& v : _spectrum)
                v *= f;
        }

        const std::array<FloatType, N>& data() const
        {
            return _spectrum;
        }

    private:
        FloatType _binWidth;
        std::array<FloatType, N> _spectrum{};
    };

    class ChromaCalculatorTest : public ::testing::Test
    {
    protected:
        using FloatType = float;

        static constexpr std::size_t N = 1024;
        static constexpr float SR = 22050.f;

        using ChromaCalc = ChromaCalculator<N, FloatType>;
        ChromaCalc calc{ SR };
        SyntheticSpectrum<N, FloatType> spec{ SR };
    };

    using Chroma = std::array<float, 12>;

    static float sum(const Chroma& c)
    {
        return std::accumulate(c.begin(), c.end(), 0.f);
    }

    static float dot(const Chroma& a, const Chroma& b)
    {
        float d{};
        for (std::size_t i{}; i < a.size(); ++i)
            d += a[i] * b[i];
        return d;
    }

    static float norm(const Chroma& c)
    {
        return std::sqrt(dot(c, c));
    }

    static size_t argmax(const Chroma& c)
    {
        return std::distance(c.begin(),
                             std::max_element(c.begin(), c.end()));
    }

    static constexpr float A4Frequency{ 440.F };
    static constexpr float C4Frequency{ 261.63F };
    static constexpr float C5Frequency{ 523.25F };
    static constexpr float E4Frequency{ 329.63F };
    static constexpr float G4Frequency{ 392.F };

    TEST_F(ChromaCalculatorTest, ZeroInput)
    {
        const auto out{ calc.apply(spec.data()) };
        EXPECT_EQ(sum(out), 0.F);
    }

    TEST_F(ChromaCalculatorTest, NearSilenceProducesZeroOutput)
    {
        std::array<FloatType, N> spectrum;
        constexpr FloatType belowSilence{ ChromaCalc::silenceThreshold / 10.F };
        std::fill(spectrum.begin(), spectrum.end(), belowSilence);

        const auto out{ calc.apply(spec.data()) };
        EXPECT_EQ(sum(out), 0.F);
        for (FloatType v : out)
            EXPECT_EQ(v, 0.F);
    }

    TEST_F(ChromaCalculatorTest, EnergyIsNormalized)
    {
        spec.addTone(A4Frequency);

        const auto out{ calc.apply(spec.data()) };
        EXPECT_FLOAT_EQ(sum(out), 1.F);
    }

    TEST_F(ChromaCalculatorTest, OctaveInvarianceCosine)
    {
        spec.addTone(C4Frequency);
        const auto c4{ calc.apply(spec.data()) };

        spec.clear();
        spec.addTone(C5Frequency); // C5
        const auto c5{ calc.apply(spec.data()) };

        const float sim{ dot(c4, c5) / (norm(c4) * norm(c5) + 1e-12F) };
        EXPECT_GT(sim, 0.95F);
    }

    TEST_F(ChromaCalculatorTest, PitchClassStable)
    {
        spec.addTone(C4Frequency);
        const auto a{ argmax(calc.apply(spec.data())) };

        spec.clear();
        spec.addTone(C5Frequency);
        auto b{ argmax(calc.apply(spec.data())) };

        EXPECT_EQ(a, b);
    }

    TEST_F(ChromaCalculatorTest, SingleToneHasCompactDistribution)
    {
        spec.addTone(C4Frequency);
        auto c{ calc.apply(spec.data()) };

        const FloatType maxv{ *std::max_element(c.begin(), c.end()) };
        const FloatType mean{ sum(c) / FloatType{ 12 } };

        EXPECT_GT(maxv, mean * 1.8F);
    }
    TEST_F(ChromaCalculatorTest, ChordHasHigherEntropy)
    {
        spec.addTone(C4Frequency);
        const auto mono{ calc.apply(spec.data()) };

        spec.addTone(E4Frequency); // E4
        spec.addTone(G4Frequency); // G4

        const auto chord{ calc.apply(spec.data()) };
        EXPECT_GT(math::entropy<FloatType>(chord), math::entropy<FloatType>(mono));
    }

    // ------------------------------------------------------------
    // 7. scaling invariance
    // ------------------------------------------------------------
    TEST_F(ChromaCalculatorTest, ScaleInvariance)
    {
        spec.addTone(A4Frequency); // A4

        const auto a{ calc.apply(spec.data()) };

        spec.scale(100.f);

        const auto b{ calc.apply(spec.data()) };

        FloatType diff{};
        for (std::size_t i{}; i < a.size(); ++i)
            diff += std::abs(a[i] - b[i]);

        EXPECT_LT(diff, 1e-3f);
    }
} // namespace lms::audio::features::chromaTests