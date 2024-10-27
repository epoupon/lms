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
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <stdlib.h>

#include <Wt/WDate.h>
#include <boost/program_options.hpp>

#include "core/EnumSet.hpp"
#include "core/StreamLogger.hpp"
#include "core/String.hpp"
#include "metadata/Exception.hpp"
#include "metadata/IParser.hpp"

namespace lms::metadata
{
    std::ostream& operator<<(std::ostream& os, const Lyrics& lyrics)
    {
        os << "\tTitle display name: " << lyrics.displayTitle << std::endl;
        os << "\tArtist display name: " << lyrics.displayArtist << std::endl;
        os << "\tAlbum display name: " << lyrics.displayAlbum << std::endl;
        os << "\tOffset: " << lyrics.offset.count() << "ms" << std::endl;
        os << "\tLanguage: " << lyrics.language << std::endl;
        os << "\tSynchronized: " << !lyrics.synchronizedLines.empty() << std::endl;
        for (const auto& [timestamp, line] : lyrics.synchronizedLines)
            os << "\t" << core::stringUtils::formatTimestamp(timestamp) << " '" << line << "'" << std::endl;
        for (const auto& line : lyrics.unsynchronizedLines)
            os << "\t'" << line << "'" << std::endl;

        return os;
    }

    std::ostream& operator<<(std::ostream& os, const AudioProperties& audioProperties)
    {
        os << "\tBitrate: " << audioProperties.bitrate << " bps" << std::endl;
        os << "\tBitsPerSample: " << audioProperties.bitsPerSample << std::endl;
        os << "\tChannelCount: " << audioProperties.channelCount << std::endl;
        os << "\tDuration: " << std::fixed << std::setprecision(2) << audioProperties.duration.count() / 1000. << "s" << std::endl;
        os << "\tSampleRate: " << audioProperties.sampleRate << std::endl;

        return os;
    }

    std::ostream& operator<<(std::ostream& os, const Artist& artist)
    {
        os << artist.name;

        if (artist.mbid)
            os << " (" << artist.mbid->getAsString() << ")";

        if (artist.sortName)
            os << " '" << *artist.sortName << "'";

        return os;
    }

    std::ostream& operator<<(std::ostream& os, const Release& release)
    {
        os << release.name;
        if (!release.sortName.empty())
            os << " '" << release.sortName << "'";
        os << std::endl;

        for (std::string_view label : release.labels)
            std::cout << "\tLabel: " << label << std::endl;

        if (release.mbid)
            os << "\tRelease MBID = " << release.mbid->getAsString() << std::endl;

        if (release.groupMBID)
            os << "\tRelease Group MBID = " << release.groupMBID->getAsString() << std::endl;

        if (release.mediumCount)
            std::cout << "\tMediumCount: " << *release.mediumCount << std::endl;

        if (!release.artistDisplayName.empty())
            std::cout << "\tDisplay artist: " << release.artistDisplayName << std::endl;

        std::cout << "\tIsCompilation: " << std::boolalpha << release.isCompilation << std::endl;

        for (const Artist& artist : release.artists)
            std::cout << "\tRelease artist: " << artist << std::endl;

        for (std::string_view releaseType : release.releaseTypes)
            std::cout << "\tRelease type: " << releaseType << std::endl;

        return os;
    }

    std::ostream& operator<<(std::ostream& os, const Medium& medium)
    {
        if (!medium.name.empty())
            os << medium.name;
        os << std::endl;

        if (medium.position)
            os << "\tPosition: " << *medium.position << std::endl;

        if (!medium.media.empty())
            os << "\tMedia: " << medium.media << std::endl;

        if (medium.trackCount)
            std::cout << "\tTrackCount: " << *medium.trackCount << std::endl;

        if (medium.replayGain)
            std::cout << "\tReplay gain: " << *medium.replayGain << std::endl;

        if (medium.release)
            std::cout << "Release: " << *medium.release;

        return os;
    }

