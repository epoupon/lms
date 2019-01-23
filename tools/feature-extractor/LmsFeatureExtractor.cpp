#include <stdlib.h>
#include <stdexcept>
#include <iostream>
#include <string>

#include <curl/curl.h>

#include "database/DatabaseHandler.hpp"
#include "database/Track.hpp"
#include "utils/Config.hpp"

static size_t writeToOstream(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	std::ofstream& ofs = *reinterpret_cast<std::ofstream*>(userdata);
	ofs.write(ptr, size*nmemb);
	return size*nmemb;
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
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToOstream);
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

		for (auto track : tracks)
		{
			if (track->getMBID().empty())
				continue;

			auto path = getLowLevelFeaturePath(track->getMBID());
			if (!boost::filesystem::exists(path))
				acousticBrainzGetLowLevel(track->getMBID(), path);
		}

	}
	catch( std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
	}

	return EXIT_SUCCESS;
}

