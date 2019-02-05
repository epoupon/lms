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

static InputVector::value_type
defaultLearningFactor(Network::CurrentIteration iteration)
{
	constexpr InputVector::value_type initialValue = 1;

	return initialValue * exp(-((iteration.idIteration + 1) / static_cast<InputVector::value_type>(iteration.iterationCount)));
}

static InputVector::value_type
euclidianSquareDistance(const InputVector& a, const InputVector& b, const InputVector& weights)
{
	checkSameDimensions(a, b);
	checkSameDimensions(a, weights);

	InputVector::value_type res = 0;

	for (std::size_t i = 0; i < a.size(); ++i)
	{
		res += (a[i] - b[i]) * (a[i] - b[i]) * weights[i];
	}

	return res;
}

static
InputVector::value_type
sigmaFunc(Network::CurrentIteration iteration)
{
	constexpr InputVector::value_type sigma0 = 1;

	return sigma0 * exp(- ((iteration.idIteration + 1) / static_cast<InputVector::value_type>(iteration.iterationCount)));
}

static
InputVector::value_type
defaultNeighbourhoodFunc(InputVector::value_type norm, Network::CurrentIteration iteration)
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
InputVector::value_type
norm(const InputVector& a)
{
	InputVector::value_type res = 0;

	for (const auto& val : a)
	{
		res += val * val;
	}

	return sqrt(res);
}

static
InputVector
operator+(const InputVector& a, const InputVector& b)
{
	checkSameDimensions(a, b);

	InputVector res(a.size(), 0);

	for (std::size_t dimId = 0; dimId < a.size(); ++dimId)
	{
		res[dimId] = a[dimId] + b[dimId];
	}

	return res;
}

static
InputVector
operator-(const InputVector& a, const InputVector& b)
{
	checkSameDimensions(a, b);

	InputVector res(a.size(), 0);

	for (std::size_t dimId = 0; dimId < a.size(); ++dimId)
	{
		res[dimId] = a[dimId] - b[dimId];
	}

	return res;
}

static
InputVector
operator*(const InputVector& a, InputVector::value_type factor)
{
	InputVector res(a.size(), 0);

	for (std::size_t dimId = 0; dimId < a.size(); ++dimId)
	{
		res[dimId] = a[dimId] * factor;
	}

	return res;
}

