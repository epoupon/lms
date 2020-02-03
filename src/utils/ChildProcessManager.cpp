
#include "ChildProcessManager.hpp"

#include "utils/Logger.hpp"

#include "ChildProcess.hpp"

ChildProcessManager::ChildProcessManager()
: _work {boost::asio::make_work_guard(_ioContext)}
{}

void
ChildProcessManager::start()
{
	LMS_LOG(CHILDPROCESS, DEBUG) << "Starting...";

	_thread = std::make_unique<std::thread>([&]()
			{
	LMS_LOG(CHILDPROCESS, DEBUG) << "RUN";
				_ioContext.run();
	LMS_LOG(CHILDPROCESS, DEBUG) << "RUN DONE";
			});
}

void
ChildProcessManager::stop()
{
	LMS_LOG(CHILDPROCESS, DEBUG) << "Stopping...";
	_work.reset();
	_thread->join();
}

std::unique_ptr<IChildProcess>
ChildProcessManager::spawnChildProcess(const std::filesystem::path& path, const IChildProcess::Args& args)
{
	return std::make_unique<ChildProcess>(_ioContext, path, args);
}


