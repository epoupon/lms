#include <stdlib.h>
#include <chrono>

#include <stdexcept>
#include <iostream>

#include <Wt/WDate.h>

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
		MetaData::TagLibParser parser;

		boost::optional<MetaData::Items> items = parser.parse(argv[1], true);
		if (!items)
		{
			std::cerr << "Parsing failed" << std::endl;
			return EXIT_FAILURE;
		}

		std::cout << "Items:" << std::endl;
		for (auto item : (*items))
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

				case MetaData::Type::Clusters:
					for (const auto& cluster : boost::any_cast<MetaData::Clusters>(item.second))
					{
						std::cout << "Cluster: " << cluster.first << std::endl;
						for (const auto name : cluster.second)
						{
							std::cout << "\t" << name << std::endl;
						}
					}
					break;

				case MetaData::Type::Duration:
					std::cout << "Duration: " << boost::any_cast<std::chrono::milliseconds>(item.second).count() / 1000 << "s" << std::endl;
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

				case MetaData::Type::Year:
					std::cout << "Year: " << std::to_string(boost::any_cast<int>(item.second)) << std::endl;
					break;

				case MetaData::Type::OriginalYear:
					std::cout << "Original year: " << std::to_string(boost::any_cast<int>(item.second)) << std::endl;
					break;

				case MetaData::Type::HasCover:
					std::cout << "HasCover = " << std::boolalpha << boost::any_cast<bool>(item.second) << std::endl;
					break;

				case MetaData::Type::AudioStreams:
					for (auto& audioStream : boost::any_cast<std::vector<MetaData::AudioStream> >(item.second))
						std::cout << "Audio stream: " << audioStream.bitRate << " bps" << std::endl;
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

				case MetaData::Type::AcoustID:
					std::cout << "AcoustID: " << boost::any_cast<std::string>(item.second) << std::endl;
					break;

				default:
					break;
			}
		}

		std::cout << std::endl;
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
