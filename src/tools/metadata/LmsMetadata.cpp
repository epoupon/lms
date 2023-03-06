/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <chrono>
#include <iostream>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <stdlib.h>

#include <Wt/WDate.h>

#include "metadata/IParser.hpp"
#include "utils/StreamLogger.hpp"

static
std::ostream&
operator<<(std::ostream& os, const MetaData::Artist& artist)
{
	os << artist.name;

	if (artist.artistMBID)
		os << " (" << artist.artistMBID->getAsString() << ")";

	if (artist.sortName)
		os << " '" << *artist.sortName << "'";

	return os;
}

static
std::ostream&
operator<<(std::ostream& os, const MetaData::Release& release)
{
	os << release.name;

	if (release.releaseMBID)
		os << " (" << release.releaseMBID->getAsString() << ")" << std::endl;

	if (release.totalDisc)
		std::cout << "\tTotalDisc: " << *release.totalDisc << std::endl;

	for (const MetaData::Artist& artist : release.releaseArtists)
		std::cout << "\tAlbum artist: " << artist << std::endl;

	return os;
}

static
std::ostream&
operator<<(std::ostream& os, const MetaData::Disc& disc)
{
	if (!disc.subtitle.empty())
		os << disc.subtitle << std::endl;

	if (disc.totalTrack)
		std::cout << "\tTotalTrack: " << *disc.totalTrack << std::endl;

	if (disc.replayGain)
		std::cout << "\tDisc replay gain: " << *disc.replayGain << std::endl;

	return os;
}

void parse(MetaData::IParser& parser, const std::filesystem::path& file)
{
	using namespace MetaData;

	parser.setClusterTypeNames( {"ALBUMMOOD", "MOOD", "ALBUMGROUPING", "ALBUMGENRE", "GENRE"} );

	const auto start {std::chrono::steady_clock::now()};
	std::optional<Track> track {parser.parse(file, true)};
	if (!track)
	{
		std::cerr << "Parsing failed" << std::endl;
		return;
	}
	const auto end {std::chrono::steady_clock::now()};

	std::cout << "Parsing time: " << std::fixed << std::setprecision(2) << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000. << "ms" << std::endl;

	std::cout << "Track metadata:" << std::endl;

	for (const Artist& artist : track->artists)
		std::cout << "Artist: " << artist << std::endl;

	for (const Artist& artist : track->conductorArtists)
		std::cout << "Conductor: " << artist << std::endl;

	for (const Artist& artist : track->composerArtists)
		std::cout << "Composer: " << artist << std::endl;

	for (const Artist& artist : track->lyricistArtists)
		std::cout << "Lyricist: " << artist << std::endl;

	for (const Artist& artist : track->mixerArtists)
		std::cout << "Mixer: " << artist << std::endl;

	for (const auto& [role, artists] : track->performerArtists)
	{
		std::cout << "Performer";
		if (!role.empty())
			std::cout << " (" << role << ")";
		std::cout << ":" << std::endl;
		for (const Artist& artist : artists)
			std::cout << "\t" << artist << std::endl;
	}

	for (const Artist& artist : track->producerArtists)
		std::cout << "Producer: " << artist << std::endl;

	for (const Artist& artist : track->remixerArtists)
		std::cout << "Remixer: " << artist << std::endl;

	if (track->release)
		std::cout << "Release: " << *track->release;

	if (track->disc)
		std::cout << "Disc: " << *track->disc;

	std::cout << "Title: " << track->title << std::endl;

	if (track->trackMBID)
		std::cout << "track MBID = " << track->trackMBID->getAsString() << std::endl;

	if (track->recordingMBID)
		std::cout << "recording MBID = " << track->recordingMBID->getAsString() << std::endl;

	for (const auto& cluster : track->clusters)
	{
		std::cout << "Cluster: " << cluster.first << std::endl;
		for (const auto& name : cluster.second)
		{
			std::cout << "\t" << name << std::endl;
		}
	}

	std::cout << "Duration: " << std::fixed << std::setprecision(2) << track->duration.count() / 1000. << "s" << std::endl;

	if (track->trackNumber)
		std::cout << "Track: " << *track->trackNumber << std::endl;

	if (track->discNumber)
		std::cout << "Disc: " << *track->discNumber << std::endl;

	if (track->date.isValid())
		std::cout << "Date: " << track->date.toString("yyyy-MM-dd") << std::endl;

	if (track->originalDate.isValid())
		std::cout << "Original date: " << track->originalDate.toString("yyyy-MM-dd") << std::endl;

	std::cout << "HasCover = " << std::boolalpha << track->hasCover << std::endl;

	for (const auto& audioStream : track->audioStreams)
		std::cout << "Audio stream: " << audioStream.bitRate << " bps" << std::endl;

	if (track->replayGain)
		std::cout << "Track replay gain: " << *track->replayGain << std::endl;

	if (track->acoustID)
		std::cout << "AcoustID: " << track->acoustID->getAsString() << std::endl;

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
		// log to stdout
		Service<Logger> logger {std::make_unique<StreamLogger>(std::cout)};

		for (std::size_t  i {}; i < static_cast<std::size_t>(argc - 1); ++i)
		{
			std::filesystem::path file {argv[i + 1]};

			std::cout << "Parsing file '" << file << "'" << std::endl;

			{
				std::cout << "Using av:" << std::endl;
				auto parser {MetaData::createParser(MetaData::ParserType::AvFormat, MetaData::ParserReadStyle::Accurate)};
				parse(*parser, file);
			}

			{
				std::cout << "Using TagLib:" << std::endl;
				auto parser {MetaData::createParser(MetaData::ParserType::TagLib, MetaData::ParserReadStyle::Accurate)};
				parse(*parser, file);
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
