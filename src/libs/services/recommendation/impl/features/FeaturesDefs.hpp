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

#include "math/Vector.hpp"

#include "audio/AudioFeatures.hpp"

namespace lms::recommendation
{
    using FloatType = audio::FeatureValue;
    inline constexpr std::size_t audioFeatureCount{ 4 * audio::AudioFeatures::melBandCount + 4 * audio::AudioFeatures::mfccCount + 11 + 4 * audio::AudioFeatures::chromaCount + 2 };
    inline constexpr std::size_t pcaDimCount{ 80 };

    using AudioFeatureVector = math::Vector<audioFeatureCount, FloatType>;
    using ReducedFeatureVector = math::Vector<pcaDimCount, FloatType>;
} // namespace lms::recommendation