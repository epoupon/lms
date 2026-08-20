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

#include <chrono>
#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/program_options.hpp>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "audio/Exception.hpp"
#include "audio/IAudioOutput.hpp"
#include "audio/PcmTypes.hpp"
#include "audio/utils/IAudioDecodeStreamer.hpp"

namespace lms
{
    class Player
    {
    public:
        Player(boost::asio::io_context& ioContext, audio::IAudioOutputContext& context, const std::filesystem::path& filePath, std::chrono::microseconds offset, const audio::PcmParameters& pcmParams)
            : _ioContext{ ioContext }
            , _context{ context }
            , _filePath{ filePath }
            , _offset{ offset }
            , _pcmParams{ pcmParams }
        {
        }

        ~Player() = default;
        Player(const Player&) = delete;
        Player& operator=(const Player&) = delete;

        void start()
        {
            _context.asyncWaitReady([this] {
                createStream();
            });
        }

    private:
        void createStream()
        {
            _outputStream = _context.createOutputStream("LMS-player", _pcmParams);

            _outputStream->asyncWaitReady([this] {
                startDecodeAndPlay();
            });
        }

        void startDecodeAndPlay()
        {
            audio::utils::AudioDecodeStreamerParameters params{
                .outputStream = *_outputStream,
                .bufferCount = 2,
                .bufferDuration = std::chrono::milliseconds{ 100 },
            };

            _fileStreamer = audio::utils::createAudioDecodeStreamer(_ioContext, params);
            _fileStreamer->start(_filePath, _offset, [this](bool aborted) {
                if (aborted)
                    std::cerr << "Playback aborted!" << std::endl;

                _outputStream->asyncDrain([] {});
                _sigInt.cancel();
                _playTimer.cancel();
            });

            _sigInt.async_wait([this](const boost::system::error_code& ec, [[maybe_unused]] int sigNumber) {
                if (ec)
                    return;

                assert(sigNumber == SIGINT);

                _fileStreamer->abort();
                _playTimer.cancel();
            });

            // Gives some time for the buffer to fill in
            _playTimer.expires_after(std::chrono::milliseconds{ 50 });
            _playTimer.async_wait([this](const boost::system::error_code& ec) {
                if (ec)
                    return;

                _outputStream->resume();
            });
        }

        boost::asio::io_context& _ioContext;
        audio::IAudioOutputContext& _context;
        const std::filesystem::path _filePath;
        const std::chrono::microseconds _offset;
        const audio::PcmParameters _pcmParams;
        boost::asio::signal_set _sigInt{ _ioContext, SIGINT };
        boost::asio::steady_timer _playTimer{ _ioContext };

        std::unique_ptr<audio::IAudioOutputStream> _outputStream;
        std::shared_ptr<audio::utils::IAudioDecodeStreamer> _fileStreamer;
    };
} // namespace lms

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
            ("input",program_options::value<std::string>()->required(), "Input audio file path")
            ("offset",program_options::value<std::size_t>()->default_value(0), "Input audio offset, in seconds")
            ("backend", program_options::value<std::string>()->default_value(std::string{ "auto" }, "auto"), "Backend to be used (value can be \"auto\", \"alsa\" or \"pulseaudio\")");
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

        const std::chrono::seconds offset{ vm["offset"].as<std::size_t>() };

        audio::AudioOutputBackend outputBackend;
        if (core::stringUtils::stringCaseInsensitiveEqual(vm["backend"].as<std::string>(), "alsa"))
            outputBackend = audio::AudioOutputBackend::ALSA;
        else if (core::stringUtils::stringCaseInsensitiveEqual(vm["backend"].as<std::string>(), "pulseaudio"))
            outputBackend = audio::AudioOutputBackend::PulseAudio;
        else if (core::stringUtils::stringCaseInsensitiveEqual(vm["backend"].as<std::string>(), "auto"))
            outputBackend = audio::AudioOutputBackend::Auto;
        else
            throw program_options::validation_error{ program_options::validation_error::invalid_option_value, "backend" };

        core::Service<core::logging::ILogger> logger{ core::logging::createLogger(core::logging::Severity::DEBUG) };

        try
        {
            const audio::PcmParameters decoderParams{
                .channelCount = 2,
                .sampleRate = 44'100,
                .sampleType = audio::PcmSampleType::Signed16,
                .byteOrder = std::endian::little,
                .planar = false,
            };

            boost::asio::io_context ioContext;

            auto audioOutputContext{ audio::createAudioOutputContext(ioContext, "LMS-audioplay", outputBackend) };
            if (!audioOutputContext)
                throw std::runtime_error{ "Audio output backend not available" };

            Player player{ ioContext, *audioOutputContext, inputPath, offset, decoderParams };
            player.start();

            ioContext.run();
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
