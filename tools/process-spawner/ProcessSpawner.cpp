#include <iostream>

#include "utils/ChildProcessManager.hpp"
#include "utils/Logger.hpp"
#include "utils/StreamLogger.hpp"

int main(int , char* [])
{

	boost::asio::io_context ioContext;

	ServiceProvider<Logger>::create<StreamLogger>(std::cout);

	ChildProcessManager myChildProcessManager {};
	IChildProcessManager& childProcessManager {myChildProcessManager};

	std::cout << "Starting..." << std::endl;
	childProcessManager.start();
	std::cout << "Started..." << std::endl;

	{
		std::cout << "SPAWNING..." << std::endl;
		auto process {childProcessManager.spawnChildProcess("/usr/bin/ffmpeg", {"/usr/bin/ffmpeg", "-loglevel", "quiet", "-nostdin", "-i", "/storage/common/Media/Son/Metal/Meshuggah/1995 - Destroy Erase Improve/06 - Acrid Placidity.mp3", "-vn", "-f", "mp3", "pipe:1"})};

		unsigned char data[65536];
		std::cout << "SPAWNED..." << std::endl;

		bool done{};
		std::function<void(void)> readSome = [&]()
		{
			process->asyncRead(data, 65536,
				[&](IChildProcess::ReadResult result, std::size_t readBytes)
				{
					std::cout << "CB, readBytes = " << readBytes << std::endl;

					if (result != IChildProcess::ReadResult::Success)
					{
						std::cerr << "Error" << std::endl;
						done = true;
						return;
					}


					readSome();
				});
		};

		readSome();

		while (!done)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{500});
			break;
		}

		std::cout << "Done!" << std::endl;
	}

	childProcessManager.stop();

	std::cout << "Exiting" << std::endl;
}

