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

#include "SimilarityFeaturesSearcher.hpp"

#include <random>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "database/Artist.hpp"
#include "database/SimilaritySettings.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"
#include "database/TrackFeatures.hpp"
#include "som/DataNormalizer.hpp"
#include "utils/Config.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"


namespace Similarity {

static
boost::filesystem::path getCacheDirectory()
{
	return Config::instance().getPath("working-dir") / "cache" / "features";
}

static boost::filesystem::path getCacheNetworkFilePath()
{
	return getCacheDirectory() / "network";
};

static boost::filesystem::path getCacheTrackPositionsFilePath()
{
	return getCacheDirectory() / "track_positions";
}

static
bool
networkToCacheFile(const SOM::Network& network, boost::filesystem::path path)
{
	try
	{
		boost::property_tree::ptree root;

		root.put("width", network.getWidth());
		root.put("height", network.getHeight());
		root.put("dim_count", network.getInputDimCount());

		for (auto weight : network.getDataWeights())
			root.add("weights.weight", weight);

		for (SOM::Coordinate x = 0; x < network.getWidth(); ++x)
		{
			for (SOM::Coordinate y = 0; y < network.getWidth(); ++y)
			{
				const auto& refVector = network.getRefVector({x, y});

				boost::property_tree::ptree node;
				for (auto value : refVector)
					node.add("values.value", value);

				node.put("coord_x", x);
				node.put("coord_y", y);

				root.add_child("ref_vectors.ref_vector", node);
			}
		}

		boost::property_tree::write_xml(path.string(), root);

		LMS_LOG(SIMILARITY, DEBUG) << "Created network cache";
		return true;
	}
	catch (boost::property_tree::ptree_error& error)
	{
		LMS_LOG(SIMILARITY, ERROR) << "Cannot create network cache: " << error.what();
		return false;
	}
}

static
boost::optional<SOM::Network>
createNetworkFromCacheFile(boost::filesystem::path path)
{
	try
	{
		boost::property_tree::ptree root;

		boost::property_tree::read_xml(path.string(), root);

		auto width = root.get<double>("width");
		auto height = root.get<double>("height");
		auto dimCount = root.get<std::size_t>("dim_count");

		SOM::Network res(width, height, dimCount);

		SOM::InputVector weights;
		for (const auto& val : root.get_child("weights"))
			weights.push_back(val.second.get_value<double>());

		res.setDataWeights(weights);

		for (const auto& node : root.get_child("ref_vectors"))
		{
			auto x = node.second.get<SOM::Coordinate>("coord_x");
			auto y = node.second.get<SOM::Coordinate>("coord_y");

			std::vector<double> values;
			for (const auto& val : node.second.get_child("values"))
				values.push_back(val.second.get_value<double>());

			res.setRefVector({x, y}, values);
		}

		LMS_LOG(SIMILARITY, DEBUG) << "Successfully read network from cache";

		return res;
	}
	catch (boost::property_tree::ptree_error& error)
	{
		LMS_LOG(SIMILARITY, ERROR) << "Cannot read network cache: " << error.what();
		return boost::none;
	}
}

static
bool
objectPositionToCacheFile(const std::map<Database::IdType, std::set<SOM::Position>>& objectsPosition, boost::filesystem::path path)
{
	try
	{
		boost::property_tree::ptree root;

		for (const auto& objectPosition : objectsPosition)
		{
			boost::property_tree::ptree node;

			node.put("id", objectPosition.first);

			for (const auto& position : objectPosition.second)
			{
				boost::property_tree::ptree positionNode;
				positionNode.put("x", position.x);
				positionNode.put("y", position.y);

				node.add_child("position.position", positionNode);
			}

			root.add_child("objects.object", node);
		}

		boost::property_tree::write_xml(path.string(), root);
		return true;
	}
	catch (boost::property_tree::ptree_error& error)
	{
		LMS_LOG(SIMILARITY, ERROR) << "Cannot cache object position: " << error.what();
		return false;
	}
}

static
boost::optional<std::map<Database::IdType, std::set<SOM::Position>>>
createObjectPositionsFromCacheFile(boost::filesystem::path path)
{
	try
	{
		boost::property_tree::ptree root;

		boost::property_tree::read_xml(path.string(), root);

		std::map<Database::IdType, std::set<SOM::Position>> res;

		for (const auto& object : root.get_child("objects"))
		{
			auto id = object.second.get<Database::IdType>("id");
			for (const auto& position : object.second.get_child("position"))
			{
				auto x = position.second.get<SOM::Coordinate>("x");
				auto y = position.second.get<SOM::Coordinate>("y");

				res[id].insert({x, y});
			}
		}

		LMS_LOG(SIMILARITY, DEBUG) << "Successfully read object position from cache";

		return res;
	}
	catch (boost::property_tree::ptree_error& error)
	{
		LMS_LOG(SIMILARITY, ERROR) << "Cannot create object position from cache file: " << error.what();
		return boost::none;
	}
}

bool
FeaturesSearcher::init(Wt::Dbo::Session& session, bool& stopRequested)
{
	Wt::Dbo::Transaction transaction(session);

	auto settings = Database::SimilaritySettings::get(session);

	struct FeatureInfo
	{
		std::size_t	nbDimensions;
		double		weight;
	};

	std::map<std::string, FeatureInfo> featuresInfo;
	std::size_t nbDimensions = 0;
	for (auto feature : settings->getFeatures())
	{
		featuresInfo[feature->getName()] = { feature->getNbDimensions(), feature->getWeight() };
		nbDimensions += feature->getNbDimensions();
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Getting Tracks with features...";
	auto tracks = Database::Track::getAllWithFeatures(session);
	LMS_LOG(SIMILARITY, DEBUG) << "Getting Tracks with features DONE";

	std::vector<SOM::InputVector> samples;
	std::vector<Database::IdType> tracksIds;

	LMS_LOG(SIMILARITY, DEBUG) << "Extracting features...";
	for (auto track : tracks)
	{
		if (stopRequested)
			return false;

		SOM::InputVector sample;

		std::map<std::string, std::vector<double>> features;
		for (const auto& featureInfo : featuresInfo)
			features[featureInfo.first] = {};

		if (!track->getTrackFeatures()->getFeatures(features))
			continue;

		// Check dimensions for each feature
		bool ok = true;
		for (const auto& feature : features)
		{
			auto it = featuresInfo.find(feature.first);
			if (it == featuresInfo.end() || it->second.nbDimensions != feature.second.size())
			{
				LMS_LOG(SIMILARITY, WARNING) << "Dimension mismatch for feature '" << feature.first << "'. Expected " << it->second.nbDimensions << ", got " << feature.second.size();
				ok = false;
				break;
			}

			sample.insert( sample.end(), feature.second.begin(), feature.second.end() );
		}

		if (!ok)
			continue;

		samples.emplace_back(std::move(sample));
		tracksIds.emplace_back(track.id());
	}
	LMS_LOG(SIMILARITY, DEBUG) << "Extracting features DONE";

	transaction.commit();

	if (tracksIds.empty())
	{
		LMS_LOG(SIMILARITY, INFO) << "Nothing to classify!";
		return false;
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Normalizing data...";
	SOM::DataNormalizer dataNormalizer(nbDimensions);

	dataNormalizer.computeNormalizationFactors(samples);
	for (auto& sample : samples)
		dataNormalizer.normalizeData(sample);

	std::size_t size = std::sqrt(samples.size()/2);
	LMS_LOG(SIMILARITY, INFO) << "Found " << samples.size() << " tracks, constructing a " << size << "*" << size << " network";

	std::vector<double> weights;
	for (const auto& featureInfo : featuresInfo)
	{
		for (std::size_t i = 0; i < featureInfo.second.nbDimensions; ++i)
			weights.push_back(1. / featureInfo.second.nbDimensions * featureInfo.second.weight);
	}

	SOM::Network network(size, size, nbDimensions);
	network.setDataWeights(weights);

	auto progressIndicator{[](const auto& iter)
		{
			LMS_LOG(SIMILARITY, DEBUG) << "Current pass = " << iter.idIteration << " / " << iter.iterationCount;
		}};

	auto stopper{[&]() { return stopRequested; }};

	LMS_LOG(SIMILARITY, DEBUG) << "Training network...";
	network.train(samples, 1, progressIndicator, stopper);
	LMS_LOG(SIMILARITY, DEBUG) << "Training network DONE";

	if (stopRequested)
		return false;

	LMS_LOG(SIMILARITY, DEBUG) << "Classifying tracks...";
	std::map<Database::IdType, std::set<SOM::Position>> trackPosition;
	for (std::size_t i = 0; i < samples.size(); ++i)
	{
		if (stopRequested)
			return false;

		Wt::Dbo::Transaction transaction(session);

		const auto& sample = samples[i];
		auto trackId = tracksIds[i];
		auto position = network.getClosestRefVectorPosition(sample);

		trackPosition[trackId].insert(position);
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Classifying tracks DONE";

	init(session, std::move(network), std::move(trackPosition));

	saveToCache();

	return true;
}

bool
FeaturesSearcher::initFromCache(Wt::Dbo::Session& session)
{
	auto network{createNetworkFromCacheFile(getCacheNetworkFilePath())};
	if (!network)
	{
		clearCache();
		return false;
	}

	auto trackPositions{createObjectPositionsFromCacheFile(getCacheTrackPositionsFilePath())};
	if (!trackPositions)
	{
		clearCache();
		return false;
	}

	init(session, std::move(*network), std::move(*trackPositions));

	LMS_LOG(SIMILARITY, DEBUG) << "Init from cache OK";

	return true;
}

void
FeaturesSearcher::invalidateCache()
{
	boost::filesystem::remove(getCacheNetworkFilePath());
	boost::filesystem::remove(getCacheTrackPositionsFilePath());
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarTracks(const std::set<Database::IdType>& tracksIds, std::size_t maxCount) const
{
	return getSimilarObjects(tracksIds, _tracksMap, _trackPosition, maxCount);
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarReleases(Database::IdType releaseId, std::size_t maxCount) const
{
	return getSimilarObjects({releaseId}, _releasesMap, _releasePosition, maxCount);
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarArtists(Database::IdType artistId, std::size_t maxCount) const
{
	return getSimilarObjects({artistId}, _artistsMap, _artistPosition, maxCount);
}

void
FeaturesSearcher::dump(Wt::Dbo::Session& session, std::ostream& os) const
{
	os << "Number of tracks classified: " << _trackPosition.size() << std::endl;
	os << "Network size: " << _network.getWidth() << " * " << _network.getHeight() << std::endl;
	os << "Ref vectors median distance = " << _networkRefVectorsDistanceMedian << std::endl;

	Wt::Dbo::Transaction transaction(session);

	for (SOM::Coordinate y = 0; y < _network.getHeight(); ++y)
	{
		for (SOM::Coordinate x = 0; x < _network.getWidth(); ++x)
		{
			const auto& trackIds = _tracksMap[{x, y}];

			os << "{" << x << ", " << y << "}";

			if (y > 0)
				os << " - {" << x << ", " << y - 1 << "}: " << _network.getRefVectorsDistance({x, y}, {x, y - 1});
			if (x > 0)
				os << " - {" << x - 1 << ", " << y << "}: " << _network.getRefVectorsDistance({x, y}, {x - 1, y});
			if (y !=  _network.getHeight() - 1)
				os << " - {" << x << ", " << y + 1 << "}: " << _network.getRefVectorsDistance({x, y}, {x, y + 1});
			if (x !=  _network.getWidth() - 1)
				os << " - {" << x + 1 << ", " << y << "}: " << _network.getRefVectorsDistance({x, y}, {x + 1, y});
			os << std::endl;

			for (auto trackId : trackIds)
			{
				auto track = Database::Track::getById(session, trackId);
				if (!track)
					continue;

				os << "\t - " << track->getName() << " - ";
				if (track->getArtist())
					os << track->getArtist()->getName() << " - ";
				if (track->getRelease())
					os << track->getRelease()->getName();
				os << std::endl;
			}

		}
		os << std::endl;
	}
}


void
FeaturesSearcher::init(Wt::Dbo::Session& session,
			SOM::Network network,
			std::map<Database::IdType, std::set<SOM::Position>> tracksPosition)
{
	_network = std::move(network);

	_networkRefVectorsDistanceMedian = _network.computeRefVectorsDistanceMedian();
	LMS_LOG(SIMILARITY, DEBUG) << "Median distance betweend ref vectors = " << _networkRefVectorsDistanceMedian;

	auto width = _network.getWidth();
	auto height = _network.getHeight();

	_artistsMap = SOM::Matrix<std::set<Database::IdType>>(width, height);
	_releasesMap = SOM::Matrix<std::set<Database::IdType>>(width, height);
	_tracksMap = SOM::Matrix<std::set<Database::IdType>>(width, height);

	Wt::Dbo::Transaction transaction(session);

	for (auto itTrackCoord : tracksPosition)
	{
		auto trackId = itTrackCoord.first;
		const auto& positionSet = itTrackCoord.second;

		auto track = Database::Track::getById(session, trackId);
		if (!track)
			continue;

		for (const auto& position : positionSet)
		{
			_tracksMap[position].insert(trackId);
			_trackPosition[trackId].insert(position);

			if (track->getRelease())
			{
				_releasePosition[track->getRelease().id()].insert(position);
				_releasesMap[position].insert(track->getRelease().id());
			}
			if (track->getArtist())
			{
				_artistPosition[track->getArtist().id()].insert(position);
				_artistsMap[position].insert(track->getArtist().id());
			}
		}
	}

	LMS_LOG(SIMILARITY, DEBUG) << "Classifying tracks DONE";

}

void
FeaturesSearcher::saveToCache() const
{
	if (!networkToCacheFile(_network, getCacheNetworkFilePath())
		|| !objectPositionToCacheFile(_trackPosition, getCacheTrackPositionsFilePath()))
	{
		LMS_LOG(SIMILARITY, ERROR) << "Failed cache data";
		clearCache();
	}
}

void
FeaturesSearcher::clearCache() const
{
	for (boost::filesystem::directory_iterator itEnd, it(getCacheDirectory()); it != itEnd; ++it)
		boost::filesystem::remove_all(it->path());
}


static
std::set<SOM::Position>
getMatchingRefVectorsPosition(const std::set<Database::IdType>& ids, const std::map<Database::IdType, std::set<SOM::Position>>& objectPosition)
{
	std::set<SOM::Position> res;

	if (ids.empty())
		return res;

	for (auto id : ids)
	{
		auto it = objectPosition.find(id);
		if (it == objectPosition.end())
			continue;

		for (const auto& position : it->second)
			res.insert(position);
	}

	return res;
}

static
std::set<Database::IdType>
getObjectsIds(const std::set<SOM::Position>& positionSet, const SOM::Matrix<std::set<Database::IdType>>& objectsMap )
{
	std::set<Database::IdType> res;

	for (const auto& position : positionSet)
	{
		for (auto id : objectsMap.get(position))
			res.insert(id);
	}

	return res;
}

std::vector<Database::IdType>
FeaturesSearcher::getSimilarObjects(const std::set<Database::IdType>& ids,
		const SOM::Matrix<std::set<Database::IdType>>& objectsMap,
		const std::map<Database::IdType, std::set<SOM::Position>>& objectPosition,
		std::size_t maxCount) const
{
	std::vector<Database::IdType> res;

	auto now = std::chrono::system_clock::now();
	std::mt19937 randGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

	std::set<SOM::Position> searchedRefVectorsPosition = getMatchingRefVectorsPosition(ids, objectPosition);
	if (searchedRefVectorsPosition.empty())
		return res;

	while (1)
	{
		std::set<Database::IdType> closestObjectIds = getObjectsIds(searchedRefVectorsPosition, objectsMap);

		// Remove objects that are already in input or already reported
		for (auto id : ids)
			closestObjectIds.erase(id);

		for (auto id : res)
			closestObjectIds.erase(id);

		{
			std::vector<Database::IdType> objectIdsToAdd(closestObjectIds.begin(), closestObjectIds.end());

			std::shuffle(objectIdsToAdd.begin(), objectIdsToAdd.end(), randGenerator);
			std::copy(objectIdsToAdd.begin(), objectIdsToAdd.end(), std::back_inserter(res));
		}

		if (res.size() > maxCount)
			res.resize(maxCount);

		if (res.size() == maxCount)
			break;

		// If there is not enough objects, try again with closest neighbour until there is too much distance
		auto closestRefVectorPosition = _network.getClosestRefVectorPosition(searchedRefVectorsPosition, _networkRefVectorsDistanceMedian * 0.75);
		if (!closestRefVectorPosition)
			break;

		searchedRefVectorsPosition.insert(*closestRefVectorPosition);
	}

	return res;
}



} // ns Similarity
