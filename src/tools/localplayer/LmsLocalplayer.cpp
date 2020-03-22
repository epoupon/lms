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
#include <iomanip>
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
#include "utils/String.hpp"

void commandStatus(ILocalPlayer& localPlayer)
{
	const auto status {localPlayer.getStatus()};
	switch (status.playState)
	{
		case ILocalPlayer::Status::PlayState::Playing:
			std::cout << "PLAYING" << std::endl;
			break;
		case ILocalPlayer::Status::PlayState::Paused:
			std::cout << "Paused" << std::endl;
			break;
		case ILocalPlayer::Status::PlayState::Stopped:
			std::cout << "Stopped" << std::endl;
			break;
	}
	if (status.entryIdx)
		std::cout << "Entry idx: " << *status.entryIdx << std::endl;
	if (status.currentPlayTime)
		std::cout << "Playing time: " << std::fixed << std::setprecision(3) << status.currentPlayTime->count() / float {1000} << " s" << std::endl;

}



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

		// wait for input to terminate
		std::cout << "Enter some commands:" << std::endl;
		while (1)
		{
			std::string commandLine;
			std::getline(std::cin, commandLine);

			std::vector<std::string> args {StringUtils::splitString(commandLine, " ")};

			const std::string& command {args[0]};

			if (command == "play")
			{
				const ILocalPlayer::EntryIndex entryIdx {StringUtils::readAs<ILocalPlayer::EntryIndex>(args[1]).value_or(0)};
				const std::chrono::milliseconds offset {StringUtils::readAs<std::size_t>(args[2]).value_or(0)};
				localPlayer->playEntry(entryIdx, offset);
				commandStatus(*localPlayer.get());
			}
			else if (command == "stop")
				localPlayer->stop();
			else if (command == "status")
			{
				commandStatus(*localPlayer.get());
			}
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
