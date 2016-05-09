#include <stdlib.h>

#include <boost/date_time/time_duration.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <stdexcept>
#include <iostream>

#include "av/AvInfo.hpp"
#include "metadata/AvFormat.hpp"
#include "metadata/TagLibParser.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: <file>" << std::endl;
		return EXIT_FAILURE;
	}

	try
	{
		Av::AvInit();


		MetaData::AvFormat avFormatParser;
		MetaData::TagLibParser tagLibParser;

		std::vector<MetaData::Parser*> parsers = { &avFormatParser, &tagLibParser };

		for (auto& parser : parsers)
		{
			MetaData::Items items;

			if (!parser->parse(argv[1], items))
			{
				std::cout << "Parsing failed" << std::endl;
				continue;
			}

			std::cout << "Items:" << std::endl;
			for (auto item : items)
			{
				switch (item.first)
				{
					case MetaData::Type::Title:
						std::cout << "Title: " << boost::any_cast<std::string>(item.second) << std::endl;
						break;

					case MetaData::Type::Artist:
						std::cout << "Artist: " << boost::any_cast<std::string>(item.second) << std::endl;
						break;

					case MetaData::Type::Album:
						std::cout << "Album: " << boost::any_cast<std::string>(item.second) << std::endl;
						break;

					case MetaData::Type::Genres:
						for (auto& genre : boost::any_cast<std::list<std::string> >(item.second))
							std::cout << "Genre: " << genre << std::endl;
						break;

					case MetaData::Type::Duration:
						std::cout << "Duration: " << boost::posix_time::to_simple_string(boost::any_cast<boost::posix_time::time_duration>(item.second)) << std::endl;
						break;

					case MetaData::Type::TrackNumber:
						std::cout << "Track: " << boost::any_cast<std::size_t>(item.second) << std::endl;
						break;

					case MetaData::Type::TotalTrack:
						std::cout << "TotalTrack: " << boost::any_cast<std::size_t>(item.second) << std::endl;
						break;

					case MetaData::Type::DiscNumber:
						std::cout << "Disc: " << boost::any_cast<std::size_t>(item.second) << std::endl;
						break;

					case MetaData::Type::TotalDisc:
						std::cout << "TotalDisc: " << boost::any_cast<std::size_t>(item.second) << std::endl;
						break;

					case MetaData::Type::Date:
						std::cout << "Date: " << boost::posix_time::to_simple_string(boost::any_cast<boost::posix_time::ptime>(item.second)) << std::endl;
						break;

					case MetaData::Type::OriginalDate:
						std::cout << "Original date: " << boost::posix_time::to_simple_string(boost::any_cast<boost::posix_time::ptime>(item.second)) << std::endl;
						break;

					case MetaData::Type::HasCover:
						std::cout << "HasCover = " << std::boolalpha << boost::any_cast<bool>(item.second) << std::endl;
						break;

					case MetaData::Type::AudioStreams:
						for (auto& audioStream : boost::any_cast<std::vector<MetaData::AudioStream> >(item.second))
							std::cout << "Audio stream '" << audioStream.desc << "' - " << audioStream.bitRate << " bps" << std::endl;
						break;

					case MetaData::Type::MusicBrainzArtistID:
						std::cout << "MusicBrainzArtistID: " << boost::any_cast<std::string>(item.second) << std::endl;
						break;

					case MetaData::Type::MusicBrainzAlbumID:
						std::cout << "MusicBrainzAlbumID: " << boost::any_cast<std::string>(item.second) << std::endl;
						break;

					case MetaData::Type::MusicBrainzTrackID:
						std::cout << "MusicBrainzTrackID: " << boost::any_cast<std::string>(item.second) << std::endl;
						break;

					case MetaData::Type::MusicBrainzRecordingID:
						std::cout << "MusicBrainzRecordingID: " << boost::any_cast<std::string>(item.second) << std::endl;
						break;

					default:
						break;
				}
			}

			std::cout << std::endl;
		}

		return EXIT_SUCCESS;
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what();
		return EXIT_FAILURE;
	}

}

