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
	os << "id = " << info.id << ", name = '" << info.name << "'";
	return os;
}

struct ArtistInfo
{
	uint64_t	id;
	std::string	name;
};

std::ostream& operator<<(std::ostream& os, const ArtistInfo& info)
{
	os << "id = " << info.id << ", name = '" << info.name << "'";
	return os;
}

struct ReleaseInfo
{
	uint64_t        id;
	std::string	name;
	std::size_t	nbTracks;
	boost::posix_time::time_duration duration;
};

std::ostream& operator<<(std::ostream& os, const ReleaseInfo& info)
{
	os << "id = " << info.id << ", name = '" << info.name << "', tracks = " << info.nbTracks << ", duration = " << info.duration;
	return os;
}

struct TrackInfo
{
	uint64_t        id;

	uint64_t        release_id;
	uint64_t	artist_id;
	std::vector<uint64_t>	genre_id;

	uint32_t	disc_number;
	uint32_t	track_number;

	std::string	name;

	boost::posix_time::time_duration duration;
};

std::ostream& operator<<(std::ostream& os, const TrackInfo& info)
{
	os << "id = " << info.id << ", name = '" << info.name << "', track_number = " << info.track_number << ", duration = " << info.duration;
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

			const std::size_t requestedBatchSize = 128;
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

			const std::size_t requestedBatchSize = 128;
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

		void getReleases(std::vector<ReleaseInfo>& releases, const std::vector<uint64_t> artistIds)
		{

			const std::size_t requestedBatchSize = 128;
			std::size_t offset = 0;
			std::size_t res = 0;

			while ((res = getReleases(releases, artistIds, offset, requestedBatchSize) ) > 0)
				offset += res;

		}

		std::size_t getReleases(std::vector<ReleaseInfo>& releases, const std::vector<uint64_t> artistIds, std::size_t offset, std::size_t size)
		{
			std::size_t nbAdded = 0;

			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage_Type_AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest_Type_TypeGetReleaseList);
			request.mutable_audio_collection_request()->mutable_get_releases()->mutable_batch_parameter()->set_size(size);
			request.mutable_audio_collection_request()->mutable_get_releases()->mutable_batch_parameter()->set_offset(offset);
			BOOST_FOREACH(uint64_t artistId, artistIds)
				request.mutable_audio_collection_request()->mutable_get_releases()->add_artist_id(artistId);

			sendMsg(request);

			// Receive responses
			Remote::ServerMessage response;
			recvMsg(response);

			// Process message
			if (!response.has_audio_collection_response())
				throw std::runtime_error("not an audio_collection_response!");

			if (!response.audio_collection_response().has_release_list())
				throw std::runtime_error("not an release list!");

			for (int i = 0; i < response.audio_collection_response().release_list().releases_size(); ++i)
			{
				if (!response.audio_collection_response().release_list().releases(i).has_name())
					throw std::runtime_error("no release name!");

				ReleaseInfo release;
				release.id = response.audio_collection_response().release_list().releases(i).id();
				release.name = response.audio_collection_response().release_list().releases(i).name();
				release.duration = boost::posix_time::seconds(response.audio_collection_response().release_list().releases(i).duration_secs());
				release.nbTracks = response.audio_collection_response().release_list().releases(i).nb_tracks();

				releases.push_back( release );
				nbAdded++;
			}
			return nbAdded;
		}

		void getTracks(std::vector<TrackInfo>& tracks, const std::vector<uint64_t> artistIds, const std::vector<uint64_t> releaseIds, const std::vector<uint64_t> genreIds)
		{
			const std::size_t requestedBatchSize = 256;
			std::size_t offset = 0;
			std::size_t res = 0;

			while ((res = getTracks(tracks, artistIds, releaseIds, genreIds, offset, requestedBatchSize) ) > 0)
				offset += res;
		}

		std::size_t getTracks(std::vector<TrackInfo>& tracks,
				const std::vector<uint64_t> artistIds,
				const std::vector<uint64_t> releaseIds,
				const std::vector<uint64_t> genreIds,
				std::size_t offset,
				std::size_t size)
		{
			std::size_t nbAdded = 0;

			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage_Type_AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest_Type_TypeGetTrackList);
			request.mutable_audio_collection_request()->mutable_get_tracks()->mutable_batch_parameter()->set_size(size);
			request.mutable_audio_collection_request()->mutable_get_tracks()->mutable_batch_parameter()->set_offset(offset);

			BOOST_FOREACH(uint64_t artistId, artistIds)
				request.mutable_audio_collection_request()->mutable_get_tracks()->add_artist_id(artistId);
			BOOST_FOREACH(uint64_t releaseId, releaseIds)
				request.mutable_audio_collection_request()->mutable_get_tracks()->add_release_id(releaseId);
			BOOST_FOREACH(uint64_t genreId, genreIds)
				request.mutable_audio_collection_request()->mutable_get_tracks()->add_genre_id(genreId);

			sendMsg(request);

			// Receive responses
			Remote::ServerMessage response;
			recvMsg(response);

			// Process message
			if (!response.has_audio_collection_response())
				throw std::runtime_error("not an audio_collection_response!");

			if (!response.audio_collection_response().has_track_list())
				throw std::runtime_error("not an track list!");

			for (int i = 0; i < response.audio_collection_response().track_list().tracks_size(); ++i)
			{

				TrackInfo track;
				track.id = response.audio_collection_response().track_list().tracks(i).id();
				track.name = response.audio_collection_response().track_list().tracks(i).name();
				track.duration = boost::posix_time::seconds(response.audio_collection_response().track_list().tracks(i).duration_secs());

				tracks.push_back( track );
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

				std::size_t n = boost::asio::read(_socket,
									bufs,
									boost::asio::transfer_exactly(Remote::Header::size));

				assert(n == Remote::Header::size);
				_inputStreamBuf.commit(n);

			}

			Remote::Header header;
			if (!header.from_istream(is))
				throw std::runtime_error("Cannot read header from buffer!");

			// Read in a stream buffer
			{
				// reserve bytes in output sequence
				boost::asio::streambuf::mutable_buffers_type bufs = _inputStreamBuf.prepare(header.getSize());

				std::size_t n = boost::asio::read(_socket,
									bufs,
									boost::asio::transfer_exactly(header.getSize()));

				assert(n == header.getSize());
				_inputStreamBuf.commit(n);

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

		// Dump genres
		std::cout << "Got " << genres.size() << " genres!" << std::endl;
		BOOST_FOREACH(const GenreInfo& genre, genres)
			std::cout << "Genre: '" << genre << "'" << std::endl;

		// **** Releases ******
		std::vector<ReleaseInfo> releases;
		client.getReleases(releases, std::vector<uint64_t>(1, 1162));
		BOOST_FOREACH(const ReleaseInfo& release, releases)
			std::cout << "Release: '" << release << "'" << std::endl;

		// **** Tracks ******
/*		{
			std::vector<TrackInfo> tracks;
			client.getTracks(tracks, std::vector<uint64_t>(), std::vector<uint64_t>(), std::vector<uint64_t>());
			BOOST_FOREACH(const TrackInfo& track, tracks)
				std::cout << "Track: '" << track << "'" << std::endl;
		}*/

		BOOST_FOREACH(const ArtistInfo& artist, artists)
		{
			// Get the tracks for each artist
			std::vector<TrackInfo> tracks;
			client.getTracks(tracks, std::vector<uint64_t>(1, artist.id), std::vector<uint64_t>(), std::vector<uint64_t>());

			std::cout << "Artist '" << artist.name << "', nb tracks = " << tracks.size() << std::endl;
			BOOST_FOREACH(const TrackInfo& track, tracks)
				std::cout << "Track: '" << track << "'" << std::endl;
		}

		testServer.stop();
	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


