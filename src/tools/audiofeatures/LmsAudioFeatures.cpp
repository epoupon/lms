/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <iostream>

#include <boost/program_options.hpp>

#include "core/ILogger.hpp"

#include "audio/Exception.hpp"
#include "audio/IAudioFeaturesExtractor.hpp"

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
            ("input",program_options::value<std::string>()->required(), "Input audio file path");
        // clang-format on

        program_options::variables_map vm;
        program_options::store(program_options::parse_command_line(argc, argv, options), vm);

        if (vm.count("help"))
        {
            std::cout << options << "\n";
            return EXIT_SUCCESS;
        }

        // notify required params
        program_options::notify(vm);

        const std::filesystem::path inputPath{ vm["input"].as<std::string>() };
        if (!std::filesystem::exists(inputPath))
            throw std::runtime_error{ "File '" + inputPath.string() + "' does not exist!" };

        core::Service<core::logging::ILogger> logger{ core::logging::createLogger(core::logging::Severity::DEBUG) };

        try
        {
            auto extractor{ audio::createAudioFeaturesExtractor() };

            const auto res{ extractor->extractFeatures(inputPath) };
            std::cout << "Processed " << res.metadata.pcmSampleCount << " PCM samples in " << res.metadata.frameCount << " frames (" << res.metadata.pcmSampleRate << " Hz), frame size = " << res.metadata.frameSize << " samples (" << res.metadata.frameHopSize << " hop size)" << std::endl;
            std::cout << res.features;
        }
        catch (audio::Exception& e)
        {
            std::cerr << "Caught audio exception: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
