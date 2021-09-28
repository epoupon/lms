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

#include "som/Network.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <sstream>
#include <unordered_set>

#include "utils/Logger.hpp"
#include "utils/Random.hpp"

namespace SOM
{

void
checkSameDimensions(const InputVector& a, const InputVector& b)
{
	if (!a.hasSameDimension(b))
		throw Exception("Bad data dimension count");
}

void
checkSameDimensions(const InputVector& a, std::size_t inputDimCount)
{
	if (a.getNbDimensions() != inputDimCount)
		throw Exception("Bad data dimension count");
}

static LearningFactor
defaultLearningFactor(Network::CurrentIteration iteration)
{
	static const LearningFactor initialValue{1};

	return initialValue * exp(-((iteration.idIteration + 1) / static_cast<LearningFactor>(iteration.iterationCount)));
}

static InputVector::Distance
euclidianSquareDistance(const InputVector& a, const InputVector& b, const InputVector& weights)
{
	return a.computeEuclidianSquareDistance(b, weights);
}

static
InputVector::value_type
sigmaFunc(Network::CurrentIteration iteration)
{
	constexpr InputVector::value_type sigma0 {1};

	return sigma0 * std::exp(- ((iteration.idIteration + 1) / static_cast<InputVector::value_type>(iteration.iterationCount)));
}

static
InputVector::value_type
defaultNeighbourhoodFunc(Norm norm, const Network::CurrentIteration& iteration)
{
	InputVector::value_type sigma {sigmaFunc(iteration)};

	return exp(-norm / (2 * sigma * sigma));
}

Network::Network(Coordinate width, Coordinate height, std::size_t inputDimCount)
:
_inputDimCount {inputDimCount},
_weights {inputDimCount, static_cast<InputVector::value_type>(1)},
_refVectors {width, height, _inputDimCount},
_distanceFunc {euclidianSquareDistance},
_learningFactorFunc {defaultLearningFactor},
_neighbourhoodFunc {defaultNeighbourhoodFunc}
{
	// init each vector with a random normalized value
	for (Coordinate y {}; y < _refVectors.getHeight(); ++y)
	{
		for (Coordinate x {}; x < _refVectors.getWidth(); ++x)
		{
			for (InputVector::value_type& val : _refVectors.get({x,y}))
				val = Random::getRealRandom<InputVector::value_type>(0, 1);
		}
	}
}

void
Network::setDataWeights(const InputVector& weights)
{
	checkSameDimensions(weights, _inputDimCount);

	_weights = weights;
}

void
Network::setRefVector(const Position& position, const InputVector& data)
{
	checkSameDimensions(data, _inputDimCount);

	_refVectors[position] = data;
}

InputVector::Distance
Network::getRefVectorsDistance(const Position& position1, const Position& position2) const
{
	return _distanceFunc(_refVectors.get(position1), _refVectors.get(position2), _weights);
}

InputVector::Distance
Network::computeRefVectorsDistanceMean() const
{
	std::vector<InputVector::Distance> values;
	values.reserve(2 * _refVectors.getHeight()*_refVectors.getWidth() - _refVectors.getWidth() - _refVectors.getHeight());
	for (Coordinate y {}; y < _refVectors.getHeight(); ++y)
	{
		for (Coordinate x {}; x < _refVectors.getWidth(); ++x)
		{
			if (x != _refVectors.getWidth() - 1)
				values.emplace_back(getRefVectorsDistance( {x, y}, {x + 1, y}));
			if (y != _refVectors.getHeight() - 1)
				values.emplace_back(getRefVectorsDistance( {x, y}, {x, y + 1}));
		}
	}

	return std::accumulate(values.begin(), values.end(), 0.) / values.size();
}

double
Network::computeRefVectorsDistanceMedian() const
{
	std::vector<InputVector::Distance> values;
	values.reserve(2*_refVectors.getHeight()*_refVectors.getWidth() - _refVectors.getWidth() - _refVectors.getHeight());
	for (Coordinate y {}; y < _refVectors.getHeight(); ++y)
	{
		for (Coordinate x {}; x < _refVectors.getWidth(); ++x)
		{
			if (x != _refVectors.getWidth() - 1)
				values.emplace_back(getRefVectorsDistance( {x, y}, {x + 1, y}));
			if (y != _refVectors.getHeight() - 1)
				values.emplace_back(getRefVectorsDistance( {x, y}, {x, y + 1}));
		}
	}

	std::sort(values.begin(), values.end());

	return values[values.size() > 1 ? values.size()/2 - 1 : 0];
}

void
Network::dump(std::ostream& os) const
{
	os << "Width: " << _refVectors.getWidth() << ", Height: " << _refVectors.getHeight() << std::endl;;

	for (Coordinate y {}; y < _refVectors.getHeight(); ++y)
	{
		for (Coordinate x {}; x < _refVectors.getWidth(); ++x)
		{
			os << _refVectors.get({x, y}) << " ";
		}

		os << std::endl;
	}
	os << std::endl;
}

Position
Network::getClosestRefVectorPosition(const InputVector& data) const
{
	return _refVectors.getPositionMinElement([&](const auto& a, const auto& b)
			{
				return (_distanceFunc(a, data, _weights) < _distanceFunc(b, data, _weights));
			});
}

std::optional<Position>
Network::getClosestRefVectorPosition(const InputVector& data, InputVector::Distance maxDistance) const
{
	std::optional<Position> position {getClosestRefVectorPosition(data)};

	if (_distanceFunc(data, _refVectors.get(*position), _weights) > maxDistance)
		position.reset();

	return position;
}

std::optional<Position>
Network::getClosestRefVectorPosition(const std::vector<Position>& refVectorsPosition, InputVector::Distance maxDistance) const
{
	std::unordered_set<Position> neighboursPosition;
	for (const Position& refVectorPosition : refVectorsPosition)
	{
		if (refVectorPosition.y > 0)
			neighboursPosition.insert({ refVectorPosition.x, refVectorPosition.y - 1 });
		if (refVectorPosition.y < _refVectors.getHeight() - 1)
			neighboursPosition.insert({ refVectorPosition.x, refVectorPosition.y + 1 });
		if (refVectorPosition.x > 0)
			neighboursPosition.insert({ refVectorPosition.x - 1, refVectorPosition.y });
		if (refVectorPosition.x < _refVectors.getWidth() - 1)
			neighboursPosition.insert({ refVectorPosition.x + 1, refVectorPosition.y });
	}

	// remove position that are in the input position
	for (const auto& refVectorPosition : refVectorsPosition)
		neighboursPosition.erase(refVectorPosition);

	if (neighboursPosition.empty())
		return std::nullopt;

	// Now compute the distance for each neighbour
	struct NeighbourInfo
	{
		Position position;
		double distance;
	};

	std::vector<NeighbourInfo> neighboursInfo;
	for (const Position& neighbourPosition : neighboursPosition)
	{
		auto min = std::min_element(refVectorsPosition.begin(), refVectorsPosition.end(),
				[this, neighbourPosition](const auto& a, const auto& b)
				{
					return (this->getRefVectorsDistance(a, neighbourPosition) < this->getRefVectorsDistance(b, neighbourPosition));
				});

		InputVector::Distance distance {getRefVectorsDistance(neighbourPosition, *min)};
		if (distance > maxDistance)
			continue;

		neighboursInfo.emplace_back(NeighbourInfo {neighbourPosition, distance});
	}

	if (neighboursInfo.empty())
		return std::nullopt;

	auto min {std::min_element(std::cbegin(neighboursInfo), std::cend(neighboursInfo),
		[&](const auto& a, const auto& b)
		{
			return a.distance < b.distance;
		})};


	return min->position;
}

static Norm
computePositionNorm(const Position& c1, const Position& c2)
{
	return std::sqrt((c1.x - c2.x) * (c1.x - c2.x) + (c1.y - c2.y) * (c1.y - c2.y));
}

void
Network::updateRefVectors(const Position& closestRefVectorPosition, const InputVector& input, LearningFactor learningFactor, const CurrentIteration& iteration)
{
	for (Coordinate y {}; y < _refVectors.getHeight(); ++y)
	{
		for (Coordinate x {}; x < _refVectors.getWidth(); ++x)
		{
			InputVector& refVector {_refVectors.get({x, y})};

			const Norm norm {computePositionNorm({x, y}, closestRefVectorPosition)};

			InputVector delta {input - refVector};
			delta *= (learningFactor * _neighbourhoodFunc(norm, iteration));

			refVector += delta;
		}
	}
}

void
Network::train(const std::vector<InputVector>& inputData, std::size_t nbIterations, ProgressCallback progressCallback, RequestStopCallback requestStopCallback)
{
	bool stopRequested {false};
	std::vector<const InputVector*> inputDataShuffled;

	inputDataShuffled.reserve(inputData.size());
	for (const auto& input : inputData)
		inputDataShuffled.push_back(&input);

	for (std::size_t i {}; i < nbIterations; ++i)
	{
		CurrentIteration curIter {i, nbIterations};

		if (progressCallback)
			progressCallback(curIter);

		Random::shuffleContainer(inputDataShuffled);

		const LearningFactor learningFactor {_learningFactorFunc(curIter)};

		for (const InputVector* input : inputDataShuffled)
		{
			if (requestStopCallback)
				stopRequested = requestStopCallback();

			if (stopRequested)
				return;

			updateRefVectors(getClosestRefVectorPosition(*input), *input, learningFactor, curIter);
		}

		if (stopRequested)
			return;
	}
}

const InputVector&
Network::getRefVector(const Position& position) const
{
	return _refVectors[position];
}


} // namespace SOM


