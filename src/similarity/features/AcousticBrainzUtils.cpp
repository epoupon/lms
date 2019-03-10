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
#include <curl/curl.h>

#include "utils/Config.hpp"
#include "utils/Logger.hpp"


namespace AcousticBrainz
{


static size_t writeToOStringStream(void *buffer, size_t size, size_t nmemb, void* ctx)
{
	std::ostringstream& oss = *reinterpret_cast<std::ostringstream*>(ctx);

	oss.write(reinterpret_cast<char*>(buffer), size * nmemb);

	return size * nmemb;
}

static std::string
getJsonData(const std::string& mbid)
{
	static const std::string defaultAPIURL = "https://acousticbrainz.org/api/v1/";

	std::string data;
	std::string url = Config::instance().getString("acousticbrainz-api-url", defaultAPIURL) + mbid + "/low-level";

	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if (!curl)
	{
		LMS_LOG(SIMILARITY, ERROR) << "CURL init failed";
		return data;
	}

	std::ostringstream oss;

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToOStringStream);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &oss);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		LMS_LOG(SIMILARITY, ERROR) << "CURL perform failed: " << curl_easy_strerror(res);
		return data;
	}

	curl_easy_cleanup(curl);

	data = std::move(oss.str());

	return data;
}

std::string
extractLowLevelFeatures(const std::string& mbid)
{
	if (boost::filesystem::exists("/storage/emeric/lms-dev/features/" + mbid))
	{
		std::ifstream ifs{std::string{"/storage/emeric/lms-dev/features/" + mbid}.c_str()};
		return std::string {std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};
	}

	return getJsonData(mbid);
}

} // namespace Scanner::AcousticBrainz
