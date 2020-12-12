#pragma once

#include <memory>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>

#include "utils/IChildProcessManager.hpp"

class ChildProcessManager : public IChildProcessManager
{
	public:
		ChildProcessManager();
		~ChildProcessManager();

		ChildProcessManager(const ChildProcessManager&) = delete;
		ChildProcessManager(ChildProcessManager&&) = delete;
		ChildProcessManager& operator=(const ChildProcessManager&) = delete;
		ChildProcessManager& operator=(ChildProcessManager&&) = delete;

	private:
		std::unique_ptr<IChildProcess> spawnChildProcess(const std::filesystem::path& path, const IChildProcess::Args& args) override;

		void start();
		void stop();

		boost::asio::io_context	_ioContext;
		std::unique_ptr<std::thread> _thread;
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _work;
};


