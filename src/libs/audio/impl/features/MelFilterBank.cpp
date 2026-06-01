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

#include "MelFilterBank.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

#include "audio/Exception.hpp"

namespace lms::audio::features
{
    float freqToMel(float freq)
    {
        return 2595.F * std::log10(1.0F + freq / 700.F);
    }

    float melToFreq(float mel)
    {
        return 700.F * (std::pow(10.F, mel / 2595.F) - 1.F);
    }

    MelFilterBank::MelFilterBank(std::vector<Filter>&& _filters, std::size_t binCount)
        : _filters{ std::move(_filters) }
        , _binCount{ binCount }
    {
    }

    const MelFilterBank::Filter& MelFilterBank::getFilter(std::size_t m) const
    {
        return _filters.at(m);
    }

    std::size_t MelFilterBank::getFilterCount() const
    {
        return _filters.size();
    }

    std::size_t MelFilterBank::getBinCount() const
    {
        return _binCount;
    }

    float MelFilterBank::computeEnergy(std::size_t m, std::span<const float> input) const
    {
        if (input.size() != _binCount)
            throw Exception{ "Input size must be equal to the number of bins" };

        const auto& filter{ _filters.at(m) };
        assert(filter.leftBinIndex + filter.weights.size() <= input.size());

        float energy{};
        std::size_t bin{ filter.leftBinIndex };
        for (std::size_t i{}, n = filter.weights.size(); i < n; ++i, ++bin)
            energy += input[bin] * filter.weights[i];

        return energy;
    }

    MelFilterBank computeMelFilterBank(std::size_t nfft, std::size_t sampleRate, std::size_t filterCount, float fMin, float fMax)
    {
        const float nyquist{ sampleRate / 2.F };
        const float effectiveFMin{ (fMin <= 0.F) ? 0.F : fMin };
        const float effectiveFMax{ (fMax <= 0.F || fMax > nyquist) ? nyquist : fMax };
        const float melMin{ freqToMel(effectiveFMin) };
        const float melMax{ freqToMel(effectiveFMax) };

        // 1. mel points
        std::vector<float> melPoints(filterCount + 2);
        for (std::size_t i{}; i < melPoints.size(); ++i)
            melPoints[i] = melMin + i * (melMax - melMin) / (filterCount + 1);

        // 2. mel -> Hz
        std::vector<float> freqs(filterCount + 2);
        std::transform(melPoints.begin(), melPoints.end(), freqs.begin(), melToFreq);

        // 3. Hz -> bins
        std::vector<size_t> bins(filterCount + 2);
        for (std::size_t i{}; i < bins.size(); ++i)
            bins[i] = static_cast<size_t>(std::floor(nfft * freqs[i] / sampleRate));

        // 4. fix duplicates
        for (std::size_t i{ 1 }; i < bins.size(); ++i)
        {
            if (bins[i] <= bins[i - 1])
                bins[i] = bins[i - 1] + 1;
        }

        // 5. build filters
        const std::size_t binCount{ nfft / 2 + 1 };
        std::vector<MelFilterBank::Filter> filters{ filterCount };

        for (std::size_t m{}; m < filterCount; ++m)
        {
            const std::size_t left{ bins[m] };
            const std::size_t center{ bins[m + 1] };
            const std::size_t right{ bins[m + 2] };

            std::vector<float> filterWeights;
            filterWeights.reserve(right - left);

            // rising edge of the triangle
            for (std::size_t k{ left }; k < center; ++k)
                filterWeights.push_back(float(k - left) / (center - left));

            // falling edge of the triangle
            for (std::size_t k{ center }; k < right; ++k)
                filterWeights.push_back(float(right - k) / (right - center));

            assert(filterWeights.size() == right - left);

            // normalize the filter to sum = 1
            const float sum{ std::accumulate(filterWeights.begin(), filterWeights.end(), 0.F) };
            if (sum > 0.F)
            {
                for (float& weight : filterWeights)
                    weight /= sum;
            }

            filters[m] = MelFilterBank::Filter{ std::move(filterWeights), left };
        }

        return MelFilterBank{ std::move(filters), binCount };
    }
} // namespace lms::audio::features