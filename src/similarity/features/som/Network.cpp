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

#include "Network.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <sstream>

#include "utils/Logger.hpp"

namespace SOM
{

void
checkSameDimensions(const InputVector& a, const InputVector& b)
{
	if (a.size() != b.size())
		throw SOMException("Bad data dimension count");
}

void
checkSameDimensions(const InputVector& a, std::size_t inputDimCount)
{
	if (a.size() != inputDimCount)
		throw SOMException("Bad data dimension count");
}

static FeatureType
defaultLearningFactor(Network::CurrentIteration iteration)
{
	constexpr FeatureType initialValue = 1;

	return initialValue * exp(-((iteration.idIteration + 1) / static_cast<FeatureType>(iteration.iterationCount)));
}

static FeatureType
euclidianSquareDistance(const InputVector& a, const InputVector& b, const InputVector& weights)
{
	checkSameDimensions(a, b);
	checkSameDimensions(a, weights);

	FeatureType res = 0;

	for (std::size_t i = 0; i < a.size(); ++i)
	{
		res += (a[i] - b[i]) * (a[i] - b[i]) * weights[i];
	}

	return res;
}

static
FeatureType
sigmaFunc(Network::CurrentIteration iteration)
{
	constexpr FeatureType sigma0 = 1;

	return sigma0 * std::exp(- ((iteration.idIteration + 1) / static_cast<FeatureType>(iteration.iterationCount)));
}

static
FeatureType
defaultNeighbourhoodFunc(FeatureType norm, Network::CurrentIteration iteration)
{
	auto sigma = sigmaFunc(iteration);

	return exp(-norm / (2 * sigma * sigma));
}


std::ostream&
operator<<(std::ostream& os, const InputVector& a)
{
	os << "[";
	for (const auto& val : a)
	{
		os << val << " ";
	}
	os << "]";

	return os;
}


static
FeatureType
norm(const InputVector& a)
{
	FeatureType res = 0;

	for (auto val : a)
		res += val * val;

	return std::sqrt(res);
}

static
InputVector
operator+(const InputVector& a, const InputVector& b)
{
	checkSameDimensions(a, b);

	InputVector res;
	res.reserve(a.size());

	for (std::size_t dimId = 0; dimId < a.size(); ++dimId)
		res.push_back(a[dimId] + b[dimId]);

	return res;
}

static
InputVector
operator-(const InputVector& a, const InputVector& b)
{
	checkSameDimensions(a, b);

	InputVector res;
	res.reserve(a.size());

	for (std::size_t dimId = 0; dimId < a.size(); ++dimId)
		res.push_back(a[dimId] - b[dimId]);

	return res;
}

static
InputVector
operator*(const InputVector& a, FeatureType factor)
{
	InputVector res;
	res.reserve(a.size());

	for (std::size_t dimId = 0; dimId < a.size(); ++dimId)
		res.push_back(a[dimId] * factor);

	return res;
}

Network::Network(Coordinate width, Coordinate height, std::size_t inputDimCount)
:
_inputDimCount(inputDimCount),
_weights(inputDimCount, static_cast<FeatureType>(1)),
_refVectors(width, height),
_distanceFunc(euclidianSquareDistance),
_learningFactorFunc(defaultLearningFactor),
_neighbourhoodFunc(defaultNeighbourhoodFunc)
{
	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	// init each vector with a random normalized value
	std::uniform_real_distribution<FeatureType> dist(0, 1);

	for (Coordinate y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (Coordinate x = 0; x < _refVectors.getWidth(); ++x)
		{
			auto& refVector = _refVectors.get({x,y});
			refVector.resize(_inputDimCount);
			for (auto& val : refVector)
				val = dist(randGenerator);
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

double
Network::getRefVectorsDistance(const Position& position1, const Position& position2) const
{
	return _distanceFunc(_refVectors.get(position1), _refVectors.get(position2), _weights);
}

double
Network::computeRefVectorsDistanceMean() const
{
	std::vector<double> values;
	values.reserve(2*_refVectors.getHeight()*_refVectors.getWidth() - _refVectors.getWidth() - _refVectors.getHeight());
	for (Coordinate y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (Coordinate x = 0; x < _refVectors.getWidth(); ++x)
		{
			if (x != _refVectors.getWidth() - 1)
				values.push_back(getRefVectorsDistance( {x, y}, {x + 1, y}));
			if (y != _refVectors.getHeight() - 1)
				values.push_back(getRefVectorsDistance( {x, y}, {x, y + 1}));
		}
	}

	return std::accumulate(values.begin(), values.end(), 0.) / values.size();
}

double
Network::computeRefVectorsDistanceMedian() const
{
	std::vector<double> values;
	values.reserve(2*_refVectors.getHeight()*_refVectors.getWidth() - _refVectors.getWidth() - _refVectors.getHeight());
	for (Coordinate y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (Coordinate x = 0; x < _refVectors.getWidth(); ++x)
		{
			if (x != _refVectors.getWidth() - 1)
				values.push_back(getRefVectorsDistance( {x, y}, {x + 1, y}));
			if (y != _refVectors.getHeight() - 1)
				values.push_back(getRefVectorsDistance( {x, y}, {x, y + 1}));
		}
	}

	return values[values.size()/2 - 1];
}

void
Network::dump(std::ostream& os) const
{
	os << "Width: " << _refVectors.getWidth() << ", Height: " << _refVectors.getHeight() << std::endl;;

	for (Coordinate y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (Coordinate x = 0; x < _refVectors.getWidth(); ++x)
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

boost::optional<Position>
Network::getClosestRefVectorPosition(const InputVector& data, double maxDistance) const
{
	Position position = _refVectors.getPositionMinElement([&](const auto& a, const auto& b)
			{
				return (_distanceFunc(a, data, _weights) < _distanceFunc(b, data, _weights));
			});

	if (_distanceFunc(data, _refVectors.get(position), _weights) > maxDistance)
		return boost::none;

	return position;
}

boost::optional<Position>
Network::getClosestRefVectorPosition(const std::set<Position>& refVectorsPosition, double maxDistance) const
{
	std::set<Position> neighboursPosition;
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
		return boost::none;

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

		double distance = getRefVectorsDistance(neighbourPosition, *min);
		if (distance > maxDistance)
			continue;

		neighboursInfo.push_back({neighbourPosition, distance});
	}

	if (neighboursInfo.empty())
		return boost::none;

	auto min = std::min_element(neighboursInfo.begin(), neighboursInfo.end(),
		[&](const auto& a, const auto& b)
		{
			return a.distance < b.distance;
		});


	return min->position;
}

static FeatureType
computePositionNorm(Position c1, Position c2)
{
	std::vector<FeatureType> a { static_cast<FeatureType>(c1.x), static_cast<FeatureType>(c1.y) };
	std::vector<FeatureType> b { static_cast<FeatureType>(c2.x), static_cast<FeatureType>(c2.y) };

	return norm(a - b);
}


void
Network::updateRefVectors(const Position& closestRefVectorPosition, const InputVector& input, FeatureType learningFactor, const CurrentIteration& iteration)
{
	for (Coordinate y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (Coordinate x = 0; x < _refVectors.getWidth(); ++x)
		{
			auto& refVector = _refVectors.get({x, y});

			auto delta = input - refVector;
			auto n = computePositionNorm({x, y}, closestRefVectorPosition);

			refVector = refVector + delta * (learningFactor * _neighbourhoodFunc(n, iteration));
		}
	}
}

void
Network::train(const std::vector<InputVector>& inputData, std::size_t nbIterations, ProgressCallback progressCallback, RequestStopCallback requestStopCallback)
{
	bool stopRequested{false};
	std::vector<const InputVector*> inputDataShuffled;
	inputDataShuffled.reserve(inputData.size());

	for (const auto& input : inputData)
		inputDataShuffled.push_back(&input);

	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	for (std::size_t i = 0; i < nbIterations; ++i)
	{
		CurrentIteration curIter{i, nbIterations};

		if (progressCallback)
			progressCallback(curIter);

		std::shuffle(inputDataShuffled.begin(), inputDataShuffled.end(), randGenerator);

		const auto learningFactor = _learningFactorFunc(curIter);

		for (const InputVector* input : inputDataShuffled)
		{
			if (requestStopCallback)
			{
				stopRequested = requestStopCallback();
				break;
			}

			updateRefVectors(getClosestRefVectorPosition(*input), *input, learningFactor, curIter);
		}

		if (stopRequested)
			break;
	}
}

const InputVector&
Network::getRefVector(const Position& position) const
{
	return _refVectors[position];
}


} // namespace SOM


