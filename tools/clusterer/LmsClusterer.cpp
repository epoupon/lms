#include <stdlib.h>
#include <stdexcept>
#include <iostream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <curl/curl.h>

#include "clusterer/SOM.hpp"
#include "clusterer/DataNormalizer.hpp"
#include "clusterer/Clusterer.hpp"
#include "database/DatabaseHandler.hpp"
#include "database/Track.hpp"
#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "utils/Config.hpp"

static std::vector<std::string> features =
{
	"lowlevel.average_loudness",
	"lowlevel.barkbands_flatness_db.mean",
	"lowlevel.dissonance.mean",
	"lowlevel.dynamic_complexity",
	"lowlevel.hfc.mean",				// GOOD
	"lowlevel.melbands_crest.mean",
	"lowlevel.melbands_kurtosis.mean",
	"lowlevel.melbands_skewness.mean",
	"lowlevel.melbands_spread.mean",
	"lowlevel.pitch_salience.mean",
	"lowlevel.pitch_salience.var",
	"lowlevel.silence_rate_30dB.mean",
	"lowlevel.silence_rate_60dB.mean",
	"lowlevel.spectral_centroid.mean",
	"lowlevel.spectral_complexity.mean",
	"lowlevel.spectral_decrease.mean",
	"lowlevel.spectral_energy.mean",
	"lowlevel.spectral_energyband_high.mean",
	"lowlevel.spectral_energyband_low.mean",
	"lowlevel.spectral_energyband_middle_high.mean",
	"lowlevel.spectral_energyband_middle_low.mean",
	"lowlevel.spectral_entropy.mean",
	"lowlevel.spectral_flux.mean",
	"lowlevel.spectral_kurtosis.mean",
	"lowlevel.spectral_rms.mean",
	"lowlevel.spectral_skewness.mean",
	"lowlevel.spectral_spread.mean",
	"lowlevel.spectral_strongpeak.mean",
	"lowlevel.zerocrossingrate.mean",
	"rhythm.beats_loudness.mean",		// BAD
	"rhythm.bpm",
	"tonal.chords_changes_rate",		// OK
//	"tonal.chords_number_rate",		// BAD
	"tonal.chords_strength.mean",		// OK
	"tonal.hpcp_entropy.mean",		// GOOD
};

static size_t writeToFile(void *buffer, size_t size, size_t nmemb, void* ctx)
{
	std::ofstream& ofs = *reinterpret_cast<std::ofstream*>(ctx);

	ofs.write(reinterpret_cast<char*>(buffer), size * nmemb);

	return size * nmemb;
}

static void acousticBrainzGetLowLevel(const std::string& mbid, boost::filesystem::path output)
{
	std::string url = "http://acousticbrainz.org/api/v1/" + mbid + "/low-level";

	std::cout << "GET " << url << std::endl;

	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if (!curl)
	{
		return;
	}

	std::ofstream ofs(output.string().c_str());
	if (!ofs)
	{
		curl_easy_cleanup(curl);
		std::cerr << "Cannot open " << output.string() << " for writing purpose" << std::endl;
		return;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);

	res = curl_easy_perform(curl);

	if (res != CURLE_OK)
	{
		std::cerr << "perform failed: " << curl_easy_strerror(res) << std::endl;
	}

	curl_easy_cleanup(curl);
}

static boost::filesystem::path getLowLevelFeaturePath(const std::string& mbid)
{
	return boost::filesystem::path(Config::instance().getPath("working-dir") / "features" / mbid);
}

std::vector<double> getFeatures(const std::string& mbid)
{

	std::vector<double> res;

	try
	{
		boost::property_tree::ptree root;

		boost::property_tree::read_json(getLowLevelFeaturePath(mbid).string(), root);

		for (const auto& feature : features)
		{
			res.push_back(root.get<double>(feature));
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception during processing " << mbid << std::endl;
	}

	return res;
}

int main(int argc, char *argv[])
{
	try
	{
		boost::filesystem::path configFilePath = "/etc/lms.conf";

		if (argc >= 2)
			configFilePath = std::string(argv[1], 0, 256);

		Config::instance().setFile(configFilePath);

		Database::Handler::configureAuth();
		auto connectionPool = Database::Handler::createConnectionPool(Config::instance().getPath("working-dir") / "lms.db");
		Database::Handler db(*connectionPool);

		Wt::Dbo::Transaction transaction(db.getSession());

		auto tracks = Database::Track::getAll(db.getSession());

		std::vector<std::pair<std::vector<double>, Database::IdType>> entries;

		std::cout << "Constructing input vectors..." << std::endl;

		for (auto track : tracks)
		{
			if (track->getMBID().empty())
				continue;

			auto path = getLowLevelFeaturePath(track->getMBID());
			if (!boost::filesystem::exists(path))
				acousticBrainzGetLowLevel(track->getMBID(), path);

			if (!boost::filesystem::exists(path))
				continue;

			std::pair<std::vector<double>, Database::IdType> entry;

			entry.first = getFeatures(track->getMBID());
			entry.second = track.id();

			if (entry.first.size() == features.size())
				entries.push_back(std::move(entry));
		}

		std::cout << "Constructing input vectors... DONE" << std::endl;

		std::cout << "Clutering..." << std::endl;
		Clusterer<Database::IdType> clusterer(entries, features.size(), 500);

		std::cout << "Clusterer :" << std::endl;
		clusterer.dump(std::cout);
		std::cout << std::endl;

		for (const auto& cluster : clusterer.getAllClusters())
		{
			std::cout << "******************" << std::endl;
			for (const auto& value : cluster)
			{
				auto track = Database::Track::getById(db.getSession(), value);
				auto artist = track->getArtist();
				auto release = track->getRelease();

				std::cout << "\t" << value << " - " << (artist ? artist->getName() : "") << " - " << (release ? release->getName() : "" ) << " - " << track->getName() << std::endl;
			}
			std::cout << std::endl;
		}
	}
	catch( std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
	}

	return EXIT_SUCCESS;
}

