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

#include "audio/Exception.hpp"
#include "audio/IMusicNNEmbeddingExtractor.hpp"
#include "core/ILogger.hpp"

int main(int argc, char* argv[])
{
    try
    {
        using namespace lms;
        namespace program_options = boost::program_options;

        program_options::options_description options{ "Options" };
        // clang-format off
        options.add_options()
            ("help,h",   "Display this help message")
            ("model,m",  program_options::value<std::string>()->required(), "Path to the MusicNN model file")
            ("input,i",  program_options::value<std::string>()->required(), "Input audio file path")
            ("max-patch-count,p", program_options::value<unsigned>()->default_value(20), "Max non-overlapping patches (more = better accuracy but slower, must be > 0)");
        // clang-format on

        program_options::variables_map vm;
        program_options::store(program_options::parse_command_line(argc, argv, options), vm);

        if (vm.count("help"))
        {
            std::cout << options << "\n";
            return EXIT_SUCCESS;
        }

        program_options::notify(vm);

        const std::filesystem::path modelPath{ vm["model"].as<std::string>() };
        const std::filesystem::path inputPath{ vm["input"].as<std::string>() };

        if (!std::filesystem::exists(modelPath))
            throw std::runtime_error{ "Model file '" + modelPath.string() + "' does not exist!" };
        if (!std::filesystem::exists(inputPath))
            throw std::runtime_error{ "Input file '" + inputPath.string() + "' does not exist!" };

        core::Service<core::logging::ILogger> logger{ core::logging::createLogger(core::logging::Severity::WARNING) };

        try
        {
            const unsigned maxPatchCount{ vm["max-patch-count"].as<unsigned>() };

            const auto extractor{ audio::createMusicNNEmbeddingExtractor(modelPath, maxPatchCount) };
            const auto result{ extractor->extract(inputPath) };

            std::cout << "patch_count=" << result.patchCount << "\n";

            std::cout << "mean=";
            for (std::size_t i{}; i < audio::MusicNNEmbedding::size; ++i)
            {
                if (i > 0)
                    std::cout << ',';
                std::cout << result.embeddings.mean.values[i];
            }
            std::cout << "\n";
        }
        catch (const audio::Exception& e)
        {
            std::cerr << "Audio error: " << e.what() << "\n";
            return EXIT_FAILURE;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
