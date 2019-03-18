#include <chrono>
#include <stdexcept>
#include <stdlib.h>
#include <iostream>

#include <Wt/WDate.h>

#include "av/AvInfo.hpp"
#include "metadata/AvFormat.hpp"
#include "metadata/TagLibParser.hpp"

std::ostream& operator<<(std::ostream& os, const MetaData::Artist& artist)
{
	os << artist.name;

	if (!artist.musicBrainzArtistID.empty())
		os << " (" << artist.musicBrainzArtistID << ")";

	return os;
}

std::ostream& operator<<(std::ostream& os, const MetaData::Album& album)
{
	os << album.name;

	if (!album.musicBrainzAlbumID.empty())
		os << " (" << album.musicBrainzAlbumID << ")";

	return os;
}



void parse(MetaData::Parser& parser, const boost::filesystem::path& file)
{
	using namespace MetaData;

	parser.setClusterTypeNames( {"MOOD", "GENRE"} );

	boost::optional<Track> track {parser.parse(file, true)};
	if (!track)
	{
		std::cerr << "Parsing failed" << std::endl;
		return;
	}

	std::cout << "Track metadata:" << std::endl;

	for (const Artist& artist : track->artists)
		std::cout << "Artist: " << artist << std::endl;

	if (track->albumArtist)
		std::cout << "Album artist: " << *track->albumArtist << std::endl;

	if (track->album)
		std::cout << "Album: " << *track->album << std::endl;

	std::cout << "Title: " << track->title << std::endl;

	if (!track->musicBrainzTrackID.empty())
		std::cout << "MB TrackID = " << track->musicBrainzTrackID << std::endl;

	if (!track->musicBrainzRecordID.empty())
		std::cout << "MB RecordID = " << track->musicBrainzRecordID << std::endl;

	for (const auto& cluster : track->clusters)
	{
		std::cout << "Cluster: " << cluster.first << std::endl;
		for (const auto& name : cluster.second)
		{
			std::cout << "\t" << name << std::endl;
		}
	}

	std::cout << "Duration: " << track->duration.count() / 1000 << "s" << std::endl;

	if (track->trackNumber)
		std::cout << "Track: " << *track->trackNumber << std::endl;

	if (track->totalTrack)
		std::cout << "TotalTrack: " << *track->totalTrack << std::endl;

	if (track->discNumber)
		std::cout << "Disc: " << *track->discNumber << std::endl;

	if (track->totalDisc)
		std::cout << "TotalDisc: " << *track->totalDisc << std::endl;

	if (track->year)
		std::cout << "Year: " << *track->year << std::endl;

	if (track->originalYear)
		std::cout << "Original year: " << *track->originalYear << std::endl;

	std::cout << "HasCover = " << std::boolalpha << track->hasCover << std::endl;

	for (const auto& audioStream : track->audioStreams)
		std::cout << "Audio stream: " << audioStream.bitRate << " bps" << std::endl;

	if (!track->acoustID.empty())
		std::cout << "AcoustID: " << track->acoustID << std::endl;

	if (!track->copyright.empty())
		std::cout << "Copyright: " << track->copyright << std::endl;

	if (!track->copyrightURL.empty())
		std::cout << "CopyrightURL: " << track->copyrightURL << std::endl;

	std::cout << std::endl;
}

int main(int argc, char *argv[])
{
	if (argc == 1)
	{
		std::cerr << "Usage: <file> [<file> ...]" << std::endl;
		return EXIT_FAILURE;
	}

	try
	{
		Av::AvInit();

		for (std::size_t  i {}; i < static_cast<std::size_t>(argc - 1); ++i)
		{
			boost::filesystem::path file {argv[i + 1]};

			std::cout << "Parsing file '" << file << "'" << std::endl;

			{
				std::cout << "Using av:" << std::endl;
				MetaData::AvFormat parser;
				parse(parser, file);
			}

			{
				std::cout << "Using TagLib:" << std::endl;
				MetaData::TagLibParser parser;
				parse(parser, file);
			}
		}

	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
