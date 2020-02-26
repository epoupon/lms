/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>

#include "database/Db.hpp"
#include "database/Session.hpp"
#include "localplayer/ILocalPlayer.hpp"
#include "localplayer/PulseAudioOutputCreator.hpp"
#include "utils/IConfig.hpp"
#include "utils/IChildProcessManager.hpp"
#include "utils/Logger.hpp"
#include "utils/StreamLogger.hpp"
#include "utils/Service.hpp"

int main(int argc, char *argv[])
{
	try
	{
		// log to stdout
		Service<Logger> logger {std::make_unique<StreamLogger>(std::cout)};

		std::filesystem::path configFilePath {"/etc/lms.conf"};
		if (argc >= 2)
			configFilePath = std::string(argv[1], 0, 256);

		Service<IConfig> config {createConfig(configFilePath)};
		Service<IChildProcessManager> childProcessManager {createChildProcessManager()};

		Database::Db db {config->getPath("working-dir") / "lms.db"};
		Database::Session session {db};

		Service<ILocalPlayer> localPlayer {createLocalPlayer(db)};
		localPlayer->setAudioOutput(createPulseAudioOutput(IAudioOutput::Format::S16LE, 44100, 2));

		std::cout << "Waiting..." << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
		
		std::cout << "Now playing!" << std::endl;

		localPlayer->addTrack(49813);
		localPlayer->play();

		// wait for input to terminate
		std::cout << "Enter some commands:" << std::endl;
		while (1)
		{
			std::string command;
			std::cin >> command;

			if (command == "play")
				localPlayer->play();
			if (command == "stop")
				localPlayer->stop();
			else if (command == "quit")
				break;
		}

		return EXIT_SUCCESS;

	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
