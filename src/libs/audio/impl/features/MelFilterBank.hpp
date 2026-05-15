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

#include <span>
#include <vector>

namespace lms::audio::features
{
    float freqToMel(float freq);
    float melToFreq(float mel);

    struct MelFilterBank
    {
        struct Filter
        {
            std::vector<float> weights; // normalized so that the sum of weights equals 1.0
            std::size_t leftBinIndex;   // index of the leftmost FFT bin covered by the filter
        };

        // binCount is the number of FFT bins (nfft/2 + 1) that the filters can cover
        MelFilterBank(std::vector<Filter>&& _filters, std::size_t binCount);

        const Filter& getFilter(std::size_t m) const;
        std::size_t getFilterCount() const;
        std::size_t getBinCount() const;

        float computeEnergy(std::size_t m, std::span<const float> input) const;

    private:
        const std::vector<Filter> _filters;
        const std::size_t _binCount;
    };

    // Each filter covers a range of FFT bins and is normalized so that the sum of its weights equals 1.0
    // Each filter stores only its non-zero triangular region (sparse representation)
    // fMin/fMax: frequency range in Hz. Defaults (0.f, 0.f) span from 0 to Nyquist.
    MelFilterBank computeMelFilterBank(size_t nfft, size_t sampleRate, size_t filterCount, float fMin = 0.F, float fMax = 0.F);
} // namespace lms::audio::features