    void parse(IParser& parser, const std::filesystem::path& file)
    {
        using namespace metadata;

        const auto start{ std::chrono::steady_clock::now() };
        std::unique_ptr<Track> track{ parser.parse(file, true) };
        const auto end{ std::chrono::steady_clock::now() };

        std::cout << "Parsing time: " << std::fixed << std::setprecision(2) << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000. << "ms" << std::endl;

        std::cout << "Audio properties:\n"
                  << track->audioProperties << std::endl;

        std::cout << "Parsed metadata:" << std::endl;

        if (!track->artistDisplayName.empty())
            std::cout << "Display artist: " << track->artistDisplayName << std::endl;

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

        if (track->medium)
            std::cout << "Medium: " << *track->medium;

        std::cout << "Title: " << track->title << std::endl;

        if (track->mbid)
            std::cout << "Track MBID = " << track->mbid->getAsString() << std::endl;

        if (track->recordingMBID)
            std::cout << "Recording MBID = " << track->recordingMBID->getAsString() << std::endl;

        for (std::string_view genre : track->genres)
            std::cout << "Genre: " << genre << std::endl;

        for (std::string_view genre : track->moods)
            std::cout << "Mood: " << genre << std::endl;

        for (std::string_view grouping : track->groupings)
            std::cout << "Grouping: " << grouping << std::endl;

        for (std::string_view language : track->languages)
            std::cout << "Language: " << language << std::endl;

        for (const auto& [tag, values] : track->userExtraTags)
        {
            std::cout << "Tag: " << tag << std::endl;
            for (const auto& value : values)
            {
                std::cout << "\t" << value << std::endl;
            }
        }

        if (track->position)
            std::cout << "Position: " << *track->position << std::endl;

        if (track->date.isValid())
            std::cout << "Date: " << track->date.toString("yyyy-MM-dd") << std::endl;
        if (track->year)
            std::cout << "Year: " << *track->year << std::endl;

        if (track->originalDate.isValid())
            std::cout << "Original date: " << track->originalDate.toString("yyyy-MM-dd") << std::endl;
        if (track->originalYear)
            std::cout << "Original year: " << *track->originalYear << std::endl;

        std::cout << "HasCover = " << std::boolalpha << track->hasCover << std::endl;

        if (track->replayGain)
            std::cout << "Track replay gain: " << *track->replayGain << std::endl;

        if (track->acoustID)
            std::cout << "AcoustID: " << track->acoustID->getAsString() << std::endl;

        if (!track->copyright.empty())
            std::cout << "Copyright: " << track->copyright << std::endl;

        for (const Lyrics& lyrics : track->lyrics)
            std::cout << "Lyrics:\n"
                      << lyrics << std::endl;

        for (const auto& comment : track->comments)
            std::cout << "Comment: '" << comment << "'" << std::endl;

        if (!track->copyrightURL.empty())
            std::cout << "CopyrightURL: " << track->copyrightURL << std::endl;

        std::cout << std::endl;
    }
} // namespace lms::metadata