Network::Network(std::size_t width, std::size_t height, std::size_t inputDimCount)
:
_inputDimCount(inputDimCount),
_weights(inputDimCount, static_cast<InputVector::value_type>(1)),
_refVectors(width, height),
_distanceFunc(euclidianSquareDistance),
_learningFactorFunc(defaultLearningFactor),
_neighbourhoodFunc(defaultNeighbourhoodFunc)
{
	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	// init each vector with a random normalized value
	std::uniform_real_distribution<InputVector::value_type> dist(0, 1);

	for (std::size_t y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (std::size_t x = 0; x < _refVectors.getWidth(); ++x)
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

double
Network::getRefVectorsDistance(Coords coords1, Coords coords2) const
{
	return _distanceFunc(_refVectors.get(coords1), _refVectors.get(coords2), _weights);
}

double
Network::computeRefVectorsDistanceMean() const
{
	std::vector<double> values;
	values.reserve(2*_refVectors.getHeight()*_refVectors.getWidth() - _refVectors.getWidth() - _refVectors.getHeight());
	for (std::size_t y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (std::size_t x = 0; x < _refVectors.getWidth(); ++x)
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
	for (std::size_t y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (std::size_t x = 0; x < _refVectors.getWidth(); ++x)
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

	for (std::size_t y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (std::size_t x = 0; x < _refVectors.getWidth(); ++x)
		{
			os << _refVectors.get({x, y}) << " ";
		}

		os << std::endl;
	}
	os << std::endl;
}

Coords
Network::getClosestRefVectorCoords(const InputVector& data) const
{
	return _refVectors.getCoordsMinElement([&](const auto& a, const auto& b)
			{
				return (_distanceFunc(a, data, _weights) < _distanceFunc(b, data, _weights));
			});
}

boost::optional<Coords>
Network::getClosestRefVectorCoords(const InputVector& data, double maxDistance) const
{
	Coords coords = _refVectors.getCoordsMinElement([&](const auto& a, const auto& b)
			{
				return (_distanceFunc(a, data, _weights) < _distanceFunc(b, data, _weights));
			});

	if (_distanceFunc(data, _refVectors.get(coords), _weights) > maxDistance)
		return boost::none;

	return coords;
}

boost::optional<Coords>
Network::getClosestRefVectorCoords(const std::set<Coords>& refVectorsCoords, double maxDistance) const
{
	std::set<Coords> neighboursCoords;
	for (const Coords& refVectorCoords : refVectorsCoords)
	{
		if (refVectorCoords.y > 0)
			neighboursCoords.insert({ refVectorCoords.x, refVectorCoords.y - 1 });
		if (refVectorCoords.y < _refVectors.getHeight() - 1)
			neighboursCoords.insert({ refVectorCoords.x, refVectorCoords.y + 1 });
		if (refVectorCoords.x > 0)
			neighboursCoords.insert({ refVectorCoords.x - 1, refVectorCoords.y });
		if (refVectorCoords.x < _refVectors.getWidth() - 1)
			neighboursCoords.insert({ refVectorCoords.x + 1, refVectorCoords.y });
	}

	// remove coords that are in the input coords
	for (const auto& refVectorCoords : refVectorsCoords)
		neighboursCoords.erase(refVectorCoords);

	if (neighboursCoords.empty())
		return boost::none;

	// Now compute the distance for each neighbour
	struct NeighbourInfo
	{
		Coords coords;
		double distance;
	};

	std::vector<NeighbourInfo> neighboursInfo;
	for (const Coords& neighbourCoords : neighboursCoords)
	{
		auto min = std::min_element(refVectorsCoords.begin(), refVectorsCoords.end(),
				[this, neighbourCoords](const auto& a, const auto& b)
				{
					return (this->getRefVectorsDistance(a, neighbourCoords) < this->getRefVectorsDistance(b, neighbourCoords));
				});

		double distance = getRefVectorsDistance(neighbourCoords, *min);
		if (distance > maxDistance)
			continue;

		neighboursInfo.push_back({neighbourCoords, distance});
	}

	if (neighboursInfo.empty())
		return boost::none;

	auto min = std::min_element(neighboursInfo.begin(), neighboursInfo.end(),
		[&](const auto& a, const auto& b)
		{
			return a.distance < b.distance;
		});


	return min->coords;
}

static InputVector::value_type
computeCoordsNorm(Coords c1, Coords c2)
{
	std::vector<InputVector::value_type> a = { static_cast<InputVector::value_type>(c1.x), static_cast<InputVector::value_type>(c1.y) };
	std::vector<InputVector::value_type> b = { static_cast<InputVector::value_type>(c2.x), static_cast<InputVector::value_type>(c2.y) };

	return norm(a - b);
}


void
Network::updateRefVectors(Coords closestRefVectorCoords, const InputVector& input, CurrentIteration iteration)
{
	for (std::size_t y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (std::size_t x = 0; x < _refVectors.getWidth(); ++x)
		{
			auto& refVector = _refVectors.get({x, y});

			auto delta = input - refVector;
			auto n = computeCoordsNorm({x, y}, closestRefVectorCoords);

			auto oldRefVector = refVector;
			refVector = refVector + delta * (_learningFactorFunc(iteration) * _neighbourhoodFunc(n, iteration));
		}
	}
}

void
Network::train(const std::vector<InputVector>& inputData, std::size_t nbIterations)
{

	std::vector<const InputVector*> inputDataShuffled;
	inputDataShuffled.reserve(inputData.size());

	for (const auto& input : inputData)
	{
		inputDataShuffled.push_back(&input);
	}

	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	for (std::size_t i = 0; i < nbIterations; ++i)
	{
		std::shuffle(inputDataShuffled.begin(), inputDataShuffled.end(), randGenerator);

		for (auto input : inputDataShuffled)
		{
			Coords closestRefVectorCoords = getClosestRefVectorCoords(*input);

			updateRefVectors(closestRefVectorCoords, *input, {i, nbIterations});
		}
	}
}


} // namespace SOM


