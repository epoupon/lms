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

#include "som/Network.hpp"
#include "som/Vector.hpp"

#include "audio/AudioFeatures.hpp"

namespace lms::recommendation
{
    inline constexpr std::size_t featureCount{ 4 * audio::AudioFeatures::melBandCount };
    using FloatType = audio::FeatureValue;

    using AudioSomInput = som::Vector<featureCount, FloatType>;
    using SOM = som::Network<featureCount, FloatType>;
} // namespace lms::recommendation