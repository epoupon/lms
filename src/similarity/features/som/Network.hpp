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
#include <set>
#include <ostream>
#include <functional>

#include <boost/optional.hpp>

#include "Matrix.hpp"

#include "utils/Exception.hpp"

namespace SOM
{

using FeatureType = double;
using InputVector = std::vector<FeatureType>;
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

		Network() = default;

		// Init a network with random values
		Network(Coordinate width, Coordinate height, std::size_t inputDimCount);

		// Init a network with serialized values
		Network(const std::string& data);

		std::size_t getWidth() const { return _refVectors.getWidth(); }
		std::size_t getHeight() const { return _refVectors.getHeight(); }
		std::size_t getInputDimCount() const { return _inputDimCount; }
		const InputVector& getDataWeights() const { return _weights; }

		// Set weight for each dimension (default is 1 for each weight)
		void setDataWeights(const InputVector& weights);

		// use this to manually construct a network without training
		void setRefVector(const Position& position, const InputVector& data);

		// <!> data must be normalized
		struct CurrentIteration
		{
			std::size_t idIteration;
			std::size_t iterationCount;
		};
		using ProgressCallback = std::function<void(const CurrentIteration&)>;
		using RequestStopCallback = std::function<bool()>;
		void train(const std::vector<InputVector>& dataSamples, std::size_t nbIterations, ProgressCallback = ProgressCallback{}, RequestStopCallback = RequestStopCallback{});

		const InputVector& getRefVector(const Position& position) const;
		Position getClosestRefVectorPosition(const InputVector& data) const;
		boost::optional<Position> getClosestRefVectorPosition(const InputVector& data, double maxDistance) const;

		boost::optional<Position> getClosestRefVectorPosition(const std::set<Position>& refVectorsPosition, double maxDistance) const;

		double getRefVectorsDistance(const Position& position1, const Position& position2) const;

		double computeRefVectorsDistanceMean() const;
		double computeRefVectorsDistanceMedian() const;

		void dump(std::ostream& os) const;

		// For each ref vector, update formula is:
		// i is the current iteration
		// refVector(i+1) = refVector(i) + LearningFactor(i) * NeighbourhoodFunc(i) * (MatchingRefVector - refVector)

		using DistanceFunc = std::function<FeatureType(const InputVector& /* a */, const InputVector& /* b */, const InputVector& /* weights */)>;
		void setDistanceFunc(DistanceFunc distanceFunc);

		using LearningFactorFunc = std::function<FeatureType(const CurrentIteration&)>;
		void setLearningFactorFunc(LearningFactorFunc learningFactorFunc);

		using NeighbourhoodFunc = std::function<FeatureType(FeatureType /* norm(Position - CoordMatchingRefVector) */, const CurrentIteration&)>;
		void setNeighbourhoodFunc(NeighbourhoodFunc neighbourhoodFunc);

	private:

		void updateRefVectors(const Position& closestRefVectorPosition, const InputVector& input, FeatureType learningFactor, const CurrentIteration& iteration);

		std::size_t _inputDimCount = 0;
		InputVector _weights;	// weight for each dimension
		Matrix<InputVector> _refVectors;

		DistanceFunc _distanceFunc;
		LearningFactorFunc _learningFactorFunc;
		NeighbourhoodFunc _neighbourhoodFunc;
};

} // namespace SOM
