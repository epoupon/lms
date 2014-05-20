#include <iostream>
#include <stdexcept>
#include <thread>

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/foreach.hpp>

#include "remote/server/Server.hpp"
#include "remote/messages/Header.hpp"
#include "remote/messages/messages.pb.h"

#include "TestDatabase.hpp"

struct GenreInfo
{
	uint64_t	id;
	std::string	name;
};

std::ostream& operator<<(std::ostream& os, const GenreInfo& info)
{
	os << "id = " << info.id << ", name = '" << info.name << "'" << std::endl;
	return os;
}

struct ArtistInfo
{
	uint64_t	id;
	std::string	name;
};

std::ostream& operator<<(std::ostream& os, const ArtistInfo& info)
{
	os << "id = " << info.id << ", name = '" << info.name << "'" << std::endl;
	return os;
}

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

		void getArtists(std::vector<ArtistInfo>& artists)
		{

			const std::size_t requestedBatchSize = 32;
			std::size_t offset = 0;
			std::size_t res = 0;


			while ((res = getArtists(artists, offset, requestedBatchSize) ) > 0)
				offset += res;

		}

		std::size_t getArtists(std::vector<ArtistInfo>& artists, std::size_t offset, std::size_t size)
		{
			std::size_t nbArtists = 0;

			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage_Type_AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest_Type_TypeGetArtistList);
			request.mutable_audio_collection_request()->mutable_get_artists()->mutable_batch_parameter()->set_size(size);
			request.mutable_audio_collection_request()->mutable_get_artists()->mutable_batch_parameter()->set_offset(offset);

			sendMsg(request);

			// Receive responses
			Remote::ServerMessage response;
			recvMsg(response);

			// Process message
			if (!response.has_audio_collection_response())
				throw std::runtime_error("not an audio_collection_response!");

			if (!response.audio_collection_response().has_artist_list())
				throw std::runtime_error("not an artist_list!");

			for (int i = 0; i < response.audio_collection_response().artist_list().artists_size(); ++i)
			{
				if (!response.audio_collection_response().artist_list().artists(i).has_name())
					throw std::runtime_error("no artist name!");

				ArtistInfo artist;
				artist.id = response.audio_collection_response().artist_list().artists(i).id();
				artist.name = response.audio_collection_response().artist_list().artists(i).name();

				artists.push_back( artist );
				nbArtists++;
			}

			return nbArtists;
		}

		void getGenres(std::vector<GenreInfo>& genres)
		{

			const std::size_t requestedBatchSize = 8;
			std::size_t offset = 0;
			std::size_t res = 0;

			while ((res = getGenres(genres, offset, requestedBatchSize) ) > 0)
				offset += res;

		}

		std::size_t getGenres(std::vector<GenreInfo>& genres, std::size_t offset, std::size_t size)
		{
			std::size_t nbAdded = 0;

			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage_Type_AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest_Type_TypeGetGenreList);
			request.mutable_audio_collection_request()->mutable_get_genres()->mutable_batch_parameter()->set_size(size);
			request.mutable_audio_collection_request()->mutable_get_genres()->mutable_batch_parameter()->set_offset(offset);

			sendMsg(request);

			// Receive responses
			Remote::ServerMessage response;
			recvMsg(response);

			// Process message
			if (!response.has_audio_collection_response())
				throw std::runtime_error("not an audio_collection_response!");

			if (!response.audio_collection_response().has_genre_list())
				throw std::runtime_error("not an genre_list!");

			for (int i = 0; i < response.audio_collection_response().genre_list().genres_size(); ++i)
			{
				if (!response.audio_collection_response().genre_list().genres(i).has_name())
					throw std::runtime_error("no genre name!");

				GenreInfo genre;
				genre.id = response.audio_collection_response().genre_list().genres(i).id();
				genre.name = response.audio_collection_response().genre_list().genres(i).name();

				genres.push_back( genre );
				nbAdded++;
			}


			return nbAdded;
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


int main()
{
	try {

		// Runs in its own thread
		// Listen on any
		TestServer	testServer( boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), 5080));

		// Client
		// connect to loopback
		TestClient	client( boost::asio::ip::tcp::endpoint( boost::asio::ip::address_v4::loopback(), 5080));

		// Get Artists
		std::vector<ArtistInfo>	artists;
		client.getArtists(artists);

		// Dump artists
		std::cout << "Got " << artists.size() << " artists!" << std::endl;
		BOOST_FOREACH(const ArtistInfo& artist, artists)
			std::cout << "Artist: '" << artist << "'" << std::endl;

		// Get genres
		std::vector<GenreInfo>	genres;
		client.getGenres(genres);

		// Dum genres
		std::cout << "Got " << genres.size() << " genres!" << std::endl;
		BOOST_FOREACH(const GenreInfo& genre, genres)
			std::cout << "Genre: '" << genre << "'" << std::endl;

		testServer.stop();

	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


