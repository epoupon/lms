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

#include <cmath>
#include "SOM.hpp"
#include "DataNormalizer.hpp"

/*
 * For each InputVector, associate vector<T> values
 */
template<typename T>
class Clusterer
{
	public:
		using SampleType = std::pair<SOM::InputVector /* key */, T /* value*/ >;
		using Cluster = std::vector<T>;

		Clusterer(const std::vector<SampleType>& samples, std::size_t inputDimCount, std::size_t iterationCount);

		const Cluster& getCluster(const SOM::InputVector& data) const;

		// Sorted results (best first)
		std::vector<Cluster> getClusters(const SOM::InputVector& data, std::size_t nbClusters) const;

		const std::vector<Cluster>& getAllClusters() const;

		void dump(std::ostream& os) const;

	private:

		void train(const std::vector<std::pair<SOM::InputVector, T>>& samples, std::size_t iterationCount);

		std::vector<T>& getValues(SOM::Coords coords);
		const std::vector<T>& getValues(SOM::Coords coords) const;

		std::size_t _width;
		std::size_t _height;
		std::vector<std::vector<T>> _values; // Map of T vectors
		SOM::DataNormalizer _dataNormalizer;
		SOM::Network _network;
};


template<typename T>
Clusterer<T>::Clusterer(const std::vector<SampleType>& samples, std::size_t inputDimCount, std::size_t iterationCount)
:
_width(std::sqrt(samples.size()/20)),
_height(std::sqrt(samples.size()/20)),
_dataNormalizer(inputDimCount),
_network(_width, _height, inputDimCount)
{
	_values.resize(_width * _height);
	train(samples, iterationCount);
}

template<typename T>
std::vector<T>&
Clusterer<T>::getValues(SOM::Coords coords)
{
	return _values[ coords.x + coords.y*_width ];
}

template<typename T>
const std::vector<T>&
Clusterer<T>::getValues(SOM::Coords coords) const
{
	return _values[ coords.x + coords.y*_width ];
}


template<typename T>
void
Clusterer<T>::train(const std::vector<std::pair<SOM::InputVector, T>>& samples, std::size_t iterationCount)
{
	// Train
	{
		std::vector<SOM::InputVector> inputVectors;
		inputVectors.reserve(samples.size());

		for (const auto& sample : samples)
		{
			inputVectors.push_back(sample.first);
		}

		_dataNormalizer.computeNormalizationFactors(inputVectors);

		for (auto& inputVector : inputVectors)
			_dataNormalizer.normalizeData(inputVector);

		_network.train(inputVectors, iterationCount);
	}

	// Classify data
	for (const auto& sample : samples)
	{
		auto inputVector = sample.first;
		const auto& value = sample.second;

		_dataNormalizer.normalizeData(inputVector);
		auto coords = _network.classify(inputVector);
		auto& values = getValues(coords);

		values.push_back(value);
	}
}

template<typename T>
const typename Clusterer<T>::Cluster&
Clusterer<T>::getCluster(const SOM::InputVector& inputVector) const
{
	auto inputVectorNormalized = inputVector;
	_dataNormalizer.normalizeData(inputVectorNormalized);

	return getValues(_network.classify(inputVectorNormalized));
}

template<typename T>
std::vector<typename Clusterer<T>::Cluster>
Clusterer<T>::getClusters(const SOM::InputVector& inputVector, std::size_t nbClusters) const
{
	auto inputVectorNormalized = inputVector;
	_dataNormalizer.normalizeData(inputVectorNormalized);

	std::vector<typename Clusterer<T>::Cluster> res;
	for (auto& cluster : _network.classify(inputVectorNormalized, nbClusters))
	{
		res.push_back(getValues(cluster));
	}

	return res;
}

template<typename T>
const std::vector<typename Clusterer<T>::Cluster>&
Clusterer<T>::getAllClusters() const
{
	return _values;
}

template<typename T>
void
Clusterer<T>::dump(std::ostream& os) const
{
	os << "Normalizer:" << std::endl;
	_dataNormalizer.dump(os);
	os << std::endl;
	os << "Internal network:" << std::endl;
	_network.dump(os);
	os << "Values: " << std::endl;
	for (std::size_t y = 0; y < _height; ++y)
	{
		for (std::size_t x = 0; x < _width; ++x)
		{
			os << "[";
			for (const auto& value : getValues({x, y}))
				os << value << " ";
			os << "] ";
		}
		os << std::endl;
	}

}

