/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "ListensParser.hpp"

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Value.h>
#include <Wt/Json/Parser.h>

#include "utils/Logger.hpp"
#include "Utils.hpp"

namespace
{
	using namespace Scrobbling::ListenBrainz;

	ListensParser::Entry
	parseListen(const Wt::Json::Object& listen)
	{
		ListensParser::Entry entry;

		// Mandatory fields
		const Wt::Json::Object& metadata = listen.get("track_metadata");
		entry.trackName = static_cast<std::string>(metadata.get("track_name"));
		entry.artistName = static_cast<std::string>(metadata.get("artist_name"));

		// Optional fields
		entry.releaseName = static_cast<std::string>(metadata.get("release_name").orIfNull(""));
		if (listen.type("listened_at") == Wt::Json::Type::Number)
			entry.listenedAt = Wt::WDateTime::fromTime_t(static_cast<int>(listen.get("listened_at")));
		if (!entry.listenedAt.isValid())
			LOG(ERROR) << "Invalid or missing 'listened_at' field!";

		if (metadata.type("additional_info") == Wt::Json::Type::Object)
		{
			const Wt::Json::Object& additionalInfo = metadata.get("additional_info");
			entry.recordingMBID = UUID::fromString(additionalInfo.get("recording_mbid").orIfNull(""));
			entry.releaseMBID = UUID::fromString(additionalInfo.get("release_mbid").orIfNull(""));

			int trackNumber {additionalInfo.get("tracknumber").orIfNull(-1)};
			if (trackNumber > 0)
				entry.trackNumber = trackNumber;
		}

		return entry;
	}
} // namespace

namespace Scrobbling::ListenBrainz
{
	std::vector<ListensParser::Entry>
	ListensParser::parse(std::string_view msgBody)
	{
		std::vector<Entry> entries;

		try
		{
			Wt::Json::Object root;
			Wt::Json::parse(std::string {msgBody}, root);

			const Wt::Json::Object& payload = root.get("payload");
			const Wt::Json::Array& listens = payload.get("listens");

			LOG(DEBUG) << "Parsing " << listens.size() << " listens...";

			if (listens.empty())
				return entries;

			for (const Wt::Json::Value& value : listens)
			{
				try
				{
					const Wt::Json::Object& listen = value;
					entries.push_back(parseListen(listen));
				}
				catch (const Wt::WException& error)
				{
					LOG(ERROR) << "Cannot parse 'listen': " << error.what();
				}
			}
		}
		catch (const Wt::WException& error)
		{
			LOG(ERROR) << "Cannot parse 'listens': " << error.what();
		}

		return entries;
	}

	std::ostream&
	operator<<(std::ostream& os, const ListensParser::Entry& entry)
	{
		os << "track name = '" << entry.trackName << "', artistName = '" << entry.artistName << "'";
		if (entry.listenedAt.isValid())
			os << ", listenedAt = " << entry.listenedAt.toString();
		if (!entry.releaseName.empty())
			os << ", releaseName = '" << entry.releaseName << "'";
		if (entry.trackNumber)
			os << ", trackNumber = " << *entry.trackNumber;
		if (entry.recordingMBID)
			os << ", recordingMBID = '" << entry.recordingMBID->getAsString() << "'";

		return os;
	}
} // Scrobbling::ListenBrainz
