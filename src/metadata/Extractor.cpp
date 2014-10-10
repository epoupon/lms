/*
 * Copyright (C) 2013 Emeric Poupon
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


#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>

#include "Utils.hpp"

#include "Extractor.hpp"

namespace MetaData
{


extern "C" {

/**
 * Type of a function that libextractor calls for each
 * meta data item found.
 *
 * @param cls closure (user-defined)
 * @param plugin_name name of the plugin that produced this value;
 *        special values can be used (i.e. '&lt;zlib&gt;' for zlib being
 *        used in the main libextractor library and yielding
 *        meta data).
 * @param type libextractor-type describing the meta data
 * @param format basic format information about data
 * @param data_mime_type mime-type of data (not of the original file);
 *        can be NULL (if mime-type is not known)
 * @param data actual meta-data found
 * @param data_len number of bytes in data
 * @return 0 to continue extracting, 1 to abort
 */

int processMetaData(void *cls,
		const char *plugin_name,
		enum EXTRACTOR_MetaType type,
		enum EXTRACTOR_MetaFormat format,
		const char *data_mime_type,
		const char *data,
		size_t data_len)
{
	Items& items = *(reinterpret_cast<Items*>(cls));

	switch (type) {
		case EXTRACTOR_METATYPE_ARTIST:
			assert( std::string(data_mime_type) == "text/plain" );
			items.insert( std::make_pair(MetaData::Artist, std::string( data, data_len ? data_len - 1 : 0)) );
			break;
		case EXTRACTOR_METATYPE_TITLE:
			assert( std::string(data_mime_type) == "text/plain" );
			items.insert( std::make_pair(MetaData::Title, std::string( data, data_len ? data_len - 1 : 0)) );
			break;
		case EXTRACTOR_METATYPE_ALBUM:
			assert( std::string(data_mime_type) == "text/plain" );
			items.insert( std::make_pair(MetaData::Album, std::string( data, data_len ? data_len - 1 : 0)) );
			break;
		case EXTRACTOR_METATYPE_GENRE:
			assert( std::string(data_mime_type) == "text/plain" );
			{
				std::list<std::string> genres;
				if (readList(std::string( data, data_len ? data_len - 1 : 0), ";-:/,", genres))
					items.insert( std::make_pair(MetaData::Genre, genres));
			}
			break;
/*		case EXTRACTOR_METATYPE_PICTURE:
			std::cout << "picture spotted" << std::endl;
			break;*/

		case EXTRACTOR_METATYPE_COVER_PICTURE:
			{
				GenericData parsedData;
				const unsigned char* dataStart = reinterpret_cast<const unsigned char*>(data);
				parsedData.mimeType = std::string(data_mime_type);
				parsedData.data = std::vector<unsigned char>( &dataStart[0], &dataStart[data_len]);

				items.insert( std::make_pair(MetaData::Cover, parsedData));
			}
			break;

/*		case EXTRACTOR_METATYPE_EVENT_PICTURE:
			std::cout << "Event picture spotted" << std::endl;
			break;*/

/*		case EXTRACTOR_METATYPE_CONTRIBUTOR_PICTURE:
			std::cout << "Contrib picture spotted" << std::endl;
			break;*/

/*		case EXTRACTOR_METATYPE_SONG_COUNT:
			// How many songs in the album
			break;*/

/*		case EXTRACTOR_METATYPE_AUDIO_CODEC:
			assert( std::string(data_mime_type) == "text/plain" );
			std::cout << "Aduio codec: " << std::string( data, data_len ? data_len - 1 : 0) << std::endl;
			break;*/
		case EXTRACTOR_METATYPE_PUBLICATION_YEAR:
			assert( std::string(data_mime_type) == "text/plain" );
			std::cout << "publication year = " << std::string( data, data_len ? data_len - 1 : 0) << std::endl;
			break;

		case EXTRACTOR_METATYPE_PUBLICATION_DATE:
			assert( std::string(data_mime_type) == "text/plain" );
			std::cout << "publication date = " << std::string( data, data_len ? data_len - 1 : 0) << std::endl;
			break;

		case EXTRACTOR_METATYPE_ORIGINAL_RELEASE_YEAR:
			assert( std::string(data_mime_type) == "text/plain" );
			std::cout << "original release year = " << std::string( data, data_len ? data_len - 1 : 0) << std::endl;
			break;

		case EXTRACTOR_METATYPE_CREATION_TIME:
			assert( std::string(data_mime_type) == "text/plain" );
			{
				boost::posix_time::ptime p;
				if (readAs<boost::posix_time::ptime>(std::string( data, data_len ? data_len - 1 : 0),  p))
					items.insert( std::make_pair(MetaData::CreationTime, p));
				else if (readAsPosixTime(std::string( data, data_len ? data_len - 1 : 0), p))
					items.insert( std::make_pair(MetaData::CreationTime, p));
			}
			break;
		case EXTRACTOR_METATYPE_DURATION:
			assert( std::string(data_mime_type) == "text/plain" );
			{
				boost::posix_time::time_duration duration;
				if (readAs<boost::posix_time::time_duration>( std::string( data, data_len ? data_len - 1 : 0), duration))
					items.insert( std::make_pair(MetaData::Duration, duration) );
			}
			break;
		case EXTRACTOR_METATYPE_TRACK_NUMBER:
			assert( std::string(data_mime_type) == "text/plain" );
			{
				std::size_t number(0);
				if (readAs<size_t>( std::string( data, data_len ? data_len - 1 : 0), number))
					items.insert( std::make_pair(MetaData::TrackNumber, number) );
			}
			break;
		case EXTRACTOR_METATYPE_DISC_NUMBER:
			assert( std::string(data_mime_type) == "text/plain" );
			{
				std::size_t number(0);
				if (readAs<size_t>( std::string( data, data_len ? data_len - 1 : 0), number))
					items.insert( std::make_pair(MetaData::DiscNumber, number) );
			}
			break;
		default:
/*			if (std::string(data_mime_type) == "text/plain")
				std::cout << "TYPE = " << type << ", data = '" << std::string(data, data_len ? data_len - 1 : 0) << "'" << std::endl;
			else
				std::cout << "TYPE = " << type << ", data_mime_type = '" << data_mime_type << "', data len = " << data_len << std::endl;
			*/
			break;

	}
	return 0;
}


};


