/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <boost/program_options.hpp>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Service.hpp"
#include "core/StreamLogger.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "image/Image.hpp"
#include "services/cover/ICoverService.hpp"

namespace lms
{
    void dumpTrackCovers(db::Session& session, image::ImageSize width)
    {
        using namespace db;

        RangeResults<db::TrackId> trackIds;
        {
            auto transaction{ session.createReadTransaction() };
            trackIds = db::Track::findIds(session, db::Track::FindParameters{});
        }

        for (const db::TrackId trackId : trackIds.results)
        {
            std::cout << "Getting cover for track id " << trackId.toString() << std::endl;
            core::Service<cover::ICoverService>::get()->getFromTrack(trackId, width);
        }
    }
} // namespace lms

int main(int argc, char* argv[])
{
    try
    {
        using namespace lms;
        namespace po = boost::program_options;

        // log to stdout
        core::Service<core::logging::ILogger> logger{ std::make_unique<core::logging::StreamLogger>(std::cout) };

        po::options_description desc{ "Allowed options" };
        desc.add_options()("help,h", "print usage message")("conf,c", po::value<std::string>()->default_value("/etc/lms.conf"), "LMS config file")("default-cover,d", po::value<std::string>(), "Default cover path")("tracks,t", "dump covers for tracks")("size,s", po::value<unsigned>()->default_value(512), "Requested cover size")("quality,q", po::value<unsigned>()->default_value(75), "JPEG quality (1-100)");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }

        image::init(argv[0]);
        core::Service<core::IConfig> config{ core::createConfig(vm["conf"].as<std::string>()) };
        db::Db db{ config->getPath("working-dir") / "lms.db" };
        core::Service<cover::ICoverService> coverArtService{ cover::createCoverService(db, vm["default-cover"].as<std::string>()) };

        coverArtService->setJpegQuality(config->getULong("cover-jpeg-quality", vm["quality"].as<unsigned>()));

        db::Session session{ db };

        if (vm.count("tracks"))
            dumpTrackCovers(session, vm["size"].as<unsigned>());
    }
    catch (std::exception& e)
    {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
