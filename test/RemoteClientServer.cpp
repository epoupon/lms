
#include <stdexcept>
#include <thread>

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

#include "remote/server/Server.hpp"
#include "remote/messages/Header.hpp"
#include "remote/messages/messages.pb.h"

#include "TestDatabase.hpp"


// Ugly class for testing purposes
class TestServer
{
	public:
		TestServer(boost::asio::ip::tcp::endpoint endpoint)
			: _db(TestDatabase::create()),
			 _server(_ioService, endpoint, _db->getPath()),
			 _thread( boost::bind(&TestServer::threadEntry, this))
		{
			_server.run();
		}

		void stop()
		{
			_ioService.stop();
			_thread.join();
		}

	private:

		void threadEntry(void)
		{
			_ioService.run();
		}

		boost::asio::io_service			_ioService;
		std::unique_ptr<DatabaseHandler>	_db;
		Remote::Server::Server			_server;
		std::thread				_thread;

};


class TestClient
{
	public:
		TestClient(boost::asio::ip::tcp::endpoint endpoint)
		:_socket(_ioService)
		{
			_socket.connect(endpoint);
		}

		void getArtists(std::vector<std::string>& artists)
		{


		}



	private:

		void sendMsg(const ::google::protobuf::Message& message)
		{

			// Serialize message
			std::string outputStr;
			if (message.SerializeToString(&outputStr))
			{
				// Send message header
				std::array<unsigned char, Remote::Header::size> headerBuffer;

				// Generate header content
				{
					Remote::Header header;
					header.setSize(outputStr.size());
					header.to_buffer(headerBuffer);
				}

				boost::asio::write(_socket, boost::asio::buffer(headerBuffer));

				// Send serialized message
				boost::asio::write(_socket, boost::asio::buffer(outputStr));
			}
			else
			{
				std::cerr << "Cannot serialize message!" << std::endl;
				throw std::runtime_error("message.SerializeToString failed!");
			}

		}

		void recvMsg(::google::protobuf::Message& message)
		{
			boost::asio::streambuf messageStreamBuffer;
			std::istream	responseStream(&response);

			boost::asio::read(_socket,
					messageStreamBuffer,
					boost::asio::transfer_exactly(Remote::Header::size));

			bool error;
			Remote::Header header (Remote::Header::from_buffer(headerBuffer, error));
			if (error)
				throw std::runtime_error("Cannot read header from buffer!");

			if (header.getSize() > _inputBuffer.size())
				throw std::runtime_error("Input buffer too small!");

			// Read in a stream buffer
			{
				boost::asio::streambuf response;
				boost::asio::read(_socket,
						response,
						boost::asio::transfer_exactly(header.getSize()));


				if (!message.ParseFromIstream(&responseStream))
					throw std::runtime_error("message.ParseFromIstream failed!");
			}
		}

		boost::asio::io_service		_ioService;
		boost::asio::ip::tcp::socket	_socket;

		boost::streambuf		_inputStreamBuf;

		std::array<unsigned char, 65536> _inputBuffer;

};


int main(int argc, char* argv[])
{
	try {

		// Runs in its own thread
		// Listen on any
		TestServer	testServer( boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), 5080));

		// Client
		// connect to loopback
		TestClient	client( boost::asio::ip::tcp::endpoint( boost::asio::ip::address_v4::loopback(), 5080));

		// TODO get some data
		std::vector<std::string>	artists;
		client.getArtists(artists);

		testServer.stop();

	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