Extractor::Extractor()
: _plugins( nullptr )
{
//	_plugins = EXTRACTOR_plugin_add_config (nullptr, "mp3:ogg:flac:wav:gstreamer", EXTRACTOR_OPTION_DEFAULT_POLICY);
}

Extractor::~Extractor()
{
}

void
Extractor::parse(const boost::filesystem::path& p, Items& items)
{
	if (boost::filesystem::is_regular(p)) {
		_plugins = EXTRACTOR_plugin_add_defaults (EXTRACTOR_OPTION_DEFAULT_POLICY);
		if (_plugins == nullptr) {
			throw  std::runtime_error("EXTRACTOR_plugin_add_config failed!");
		}
		EXTRACTOR_extract (_plugins, p.string().c_str(), NULL, 0, &processMetaData, &items);
		EXTRACTOR_plugin_remove_all (_plugins);
	}
}

bool
Extractor::parseCover(const boost::filesystem::path& p, GenericData& data)
{
	bool res = false;
	Items items;
	if (boost::filesystem::is_regular(p)) {
		_plugins = EXTRACTOR_plugin_add_defaults (EXTRACTOR_OPTION_DEFAULT_POLICY);
		if (_plugins == nullptr) {
			throw  std::runtime_error("EXTRACTOR_plugin_add_config failed!");
		}
		EXTRACTOR_extract (_plugins, p.string().c_str(), NULL, 0, &processMetaData, &items);
		EXTRACTOR_plugin_remove_all (_plugins);

		if (items.find(MetaData::Cover) != items.end()) {
			data = boost::any_cast<GenericData>(items[MetaData::Cover]);
			res = true;
		}
	}

	return res;
}


} // namespace MetaData
