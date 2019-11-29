/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <stdlib.h>

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "utils/Config.hpp"
#include "utils/Service.hpp"
#include "utils/StreamLogger.hpp"
#include "similarity/features/SimilarityFeaturesSearcher.hpp"

int main(int argc, char *argv[])
{
	try
	{
		using namespace Similarity;

		// log to stdout
		ServiceProvider<Logger>::create<StreamLogger>(std::cout);

		const FeatureSettingsMap featuresSettings
		{
/*			{ "lowlevel.average_loudness",			1 },
			{ "lowlevel.dynamic_complexity",		1 },
			{ "lowlevel.spectral_contrast_coeffs.median",	{1} },
			{ "lowlevel.erbbands.median",			{1} },
			{ "tonal.hpcp.median",				{1} },
			{ "lowlevel.melbands.median",			{1} },
			{ "lowlevel.barkbands.median",			{1} },
			{ "lowlevel.mfcc.mean",				{1} },
			{ "lowlevel.gfcc.mean",				{1} },
*/
			{ "lowlevel.spectral_kurtosis.median",	{1}},
			{ "lowlevel.spectral_kurtosis.mean",	{1}},
			{ "lowlevel.spectral_complexity.var",	{1}},
			{ "lowlevel.barkbands.median",	{1}},
			{ "lowlevel.barkbands_kurtosis.mean",	{1}},
				/*
				   {"lowlevel.spectral_centroid.dvar2",		{1} },
				   {"lowlevel.barkbands.median",		{1} },
				   {        "lowlevel.barkbands.dvar",		{1} },
				{        "lowlevel.spectral_complexity.min",		{1} },
				{        "lowlevel.pitch_salience.dmean2",		{1} },
				{        "lowlevel.spectral_contrast_valleys.dmean",		{1} },
				{        "lowlevel.pitch_salience.max",		{1} },
				{        "lowlevel.barkbands.mean",		{1} },
				{        "lowlevel.spectral_complexity.mean",		{1} },
				{        "lowlevel.dissonance.dvar",		{1} },
				*/
/*
			{ "lowlevel.spectral_energy.dvar",		{1} },
			{ "lowlevel.barkbands.min",			{1} },
			{ "lowlevel.spectral_centroid.median",		{1} },
			{"lowlevel.barkbands_kurtosis.median",		{1} },
			{"lowlevel.spectral_energy.median",		{1} },
			{"lowlevel.barkbands.max",		{1} },
			{"lowlevel.barkbands_spread.var",		{1} },
			{"lowlevel.spectral_decrease.var",		{1} },
			{"lowlevel.spectral_contrast_valleys.dmean",		{1} },
			{"lowlevel.barkbands_crest.mean",		{1} },
			{"lowlevel.spectral_entropy.var",		{1} },
			{"lowlevel.barkbands_crest.max",		{1} },
			{"lowlevel.hfc.dvar",		{1} },
			{"lowlevel.barkbands_skewness.dvar2",		{1} },
			{"lowlevel.spectral_centroid.max",		{1} },
			*/
		};

		std::filesystem::path configFilePath {"/etc/lms.conf"};
		if (argc >= 2)
			configFilePath = std::string(argv[1], 0, 256);

		ServiceProvider<Config>::create(configFilePath);

		Database::Db db {ServiceProvider<Config>::get()->getPath("working-dir") / "lms.db"};
		Database::Session session {db};

		std::cout << "Classifying tracks..." << std::endl;
		// may be long...
		struct FeaturesSearcher::TrainSettings trainSettings;
		trainSettings.featureSettingsMap = featuresSettings;
		FeaturesSearcher searcher {session, trainSettings};
		std::cout << "Classifying tracks DONE" << std::endl;

		const std::vector<Database::IdType> trackIds = std::invoke([&]()
					{
						auto transaction {session.createSharedTransaction()};
						return Database::Track::getAllIds(session);
					});

		std::cout << "*** Tracks ***" << std::endl;
		for (Database::IdType trackId : trackIds)
		{
			auto trackToString = [&](Database::IdType trackId)
			{
				std::string res;
				auto transaction {session.createSharedTransaction()};
				Database::Track::pointer track {Database::Track::getById(session, trackId)};

				res += track->getName();
				if (track->getRelease())
					res += " [" + track->getRelease()->getName() + "]";
				for (auto artist : track->getArtists())
					res += " - " + artist->getName();
				for (auto cluster : track->getClusters())
					res += " {" + cluster->getType()->getName() + "-"+ cluster->getName() + "}";

				return res;
			};

			std::cout << "Processing track '" << trackToString(trackId) << std::endl;
			for (Database::IdType similarTrackId : searcher.getSimilarTracks({trackId}, 3))
				std::cout << "\t- Similar track '" << trackToString(similarTrackId) << std::endl;
		}

		const std::vector<Database::IdType> releaseIds = std::invoke([&]()
					{
						auto transaction {session.createSharedTransaction()};
						return Database::Release::getAllIds(session);
					});

		std::cout << "*** Releases ***" << std::endl;
		for (Database::IdType releaseId : releaseIds)
		{
			auto releaseToString = [&](Database::IdType releaseId)
			{
				auto transaction {session.createSharedTransaction()};

				Database::Release::pointer release {Database::Release::getById(session, releaseId)};
				return release->getName();
			};

			std::cout << "Processing release '" << releaseToString(releaseId) << "'" << std::endl;
			for (Database::IdType similarReleaseId : searcher.getSimilarReleases({releaseId}, 3))
				std::cout << "\t- Similar release '" << releaseToString(similarReleaseId) << "'" << std::endl;
		}

		const std::vector<Database::IdType> artistIds = std::invoke([&]()
					{
						auto transaction {session.createSharedTransaction()};
						return Database::Artist::getAllIds(session);
					});

		std::cout << "*** Artists ***" << std::endl;
		for (Database::IdType artistId : artistIds)
		{
			auto artistToString = [&](Database::IdType artistId)
			{
				auto transaction {session.createSharedTransaction()};

				Database::Artist::pointer artist {Database::Artist::getById(session, artistId)};
				return artist->getName();
			};

			std::cout << "Processing artist '" << artistToString(artistId) << "'" << std::endl;
			for (Database::IdType similarArtistId : searcher.getSimilarArtists({artistId}, 3))
				std::cout << "\t- Similar artist '" << artistToString(similarArtistId) << "'" << std::endl;
		}

	}
	catch( std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

