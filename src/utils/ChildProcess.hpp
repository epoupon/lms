#pragma once

#include <sys/types.h>
#include <unistd.h>

#include <filesystem>

#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include "IChildProcess.hpp"


class ChildProcess : public IChildProcess
{
	public:

		ChildProcess(boost::asio::io_context& ioContext, const std::filesystem::path& path, const Args& args);

		~ChildProcess();

	private:

		void			asyncRead(unsigned char* data, std::size_t bufferSize, ReadCallback callback) override;

		void			kill();
		void			drain();
		bool			wait(bool block); // return true if waited

		using FileDescriptor = boost::asio::posix::stream_descriptor;

		boost::asio::io_context&	_ioContext;
		FileDescriptor			_childStdout;
		pid_t				_childPID {};
		bool				_waited {};
		std::optional<int>		_exitCode;
};
