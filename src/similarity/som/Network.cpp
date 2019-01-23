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
defaultLearningFactor(Network::Progress progress)
{
	constexpr InputVector::value_type initialValue = 1;

	return initialValue * exp(-((progress.idIteration + 1) / static_cast<InputVector::value_type>(progress.iterationCount)));
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
sigmaFunc(Network::Progress progress)
{
	constexpr InputVector::value_type sigma0 = 1;

	return sigma0 * exp(- ((progress.idIteration + 1) / static_cast<InputVector::value_type>(progress.iterationCount)));
}

static
InputVector::value_type
defaultNeighborhoodFunc(InputVector::value_type norm, Network::Progress progress)
{
	auto sigma = sigmaFunc(progress);

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
_neighborhoodFunc(defaultNeighborhoodFunc)
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

Network::Network(const std::string& data)
: _refVectors(0, 0),
_distanceFunc(euclidianSquareDistance),
_learningFactorFunc(defaultLearningFactor),
_neighborhoodFunc(defaultNeighborhoodFunc)
{
	serializeFrom(data);
}

void
Network::setDataWeights(const InputVector& weights)
{
	checkSameDimensions(weights, _inputDimCount);

	_weights = weights;
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
Network::getClosestRefVector(const InputVector& data) const
{
	return _refVectors.getCoordsMinElement([&](const auto& a, const auto& b)
			{
				return (_distanceFunc(a, data, _weights) < _distanceFunc(b, data, _weights));
			});
}

Coords
Network::classify(const InputVector& data) const
{
	return getClosestRefVector(data);
}

std::vector<Coords>
Network::classify(const InputVector& data, std::size_t size) const
{
	struct Entry
	{
		Coords coords;
		InputVector refVector;
	};
	std::vector<Entry> sortedEntries;

	for (std::size_t x = 0; x < _refVectors.getWidth(); ++x)
	{
		for (std::size_t y = 0; y < _refVectors.getHeight(); ++y)
		{
			sortedEntries.push_back( Entry{{x, y}, _refVectors.get({x, y})} );
		}
	}

	const InputVector& closestRefVector = _refVectors.get(getClosestRefVector(data));

	std::sort(sortedEntries.begin(), sortedEntries.end(),
		[&](const Entry& a, const Entry& b)
		{
			return _distanceFunc(a.refVector, closestRefVector, _weights) < _distanceFunc(b.refVector, closestRefVector, _weights);
		});

	std::vector<Coords> res;
	for (const Entry& entry : sortedEntries)
	{
		res.push_back(entry.coords);

		if (res.size() == size)
			break;
	}

	return res;
}

static InputVector::value_type
computeCoordsNorm(Coords c1, Coords c2)
{
	std::vector<InputVector::value_type> a = { static_cast<InputVector::value_type>(c1.x), static_cast<InputVector::value_type>(c1.y) };
	std::vector<InputVector::value_type> b = { static_cast<InputVector::value_type>(c2.x), static_cast<InputVector::value_type>(c2.y) };

	return norm(a - b);
}


void
Network::updateRefVectors(Coords closestRefVectorCoords, const InputVector& input, Progress progress)
{
	for (std::size_t y = 0; y < _refVectors.getHeight(); ++y)
	{
		for (std::size_t x = 0; x < _refVectors.getWidth(); ++x)
		{
			auto& refVector = _refVectors.get({x, y});

			auto delta = input - refVector;
			auto n = computeCoordsNorm({x, y}, closestRefVectorCoords);

			auto oldRefVector = refVector;
			refVector = refVector + delta * (_learningFactorFunc(progress) * _neighborhoodFunc(n, progress));
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
			Coords closestRefVectorCoords = getClosestRefVector(*input);

			updateRefVectors(closestRefVectorCoords, *input, {i, nbIterations});
		}
	}
}

std::string
Network::serializeTo() const
{
	std::ostringstream oss;

	oss << _inputDimCount << " ";

	for (auto weight : _weights)
		oss << weight << " ";

	// Matrix
	oss << _refVectors.getWidth() << " " << _refVectors.getHeight() << " ";
	for (std::size_t x = 0; x < _refVectors.getWidth(); ++x)
	{
		for (std::size_t y = 0; y < _refVectors.getHeight(); ++y)
		{
			for (auto val : _refVectors.get({x,y}))
				oss << val << " ";
		}
	}

	return oss.str();
}

void
Network::serializeFrom(const std::string& data)
{
	std::istringstream iss(data);

	LMS_LOG(SIMILARITY, DEBUG) << "data = '" << data << "'";
	iss >> _inputDimCount;
	LMS_LOG(SIMILARITY, DEBUG) << "Input dim count = " << _inputDimCount;

	for (std::size_t i = 0; i < _inputDimCount; ++i)
	{
		InputVector::value_type val;
		iss >> val;
		_weights.push_back(val);
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Reading matrix...";
	std::size_t width, height;
	iss >> width >> height;
	_refVectors = Matrix<SOM::InputVector>(width, height);

	for (std::size_t x = 0; x < _refVectors.getWidth(); ++x)
	{
		for (std::size_t y = 0; y < _refVectors.getHeight(); ++y)
		{
			InputVector refVector;
			refVector.reserve(_inputDimCount);
			for (std::size_t i = 0; i < _inputDimCount; ++i)
			{
				InputVector::value_type val;
				iss >> val;
				refVector.push_back(val);
			}
			_refVectors.get({x, y}) = refVector;
		}
	}
}

} // namespace SOM


