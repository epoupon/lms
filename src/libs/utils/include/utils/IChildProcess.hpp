#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

class IChildProcess
{
	public:
		using Args = std::vector<std::string>;

		virtual ~IChildProcess() = default;

		enum class ReadResult
		{
			Success,
			Error,
			EndOfFile,
		};

		using ReadCallback = std::function<void(ReadResult, std::size_t)>;
		virtual void		asyncRead(unsigned char* data, std::size_t bufferSize, ReadCallback callback) = 0;

		using WaitCallback = std::function<void(void)>;
		virtual void		asyncWaitForData(WaitCallback cb) = 0;
		virtual std::size_t	readSome(unsigned char* data, std::size_t bufferSize) = 0;
		virtual bool		finished() = 0;
};

