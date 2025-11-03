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
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include <Wt/WDate.h>
#include <boost/program_options.hpp>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "audio/AudioTypes.hpp"
#include "audio/Exception.hpp"
#include "audio/IAudioFileInfo.hpp"
#include "audio/IImageReader.hpp"
#include "audio/ITagReader.hpp"

namespace lms::audio
{
    std::ostream& operator<<(std::ostream& os, const AudioProperties& audioProperties)
    {
        os << "\tDuration: " << std::fixed << std::setprecision(2) << std::chrono::duration_cast<std::chrono::duration<float>>(audioProperties.duration) << std::endl;
        os << "\tContainer: " << containerTypeToString(audioProperties.container) << std::endl;
        os << "\tCodec: " << codecTypeToString(audioProperties.codec) << std::endl;
        os << "\tBitrate: " << audioProperties.bitrate << " bps" << std::endl;
        if (audioProperties.bitsPerSample)
            os << "\tBitsPerSample: " << *audioProperties.bitsPerSample << std::endl;
        os << "\tChannelCount: " << audioProperties.channelCount << std::endl;
        os << "\tSampleRate: " << audioProperties.sampleRate << std::endl;

        return os;
    }

    void displayTags(const ITagReader& tagReader, TagType type)
    {
        std::vector<std::string> strs;

        tagReader.visitTagValues(type, [&](std::string_view value) {
            strs.push_back(std::string{ value });
        });

        if (strs.empty())
            return;

        std::cout << "\t" << tagTypeToString(type) << ": ";
        if (strs.size() == 1)
        {
            std::cout << "'" << strs.front() << "'\n";
        }
        else
        {
            std::cout << "[";
            bool first{ true };
            for (std::string_view str : strs)
            {
                if (!first)
                    std::cout << ", ";
                first = false;
                std::cout << "'" << str << "'";
            }
            std::cout << "]\n";
        };
    }

    void displayLyrics(const ITagReader& tagReader)
    {
        tagReader.visitLyricsTags([](std::string_view language, std::string_view lyrics) {
            std::cout << "Lyrics";
            if (!language.empty())
                std::cout << " (" << language << ")";
            std::cout << ":\n"
                      << lyrics << "\n\n";
        });
    }

    void displayPerformers(const ITagReader& tagReader)
    {
        tagReader.visitPerformerTags([](std::string_view role, std::string_view artist) {
            std::cout << "\tPerformer";
            if (!role.empty())
                std::cout << " '" << role << "'";
            std::cout << ": '" << artist << "'\n";
        });
    }

    void displayTags(const ITagReader& tagReader)
    {
        std::cout << "Tags:\n";
        for (int i{}; i < static_cast<int>(TagType::Count); ++i)
            displayTags(tagReader, static_cast<TagType>(i));

        displayPerformers(tagReader);
        displayLyrics(tagReader);
    }

    std::ostream& operator<<(std::ostream& os, const Image& image)
    {
        os << "\ttype = " << imageTypeToString(image.type) << std::endl;
        if (!image.description.empty())
            os << "\tdesc: " << image.description << std::endl;
        os << "\tmimeType: " << image.mimeType << std::endl;
        os << "\tsize: " << image.data.size() << std::endl;

        return os;
    }

    void displayImages(const IImageReader& imageReader)
    {
        imageReader.visitImages([](const Image& image) {
            std::cout << "Image:\n"
                      << image << "\n";
        });
    }

    void displayInfo(const IAudioFileInfo& fileInfo)
    {
        std::cout << "Audio properties:\n"
                  << fileInfo.getAudioProperties() << std::endl;

        displayImages(fileInfo.getImageReader());
        displayTags(fileInfo.getTagReader());

        std::cout << "\n";
    }
} // namespace lms::audio

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
            ("parser", program_options::value<std::string>()->default_value(std::string{ "taglib" }, "taglib"), "Parser to be used (value can be \"taglib\", \"ffmpeg\")");
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

        if (vm.count("parser") != 1)
        {
            std::cerr << "You must specify a parser to be used" << std::endl;
            return EXIT_FAILURE;
        }

        audio::ParserOptions parserOptions;
        parserOptions.enableExtraDebugLogs = true;
        if (core::stringUtils::stringCaseInsensitiveEqual(vm["parser"].as<std::string>(), "taglib"))
            parserOptions.parser = audio::ParserOptions::Parser::TagLib;
        else if (core::stringUtils::stringCaseInsensitiveEqual(vm["parser"].as<std::string>(), "ffmpeg"))
            parserOptions.parser = audio::ParserOptions::Parser::FFmpeg;
        else
            throw program_options::validation_error{ program_options::validation_error::invalid_option_value, "parser" };

        const auto& inputFiles{ vm["file"].as<std::vector<std::string>>() };
        // log to stdout
        core::Service<core::logging::ILogger> logger{ core::logging::createLogger(core::logging::Severity::DEBUG) };

        for (const std::string& inputFile : inputFiles)
        {
            const std::filesystem::path file{ inputFile };

            try
            {
                std::cout << "Parsing file " << file << ":\n";
                const auto audioFileInfo{ audio::parseAudioFile(file, parserOptions) };

                displayInfo(*audioFileInfo);
            }
            catch (const audio::Exception& e)
            {
                std::cerr << "Failed to parse file " << file << ": " << e.what() << std::endl;
                continue;
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