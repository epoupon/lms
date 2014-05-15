
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
			// Send request
			Remote::ClientMessage msg;

			msg.set_type( Remote::ClientMessage_Type_AudioCollectionRequest );

			msg.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest_Type_TypeGetArtistList);
			msg.mutable_audio_collection_request()->mutable_get_artists()->set_preferred_batch_size(64);

			sendMsg(msg);

			// Receive responses
			Remote::ServerMessage response;
			recvMsg(response);

			// 

		}



	private:

		void sendMsg(const ::google::protobuf::Message& message)
		{
			std::ostream os(&_outputStreamBuf);

			// Serialize message
			if (message.SerializeToOstream(&os))
			{
				// Send message header
				std::array<unsigned char, Remote::Header::size> headerBuffer;

				// Generate header content
				{
					Remote::Header header;
					header.setSize(_outputStreamBuf.size());
					header.to_buffer(headerBuffer);
				}

				boost::asio::write(_socket, boost::asio::buffer(headerBuffer));

				// Send serialized message
				std::size_t n = boost::asio::write(_socket,
									_outputStreamBuf.data(),
									boost::asio::transfer_exactly(_outputStreamBuf.size()));
				assert(n == _outputStreamBuf.size());

				_outputStreamBuf.consume(n);

				std::cout << "Client: Message sent!" << std::endl;
			}
			else
			{
				std::cerr << "Cannot serialize message!" << std::endl;
				throw std::runtime_error("message.SerializeToString failed!");
			}

		}

		void recvMsg(::google::protobuf::Message& message)
		{
			std::istream is(&_inputStreamBuf);

			{
				// reserve bytes in output sequence
				boost::asio::streambuf::mutable_buffers_type bufs = _inputStreamBuf.prepare(Remote::Header::size);

				std::cout << "Client: waiting for header message!" << std::endl;
				std::size_t n = boost::asio::read(_socket,
									bufs,
									boost::asio::transfer_exactly(Remote::Header::size));

				assert(n == Remote::Header::size);
				_inputStreamBuf.commit(n);

				std::cout << "Client: Header message received!" << std::endl;
			}

			Remote::Header header;
			if (!header.from_istream(is))
				throw std::runtime_error("Cannot read header from buffer!");

			// Read in a stream buffer
			{
				// reserve bytes in output sequence
				boost::asio::streambuf::mutable_buffers_type bufs = _inputStreamBuf.prepare(header.getSize());

				std::cout << "Client: waiting for message!" << std::endl;
				std::size_t n = boost::asio::read(_socket,
									bufs,
									boost::asio::transfer_exactly(header.getSize()));

				assert(n == header.getSize());
				_inputStreamBuf.commit(n);

				std::cout << "Client: message received!" << std::endl;

				if (!message.ParseFromIstream(&is))
					throw std::runtime_error("message.ParseFromIstream failed!");

			}
		}

		boost::asio::io_service		_ioService;
		boost::asio::ip::tcp::socket	_socket;

		boost::asio::streambuf _inputStreamBuf;
		boost::asio::streambuf _outputStreamBuf;

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


