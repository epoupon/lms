
/*
 * Copyright (C) 2016 Emeric Poupon
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

#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>

#include <pstreams/pstream.h>

#include <boost/property_tree/json_parser.hpp>

#include "utils/Logger.hpp"
#include "utils/Path.hpp"

#include "FeatureExtractor.hpp"

namespace Feature {

static boost::filesystem::path	extractorPath = boost::filesystem::path();

bool
Extractor::init(void)
{
	static const std::string execName = "streaming_extractor_music";
	extractorPath = searchExecPath(execName);

	if (extractorPath.empty())
	{
		LMS_LOG(FEATURE, ERROR) << "Failed to find path to " << execName;
		return false;
	}

	return true;
}

Extractor::Extractor()
{ }



static bool fetchJSONData(boost::property_tree::ptree& pt, std::string url)
{
	try
	{
		curlpp::Cleanup myCleanup;

		std::ostringstream os;
		os << curlpp::options::Url(url);

		std::istringstream iss(os.str());
		boost::property_tree::ptree res;
		boost::property_tree::json_parser::read_json(iss, res);

		pt = res;
	}
	catch( curlpp::RuntimeError &e )
	{
		LMS_LOG(FEATURE, ERROR) << "curlpp error: " << e.what();
		return false;
	}
	catch( curlpp::LogicError &e )
	{
		LMS_LOG(FEATURE, ERROR) << "curlpp error: " << e.what();
		return false;
	}
	catch ( boost::property_tree::ptree_error& e)
	{
		LMS_LOG(FEATURE, ERROR) << "JSON paring failed: " << e.what();
		return false;
	}

	return true;
}


bool
Extractor::getLowLevel(boost::property_tree::ptree& pt, std::string mbid)
{
	LMS_LOG(FEATURE, DEBUG) << "Trying to fetch low level metadata for track '" << mbid << "' on AcousticBrainz";

	boost::property_tree::ptree res;
	if (!fetchJSONData(res, "https://acousticbrainz.org/" + mbid + "/low-level"))
		return false;

	auto message = res.get_child_optional("message");
	if (message)
	{
		LMS_LOG(FEATURE, ERROR) << "Track '" << mbid << "': cannot get data on AcousticBrainz: " << message->data();
		return false;
	}

	auto lowlevel = res.get_child_optional("lowlevel");
	if (!lowlevel)
	{
		LMS_LOG(FEATURE, ERROR) << "Track '" << mbid << "': low level data not found!";
		return false;
	}

	// Keep metadata to ease debugging
//	res.erase("metadata");

	pt = res;

	return true;
}

bool
Extractor::getHighLevel(boost::property_tree::ptree& pt, std::string mbid)
{
	LMS_LOG(FEATURE, DEBUG) << "Trying to fetch high level metadata for track '" << mbid << "' on AcousticBrainz";

	// TODO check MBID
	if (mbid.empty())
		return false;

	boost::property_tree::ptree res;
	if (!fetchJSONData(res, "https://acousticbrainz.org/" + mbid + "/high-level"))
		return false;

	auto message = res.get_child_optional("message");
	if (message)
	{
		LMS_LOG(FEATURE, ERROR) << "Track '" << mbid << "': cannot get data on AcousticBrainz: " << message->data();
		return false;
	}

	auto lowlevel = res.get_child_optional("highlevel");
	if (!lowlevel)
	{
		LMS_LOG(FEATURE, ERROR) << "Track '" << mbid << "': high level data not found!";
		return false;
	}

	res.erase("metadata");

	pt = res;

	return true;
}


bool
Extractor::getLowLevel(boost::property_tree::ptree& pt, boost::filesystem::path path)
{
	LMS_LOG(FEATURE, DEBUG) << "Extracting low level data from '" << path << "'";

	if (extractorPath.empty())
		return false;

	std::vector<std::string> args;

	args.push_back(extractorPath.string());
	args.push_back(path.string());
	args.push_back("-"); // output to stdout

	std::string jsonData;
	{
		redi::ipstream in;
		in.open(extractorPath.string(), args);
		if (!in.is_open())
		{
			LMS_LOG(FEATURE, ERROR) << "Exec failed!";
			return false;
		}

		bool firstLineHit = false;

		std::string line;
		while(std::getline(in, line))
		{
			if (!firstLineHit)
			{
				if (line != "{")
					continue;

				firstLineHit = true;
			}
			jsonData += line;
			jsonData += '\n';
		}
	}

	try
	{
		std::istringstream iss(jsonData);
		boost::property_tree::json_parser::read_json(iss, pt);

		pt.erase("metadata");
	}
	catch ( boost::property_tree::ptree_error& e)
	{
		LMS_LOG(FEATURE, ERROR) << "JSON parsing failed: " << e.what();
		return false;
	}

	return true;
}

} // namespace Feature

