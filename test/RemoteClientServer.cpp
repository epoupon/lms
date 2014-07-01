#include <iostream>
#include <stdexcept>
#include <thread>
#include <fstream>

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
};

std::ostream& operator<<(std::ostream& os, const ReleaseInfo& info)
{
	os << "id = " << info.id << ", name = '" << info.name;
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

struct Cover
{
	std::string 			mimeType;
	std::vector<unsigned char>	data;
};

// Ugly class for testing purposes

class TestClient
{
	public:
		TestClient(boost::asio::ip::tcp::endpoint endpoint)
		: _context(boost::asio::ssl::context::sslv23),		// be large on this
		  _socket(_ioService, _context)
		{
			boost::asio::ip::tcp::resolver resolver(_ioService);

			_socket.set_verify_mode(boost::asio::ssl::verify_peer);
			_socket.set_verify_callback(boost::bind(&TestClient::verifyCertificate, this, _1, _2));

			boost::asio::connect(_socket.lowest_layer(), resolver.resolve(endpoint));

			_socket.handshake(boost::asio::ssl::stream_base::client);
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

		void getReleases(std::vector<ReleaseInfo>& releases, const std::vector<uint64_t> artistIds)
		{

			const std::size_t requestedBatchSize = 256;
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
			if (!response.audio_collection_response().has_type())
				throw std::runtime_error("Missing type!");

//			if (!response.audio_collection_response().type() != Remote::ServerMessage::AudioCollectionResponse::TypeReleaseList)
//				throw std::runtime_error("Bad type!");

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

				releases.push_back( release );
				nbAdded++;
			}
			return nbAdded;
		}

		void getTracks(std::vector<TrackInfo>& tracks, const std::vector<uint64_t> artistIds, const std::vector<uint64_t> releaseIds, const std::vector<uint64_t> genreIds)
		{
			const std::size_t requestedBatchSize = 0;
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


		void getMediaAudio(uint64_t audioId, std::vector<unsigned char>& data)
		{

			// Prepare
			mediaAudioPrepare( audioId );

			// Get while available
			mediaGet( data );

			// Terminate
			mediaTerminate();
		}

		void getCoverTrack(std::vector<Cover>& coverArt, uint64_t trackId)
		{
			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage::AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest::TypeGetCoverArt);
			request.mutable_audio_collection_request()->mutable_get_cover_art()->set_type( Remote::AudioCollectionRequest::GetCoverArt::TypeGetCoverArtTrack);
			request.mutable_audio_collection_request()->mutable_get_cover_art()->set_track_id( trackId );
			request.mutable_audio_collection_request()->mutable_get_cover_art()->set_size( 256 );
			sendMsg(request);

			// Receive responses
			Remote::ServerMessage response;
			recvMsg(response);

			// Process message
			if (!response.has_audio_collection_response())
				throw std::runtime_error("not an audio_collection_response!");

			for (int i = 0; i < response.audio_collection_response().cover_art_size(); ++i)
			{
				Cover cover;
				cover.mimeType = response.audio_collection_response().cover_art(i).mime_type();
				cover.data.assign(response.audio_collection_response().cover_art(i).data().begin(), response.audio_collection_response().cover_art(i).data().end());;

				coverArt.push_back(cover);
			}
		}

		void getCoverRelease(std::vector<Cover>& coverArt, uint64_t releaseId)
		{
			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage::AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest::TypeGetCoverArt);
			request.mutable_audio_collection_request()->mutable_get_cover_art()->set_type( Remote::AudioCollectionRequest::GetCoverArt::TypeGetCoverArtRelease);
			request.mutable_audio_collection_request()->mutable_get_cover_art()->set_release_id( releaseId );
			request.mutable_audio_collection_request()->mutable_get_cover_art()->set_size( 256 );

			sendMsg(request);

			// Receive responses
			Remote::ServerMessage response;
			recvMsg(response);

			// Process message
			if (!response.has_audio_collection_response())
				throw std::runtime_error("not an audio_collection_response!");

			for (int i = 0; i < response.audio_collection_response().cover_art_size(); ++i)
			{
				Cover cover;
				cover.mimeType = response.audio_collection_response().cover_art(i).mime_type();
				cover.data.assign(response.audio_collection_response().cover_art(i).data().begin(), response.audio_collection_response().cover_art(i).data().end());;

				coverArt.push_back(cover);
			}

		}


	private:

		bool verifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx)
		{
			// In this example we will simply print the certificate's subject name.
			std::array<char, 256> subject_name;

			X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
			X509_NAME_oneline(X509_get_subject_name(cert), subject_name.data(), subject_name.size());

			std::cout << "Verifying '" << std::string(subject_name.data()) << "', preverified = " << std::boolalpha << preverified << std::endl;

			return true; // preverified;
		}

		void mediaAudioPrepare(uint64_t audioId)
		{

			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage_Type_MediaRequest);

			request.mutable_media_request()->set_type( Remote::MediaRequest_Type_TypeMediaPrepare);
			request.mutable_media_request()->mutable_prepare()->set_type(Remote::MediaRequest_Prepare_Type_AudioRequest);

			// Set fields
			request.mutable_media_request()->mutable_prepare()->mutable_audio()->set_track_id( audioId );

			std::cout << "Sending prepare request" << std::endl;
			sendMsg(request);

			std::cout << "Waiting for response" << std::endl;
			// Receive response
			Remote::ServerMessage response;
			recvMsg(response);

			std::cout << "Got a response" << std::endl;

			// Process message
			if (!response.has_media_response())
				throw std::runtime_error("Preapre: not an media reponse!");

			if (!response.media_response().has_error())
				throw std::runtime_error("Prepare: not an error msg!");

			if (response.media_response().error().error())
				throw std::runtime_error("Prepare failed: '" + response.media_response().error().message() + "'");

		}

		void mediaGet(std::vector<unsigned char>& data)
		{
			while (mediaGetPart(data) > 0)
				;
		}


		std::size_t mediaGetPart(std::vector<unsigned char>& data)
		{
			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage::MediaRequest);

			request.mutable_media_request()->set_type( Remote::MediaRequest::TypeMediaGetPart);
			request.mutable_media_request()->mutable_get_part()->set_requested_data_size(65536);

			std::cout << "Sending GetPart request" << std::endl;
			sendMsg(request);

			std::cout << "Waiting for response" << std::endl;
			// Receive response
			Remote::ServerMessage response;
			recvMsg(response);

			std::cout << "Got a response" << std::endl;

			if (!response.has_media_response())
				throw std::runtime_error("not an media response");

			// Process message
			switch( response.media_response().type())
			{
				case Remote::MediaResponse::TypeError:
					{
						std::string msg;
						if (response.media_response().has_error())
						{
							if (response.media_response().error().has_message())
								throw std::runtime_error("Error spotted: '" + response.media_response().error().message()+ "'");
							else
								throw std::runtime_error("Error spotted, no msg");
						}
						else
							throw std::runtime_error("Error spotted but no error message");
					}
					break;

				case Remote::MediaResponse::TypePart:
					if (!response.media_response().has_part())
						throw std::runtime_error("not an media part");

					std::copy(response.media_response().part().data().begin(), response.media_response().part().data().end(), std::back_inserter(data));

					return response.media_response().part().data().size();
					break;

				default:
					throw std::runtime_error("Unhandled response.media_response().type()!");
			}

			return 0;
		}

		void mediaTerminate(void)
		{
			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage::MediaRequest);

			request.mutable_media_request()->set_type( Remote::MediaRequest::TypeMediaTerminate);
			request.mutable_media_request()->mutable_terminate();


			sendMsg(request);

			// Receive response
			Remote::ServerMessage response;
			recvMsg(response);


			// Process message
			if (!response.has_media_response())
				throw std::runtime_error("Terminate: not an media response");

			if (!response.media_response().has_error())
				throw std::runtime_error("Terminate: not an error msg!");

			if (response.media_response().error().error())
				throw std::runtime_error("Terminate failed: '" + response.media_response().error().message() + "'");

		}


		void sendMsg(const ::google::protobuf::Message& message)
		{
			std::ostream os(&_outputStreamBuf);

			// Serialize message
			if (message.SerializeToOstream(&os))
			{

				if (_outputStreamBuf.size() > Remote::Header::max_data_size)
				{
					std::ostringstream oss; oss << "Message too big = " << _outputStreamBuf.size() << " bytes! (max is " << Remote::Header::max_data_size << ")" << std::endl;
					throw std::runtime_error("Message to big!");
				}

				// Send message header
				std::array<unsigned char, Remote::Header::size> headerBuffer;

				// Generate header content
				{
					Remote::Header header;
					header.setDataSize(_outputStreamBuf.size());
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
				boost::asio::streambuf::mutable_buffers_type bufs = _inputStreamBuf.prepare(header.getDataSize());

				std::size_t n = boost::asio::read(_socket,
									bufs,
									boost::asio::transfer_exactly(header.getDataSize()));

				assert(n == header.getDataSize());
				_inputStreamBuf.commit(n);

				if (!message.ParseFromIstream(&is))
					throw std::runtime_error("message.ParseFromIstream failed!");

			}
		}

		typedef   boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

		boost::asio::io_service		_ioService;
		boost::asio::ssl::context	_context;
		ssl_socket			_socket;

		boost::asio::streambuf	_inputStreamBuf;
		boost::asio::streambuf	_outputStreamBuf;

};


