/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <vector>
#include <ostream>
#include <functional>

#include "Matrix.hpp"

#include "utils/Exception.hpp"

namespace SOM
{

using InputVector = std::vector<double>;
void checkSameDimensions(const InputVector& a, const InputVector& b);
void checkSameDimensions(const InputVector& a, std::size_t inputDimCount);
std::ostream& operator<<(std::ostream& os, const InputVector& a);

class SOMException : public LmsException
{
	public:
		SOMException(const std::string& msg) : LmsException(msg) {}
};


class Network
{
	public:

		// Init a network with random values
		Network(std::size_t width, std::size_t height, std::size_t inputDimCount);

		// Init a network with serialized values
		Network(const std::string& data);

		std::size_t getWidth() const { return _refVectors.getWidth(); }
		std::size_t getHeight() const { return _refVectors.getHeight(); }
		std::size_t getInputDimCount() const {return _inputDimCount;}
		// Set weight for each dimension (default is 1 for each weight)
		void setDataWeights(const InputVector& weights);

		// data must be normalized
		void train(const std::vector<InputVector>& dataSamples, std::size_t nbIterations);

		// data must be normalized
		Coords classify(const InputVector& data) const;

		// ordered from closest to farthest
		std::vector<Coords> classify(const InputVector& data, std::size_t size) const;

		void dump(std::ostream& os) const;

		// For each ref vector, update formula is:
		// i is the current iteration
		// refVector(i+1) = refVector(i) + LearningFactor(i) * NeighborhoodFunc(i) * (MatchingRefVector - refVector)

		using DistanceFunc = std::function<InputVector::value_type(const InputVector& /* a */, const InputVector& /* b */, const InputVector& /* weights */)>;
		void setDistanceFunc(DistanceFunc distanceFunc);

		struct Progress
		{
			std::size_t idIteration;
			std::size_t iterationCount;
		};

		using LearningFactorFunc = std::function<InputVector::value_type(Progress)>;
		void setLearningFactorFunc(LearningFactorFunc learningFactorFunc);

		using NeighborhoodFunc = std::function<InputVector::value_type(InputVector::value_type /* norm(Coords - CoordMatchingRefVector) */, Progress)>;
		void setNeighborhoodFunc(NeighborhoodFunc neighborhoodFunc);

		std::string serializeTo() const;

	private:

		void serializeFrom(const std::string& data);

		Coords getClosestRefVector(const InputVector& data) const;

		void updateRefVectors(Coords closestRefVectorCoords, const InputVector& input, Progress progress);

		std::size_t _inputDimCount;
		InputVector _weights;	// weight for each dimension
		Matrix<InputVector> _refVectors;

		DistanceFunc _distanceFunc;
		LearningFactorFunc _learningFactorFunc;
		NeighborhoodFunc _neighborhoodFunc;
};

} // namespace SOM
