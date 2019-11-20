
#include <iostream>
#include <filesystem>
#include <string>

#include "database/Db.hpp"
#include "database/Session.hpp"
#include "utils/Config.hpp"
#include "utils/Service.hpp"
#include "utils/StreamLogger.hpp"


int main(int argc, char *argv[])
{
	try
	{
		// log to stdout
		ServiceProvider<Logger>::create<StreamLogger>(std::cout);

		std::filesystem::path configFilePath {"/etc/lms.conf"};
		if (argc >= 2)
			configFilePath = std::string(argv[1], 0, 256);

		ServiceProvider<Config>::create(configFilePath);

		Database::Db db {ServiceProvider<Config>::get()->getPath("working-dir") / "lms.db"};
		Database::Session session {db};

/*		const FeatureSettings
		{
			{ "lowlevel.average_loudness",			1 },
			{ "lowlevel.dynamic_complexity",		1 },
			{ "lowlevel.spectral_contrast_coeffs.median",	6 },
			{ "lowlevel.erbbands.median",			40 },
			{ "tonal.hpcp.median",				36 },
			{ "lowlevel.melbands.median",			40 },
			{ "lowlevel.barkbands.median",			27 },
			{ "lowlevel.mfcc.mean",				13 },
			{ "lowlevel.gfcc.mean",				13 },
		};

		const TrackFeaturesMap trackFeaturesMap {getAllTrackFeatures(*session)};

		std::cout << "Found " << trackFeaturesMap.size() << " tracks with features!" << std::endl;*/
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
	}

	return EXIT_SUCCESS;
}


