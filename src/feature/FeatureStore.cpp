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

#include <boost/property_tree/json_parser.hpp>

#include "logger/Logger.hpp"
#include "config/Config.hpp"

#include "FeatureStore.hpp"

namespace Feature {

Store::Store()
{
}

Store&
Store::instance(void)
{
	static Store instance;
	return instance;
}

void
Store::reload(void)
{
	_storePath = Config::instance().getString("features-dir-path", "");

	if (!boost::filesystem::is_directory(_storePath))
	{
		LMS_LOG(DBUPDATER, ERROR) << "Feature directory '" << _storePath << "' not valid!";
		throw std::runtime_error("Invalid feature directory '" + _storePath.string());
	}
}

boost::filesystem::path getPath(boost::filesystem::path root, std::string mbid, std::string type)
{
	return root / (mbid + "_" + type);
}

bool
Store::exists(Wt::Dbo::Session& session, Database::Track::id_type trackId, std::string type)
{
	if (_storePath.empty())
		reload();

	Wt::Dbo::Transaction transaction(session);

	auto track = Database::Track::getById(session, trackId);
	std::string mbid = track->getMBID();

	transaction.commit();

	if (mbid.empty())
		return false;

	boost::filesystem::path path = getPath(_storePath, mbid, type);
	return boost::filesystem::exists(path);
}

bool
Store::get(Wt::Dbo::Session& session, Database::Track::id_type trackId, std::string type, Type& feature)
{
	if (_storePath.empty())
		reload();

	Wt::Dbo::Transaction transaction(session);

	auto track = Database::Track::getById(session, trackId);
	std::string mbid = track->getMBID();

	transaction.commit();

	if (mbid.empty())
		return false;

	boost::filesystem::path path = getPath(_storePath, mbid, type);
	if (!boost::filesystem::exists(path))
		return false;

	try
	{
		std::ifstream iss(path.string().c_str(), std::ios::in);
		boost::property_tree::json_parser::read_json(iss, feature);
	}
	catch (boost::property_tree::ptree_error& e)
	{
		LMS_LOG(FEATURE, ERROR) << "JSON parsing failed: " << e.what();
		return false;
	}

	return true;
}


bool
Store::set(Wt::Dbo::Session& session, Database::Track::id_type trackId, std::string type, const Type& feature)
{
	if (_storePath.empty())
		reload();

	Wt::Dbo::Transaction transaction(session);

	auto track = Database::Track::getById(session, trackId);
	std::string mbid = track->getMBID();

	transaction.commit();

	if (mbid.empty())
		return false;

	boost::filesystem::path path = getPath(_storePath, mbid, type);

	try
	{
		std::ofstream oss(path.string().c_str(), std::ios::out);
		boost::property_tree::json_parser::write_json(oss, feature);
	}
	catch (boost::property_tree::ptree_error& e)
	{
		LMS_LOG(FEATURE, ERROR) << "JSON writing failed: " << e.what();
		return false;
	}

	return true;
}

} // namespace CoverArt

