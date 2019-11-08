#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <stdlib.h>

#include "database/Db.hpp"
#include "database/Session.hpp"
#include "utils/Config.hpp"
#include "utils/Service.hpp"
#include "similarity/features/SimilarityFeaturesSearcher.hpp"

int main(int argc, char *argv[])
{
	try
	{
		using namespace Similarity;

		const FeatureSettingsMap featuresSettings
		{
//			{ "lowlevel.average_loudness",			1 },
//			{ "lowlevel.dynamic_complexity",		1 },
			{ "lowlevel.spectral_contrast_coeffs.median",	{1} },
			{ "lowlevel.erbbands.median",			{1} },
			{ "tonal.hpcp.median",				{1} },
			{ "lowlevel.melbands.median",			{1} },
			{ "lowlevel.barkbands.median",			{1} },
			{ "lowlevel.mfcc.mean",				{1} },
			{ "lowlevel.gfcc.mean",				{1} },
		};

		std::filesystem::path configFilePath {"/etc/lms.conf"};
		if (argc >= 2)
			configFilePath = std::string(argv[1], 0, 256);

		ServiceProvider<Config>::create(configFilePath);

		Database::Db db {getService<Config>()->getPath("working-dir") / "lms.db"};
		auto session {db.createSession()};

		std::cout << "Getting all features..." << std::endl;

		std::cout << "Classifying tracks..." << std::endl;
		// may be long...
		FeaturesSearcher searcher {*session, featuresSettings};

		std::cout << "Classifying tracks DONE" << std::endl;
	}
	catch( std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