int main()
{
	try {
		bool extendedTests = true;
		bool writeCovers = false;

		std::cout << "Running test... extendedTests = " << std::boolalpha << extendedTests << std::endl;

		// Client
		// connect to loopback
		TestClient	client( boost::asio::ip::tcp::endpoint( boost::asio::ip::address_v4::loopback(), 5080));

		// ****** Artists *********
		std::vector<ArtistInfo>	artists;
		client.getArtists(artists);

		std::cout << "Got " << artists.size() << " artists!" << std::endl;
		BOOST_FOREACH(const ArtistInfo& artist, artists)
			std::cout << "Artist: '" << artist << "'" << std::endl;

		// ***** Genres *********
		std::vector<GenreInfo>	genres;
		client.getGenres(genres);

		std::cout << "Got " << genres.size() << " genres!" << std::endl;
		BOOST_FOREACH(const GenreInfo& genre, genres)
			std::cout << "Genre: '" << genre << "'" << std::endl;

		// **** Releases ******
		std::vector<ReleaseInfo> releases;
		client.getReleases(releases, std::vector<uint64_t>());
		BOOST_FOREACH(const ReleaseInfo& release, releases)
			std::cout << "Release: '" << release << "'" << std::endl;

		// **** Tracks ******
		std::vector<TrackInfo> tracks;
		client.getTracks(tracks, std::vector<uint64_t>(), std::vector<uint64_t>(), std::vector<uint64_t>());
		BOOST_FOREACH(const TrackInfo& track, tracks)
			std::cout << "Track: '" << track << "'" << std::endl;

		// Caution: long test!
/*		if (extendedTests)
		{
			BOOST_FOREACH(const ArtistInfo& artist, artists)
			{
				// Get the tracks for each artist
				std::vector<TrackInfo> tracks;
				client.getTracks(tracks, std::vector<uint64_t>(1, artist.id), std::vector<uint64_t>(), std::vector<uint64_t>());

				std::cout << "Artist '" << artist.name << "', nb tracks = " << tracks.size() << std::endl;
				BOOST_FOREACH(const TrackInfo& track, tracks)
					std::cout << "Track: '" << track << "'" << std::endl;
			}
		}*/

		// ***** Covers *******
		if (extendedTests)
		{
			BOOST_FOREACH(const ReleaseInfo& release, releases)
			{
				std::vector<Cover> coverArts;
				client.getCoverRelease(coverArts, release.id);

				if (writeCovers)
				{
					boost::filesystem::create_directory("cover");
					BOOST_FOREACH(const Cover coverArt, coverArts)
					{
						std::ostringstream oss; oss << "cover/" << release.id << "." << release.name << ".jpeg";
						std::ofstream out(oss.str().c_str());
						BOOST_FOREACH(unsigned char c, coverArt.data)
							out.put(c);
					}
				}

				std::cout << "Release '" << release << "', spotted " << coverArts.size() << " covers!" << std::endl;
			}

			BOOST_FOREACH(const TrackInfo& track, tracks)
			{
				std::vector<Cover> coverArt;
				client.getCoverTrack(coverArt, track.id);

				std::cout << "Track '" << track << "', spotted " << coverArt.size() << " covers!" << std::endl;
			}

		}

		// ****** Transcode test ********
		{
			std::vector<unsigned char> data;
			client.getMediaAudio(1, data);

			std::cout << "Media size = " << data.size() << std::endl;
		}
		{
			std::vector<unsigned char> data;
			client.getMediaAudio(100, data);

			std::cout << "Media size = " << data.size() << std::endl;
		}


		std::cout << "End of tests!" << std::endl;
	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << "Normal quit..." << std::endl;
	return EXIT_SUCCESS;
}