int main(int argc, char* argv[])
{
    try
    {
        using namespace lms;
        namespace program_options = boost::program_options;

        program_options::options_description options{ "Options" };
        // clang-format off
        options.add_options()
            ("help,h", "Display this help message")
            ("tag-delimiter", program_options::value<std::vector<std::string>>()->default_value(std::vector<std::string>{}, "[]"), "Tag delimiters (multiple allowed)")
            ("artist-tag-delimiter", program_options::value<std::vector<std::string>>()->default_value(std::vector<std::string>{}, "[]"), "Artist tag delimiters (multiple allowed)")
            ("parser", program_options::value<std::vector<std::string>>()->default_value(std::vector<std::string>{ "taglib" }, "[taglib]"), "Parser to be used (value can be \"taglib\", \"ffmpeg\" or \"lyrics\")");
        // clang-format on

        program_options::options_description hiddenOptions{ "Hidden options" };
        hiddenOptions.add_options()("file", program_options::value<std::vector<std::string>>()->composing(), "file");

        program_options::options_description allOptions;
        allOptions.add(options).add(hiddenOptions);

        program_options::positional_options_description positional;
        positional.add("file", -1); // Handle remaining arguments as input files

        program_options::variables_map vm;
        // Parse command line arguments with positional option handling
        program_options::store(program_options::command_line_parser(argc, argv)
                                   .options(allOptions)
                                   .positional(positional)
                                   .run(),
            vm);

        program_options::notify(vm);

        auto displayHelp = [&](std::ostream& os) {
            os << "Usage: " << argv[0] << " [options] file..." << std::endl;
            os << options << std::endl;
        };

        if (vm.count("help"))
        {
            displayHelp(std::cout);
            return EXIT_SUCCESS;
        }

        if (vm.count("file") == 0)
        {
            std::cerr << "No input file provided" << std::endl;
            displayHelp(std::cerr);
            return EXIT_FAILURE;
        }

        enum class Parser
        {
            Lyrics,
            Taglib,
            Ffmpeg,
        };

        if (vm.count("parser") == 0)
        {
            std::cerr << "You must specify at least one parser" << std::endl;
            return EXIT_FAILURE;
        }

        core::EnumSet<Parser> parsers;
        for (const std::string& strParser : vm["parser"].as<std::vector<std::string>>())
        {
            if (core::stringUtils::stringCaseInsensitiveEqual(strParser, "lyrics"))
                parsers.insert(Parser::Lyrics);
            else if (core::stringUtils::stringCaseInsensitiveEqual(strParser, "taglib"))
                parsers.insert(Parser::Taglib);
            else if (core::stringUtils::stringCaseInsensitiveEqual(strParser, "ffmpeg"))
                parsers.insert(Parser::Ffmpeg);
            else
            {
                std::cerr << "Invalid parser name '" << strParser << "'" << std::endl;
                return EXIT_FAILURE;
            }
        }

        const auto& inputFiles{ vm["file"].as<std::vector<std::string>>() };
        const auto& tagDelimiters{ vm["tag-delimiter"].as<std::vector<std::string>>() };
        const auto& artistTagDelimiters{ vm["artist-tag-delimiter"].as<std::vector<std::string>>() };

        for (std::string_view tagDelimiter : tagDelimiters)
        {
            std::cout << "Tag delimiter: '" << tagDelimiter << "'" << std::endl;
        }

        for (std::string_view artistTagDelimiter : artistTagDelimiters)
        {
            std::cout << "Artist tag delimiter: '" << artistTagDelimiter << "'" << std::endl;
        }

        // log to stdout
        core::Service<core::logging::ILogger> logger{ std::make_unique<core::logging::StreamLogger>(std::cout, core::logging::StreamLogger::allSeverities) };

        for (const std::string& inputFile : inputFiles)
        {
            std::filesystem::path file{ inputFile };

            std::cout << "Parsing file '" << file << "'" << std::endl;

            if (parsers.contains(Parser::Lyrics))
            {
                try
                {
                    std::cout << "Using Lyrics:" << std::endl;

                    std::ifstream ifs{ file.string() };
                    if (ifs)
                    {
                        const metadata::Lyrics lyrics{ metadata::parseLyrics(ifs) };
                        std::cout << lyrics << std::endl;
                    }
                    else
                        std::cerr << "Cannot open file '" << file.string() << "'";
                }
                catch (metadata::Exception& e)
                {
                    std::cerr << "Parsing failed: " << e.what() << std::endl;
                }
            }

            if (parsers.contains(Parser::Ffmpeg))
            {
                try
                {
                    std::cout << "Using Ffmpeg:" << std::endl;

                    auto parser{ metadata::createParser(metadata::ParserBackend::AvFormat, metadata::ParserReadStyle::Accurate) };
                    parser->setArtistTagDelimiters(artistTagDelimiters);
                    parser->setDefaultTagDelimiters(tagDelimiters);

                    parse(*parser, file);
                }
                catch (metadata::Exception& e)
                {
                    std::cerr << "Parsing failed: " << e.what() << std::endl;
                }
            }

            if (parsers.contains(Parser::Taglib))
            {
                try
                {
                    std::cout << "Using TagLib:" << std::endl;

                    auto parser{ metadata::createParser(metadata::ParserBackend::TagLib, metadata::ParserReadStyle::Accurate) };
                    parser->setArtistTagDelimiters(artistTagDelimiters);
                    parser->setDefaultTagDelimiters(tagDelimiters);

                    parse(*parser, file);
                }
                catch (metadata::Exception& e)
                {
                    std::cerr << "Parsing failed: " << e.what() << std::endl;
                }
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
