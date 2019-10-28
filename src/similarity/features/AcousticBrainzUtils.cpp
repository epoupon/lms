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

#include "AcousticBrainzUtils.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <Wt/WIOService.h>
#include <Wt/Http/Client.h>

#include "utils/Config.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"


namespace AcousticBrainz
{

static std::string
getJsonData(const std::string& mbid)
{
	static const std::string defaultAPIURL = "https://acousticbrainz.org/api/v1/";

	const std::string url {getService<Config>()->getString("acousticbrainz-api-url", defaultAPIURL) + mbid + "/low-level"};

	boost::asio::io_service ioService;

	Wt::Http::Client client {ioService};
	client.setFollowRedirect(true);
	client.setSslCertificateVerificationEnabled(true);
	client.setMaximumResponseSize(256*1024);

	if (!client.get(url))
	{
		LMS_LOG(SIMILARITY, ERROR) << "Cannot perform a GET request to url '" << url << "'";
		return {};
	}

	std::string response;
	client.done().connect([&](Wt::AsioWrapper::error_code ec, const Wt::Http::Message &msg)
	{
		if (ec)
		{
			LMS_LOG(SIMILARITY, ERROR) << "GET request to url '" << url << "' failed: " << ec.message();
			return;
		}

		if (msg.status() != 200)
		{
			LMS_LOG(SIMILARITY, ERROR) << "GET request to url '" << url << "' failed: status = " << msg.status() << ", body = " << msg.body();
			return;
		}

		response = msg.body();
	});

	ioService.run();

	return response;
}

std::string
extractLowLevelFeatures(const std::string& mbid)
{
	return getJsonData(mbid);
}

} // namespace Scanner::AcousticBrainz
