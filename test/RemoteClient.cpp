/*
 * Copyright (C) 2013 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <stdexcept>
#include <thread>
#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/foreach.hpp>

#include "remote/messages/Header.hpp"
#include "messages.pb.h"

#include "TestDatabase.hpp"

struct GenreInfo
{
	uint64_t        id;
	std::string	name;

	GenreInfo() : id(0) {}
};

std::ostream& operator<<(std::ostream& os, const GenreInfo& info)
{
	os << "name = '" << info.name << "'(" << info.id << ")";
	return os;
}

struct ArtistInfo
{
	uint64_t        id;
	std::string	mbid;
	std::string	name;

	ArtistInfo() :id(0) {}
};

std::ostream& operator<<(std::ostream& os, const ArtistInfo& info)
{
	os << "name = '" << info.name << "'(" << info.id << ")";
	return os;
}

struct ReleaseInfo
{
	uint64_t        id;
	std::string	mbid;
	std::string	name;

	ReleaseInfo() : id(0) {}
};

std::ostream& operator<<(std::ostream& os, const ReleaseInfo& info)
{
	os << "name = '" << info.name << "'(" << info.id << ")";
	return os;
}

struct TrackInfo
{
	uint64_t        id;
	std::string	mbid;

	uint64_t        release_id;
	uint64_t	artist_id;
	std::vector<uint64_t>	genre_id;

	uint32_t	disc_number;
	uint32_t	track_number;

	std::string	name;

	boost::posix_time::time_duration duration;

	std::string	date;
	std::string	original_date;

	TrackInfo(): id(0), release_id(0), artist_id(0), disc_number(0), track_number(0) {}
};

std::ostream& operator<<(std::ostream& os, const TrackInfo& info)
{
	os << "id = " << info.id << ", name = '" << info.name << "', track_number = " << info.track_number << ", duration = " << info.duration;
	if (!info.date.empty())
		os << ", date = " << info.date;
	if (!info.original_date.empty())
		os << ", original date = " << info.original_date;

	return os;
}

struct Cover
{
	std::string 			mimeType;
	std::vector<unsigned char>	data;
};

struct SearchFilter
{
	std::vector<uint64_t> artistIds;
	std::vector<uint64_t> genreIds;
	std::vector<uint64_t> releaseIds;
	std::vector<uint64_t> trackIds;
};

void SearchFilterToRequest(const SearchFilter& filter, Remote::AudioCollectionRequest_SearchFilter& request)
{
	for (uint64_t id : filter.artistIds)
		request.add_artist_id(id);

	for (uint64_t id : filter.genreIds)
		request.add_genre_id(id);

	for (uint64_t id : filter.releaseIds)
		request.add_release_id(id);

	for (uint64_t id : filter.trackIds)
		request.add_track_id(id);
}

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

		void getArtists(std::vector<ArtistInfo>& artists, const SearchFilter& filter = SearchFilter())
		{

			const std::size_t requestedBatchSize = 128;
			std::size_t offset = 0;
			std::size_t res = 0;


			while ((res = getArtists(artists, filter, offset, requestedBatchSize) ) > 0)
				offset += res;

		}

		std::size_t getArtists(std::vector<ArtistInfo>& artists, const SearchFilter& filter, std::size_t offset, std::size_t size)
		{
			std::size_t nbArtists = 0;

			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage_Type_AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest_Type_TypeGetArtistList);
			request.mutable_audio_collection_request()->mutable_get_artists()->mutable_batch_parameter()->set_size(size);
			request.mutable_audio_collection_request()->mutable_get_artists()->mutable_batch_parameter()->set_offset(offset);
			SearchFilterToRequest(filter, *request.mutable_audio_collection_request()->mutable_get_artists()->mutable_search_filter());

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
				const Remote::AudioCollectionResponse_Artist& respArtist =  response.audio_collection_response().artist_list().artists(i);

				if (!respArtist.has_id())
					throw std::runtime_error("no id!");
				if (!respArtist.has_name())
					throw std::runtime_error("no artist name!");

				ArtistInfo artist;
				artist.name = respArtist.name();
				artist.id = respArtist.id();
				if (respArtist.has_mbid())
					artist.mbid = respArtist.mbid();

				artists.push_back( artist );
				nbArtists++;
			}

			return nbArtists;
		}

		void getGenres(std::vector<GenreInfo>& genres, const SearchFilter& filter = SearchFilter())
		{

			const std::size_t requestedBatchSize = 8;
			std::size_t offset = 0;
			std::size_t res = 0;

			while ((res = getGenres(genres, filter, offset, requestedBatchSize) ) > 0)
				offset += res;

		}

		std::size_t getGenres(std::vector<GenreInfo>& genres, const SearchFilter& filter, std::size_t offset, std::size_t size)
		{
			std::size_t nbAdded = 0;

			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage_Type_AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest_Type_TypeGetGenreList);
			request.mutable_audio_collection_request()->mutable_get_genres()->mutable_batch_parameter()->set_size(size);
			request.mutable_audio_collection_request()->mutable_get_genres()->mutable_batch_parameter()->set_offset(offset);
			SearchFilterToRequest(filter, *request.mutable_audio_collection_request()->mutable_get_genres()->mutable_search_filter());

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
				const Remote::AudioCollectionResponse_Genre& respGenre =  response.audio_collection_response().genre_list().genres(i);

				if (!respGenre.has_id())
					throw std::runtime_error("no genre id!");
				if (!respGenre.has_name())
					throw std::runtime_error("no genre name!");

				GenreInfo genre;
				genre.id = respGenre.id();
				genre.name = respGenre.name();

				genres.push_back( genre );
				nbAdded++;
			}
			return nbAdded;
		}

		void getReleases(std::vector<ReleaseInfo>& releases, const SearchFilter& filter = SearchFilter())
		{

			const std::size_t requestedBatchSize = 256;
			std::size_t offset = 0;
			std::size_t res = 0;

			while ((res = getReleases(releases, filter, offset, requestedBatchSize) ) > 0)
				offset += res;

		}

		std::size_t getReleases(std::vector<ReleaseInfo>& releases, const SearchFilter& filter, std::size_t offset, std::size_t size)
		{
			std::size_t nbAdded = 0;

			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage_Type_AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest_Type_TypeGetReleaseList);
			request.mutable_audio_collection_request()->mutable_get_releases()->mutable_batch_parameter()->set_size(size);
			request.mutable_audio_collection_request()->mutable_get_releases()->mutable_batch_parameter()->set_offset(offset);
			SearchFilterToRequest(filter, *request.mutable_audio_collection_request()->mutable_get_releases()->mutable_search_filter());

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
				const Remote::AudioCollectionResponse_Release& respRelease =  response.audio_collection_response().release_list().releases(i);

				if (!respRelease.has_id())
					throw std::runtime_error("no id!");
				if (!respRelease.has_name())
					throw std::runtime_error("no release name!");

				ReleaseInfo release;
				release.id = respRelease.id();
				release.name = respRelease.name();

				releases.push_back( release );
				nbAdded++;
			}
			return nbAdded;
		}

		void getTracks(std::vector<TrackInfo>& tracks, const SearchFilter& filter = SearchFilter())
		{
			const std::size_t requestedBatchSize = 0;
			std::size_t offset = 0;
			std::size_t res = 0;

			while ((res = getTracks(tracks, filter, offset, requestedBatchSize) ) > 0)
				offset += res;
		}

		std::size_t getTracks(std::vector<TrackInfo>& tracks, const SearchFilter& filter, std::size_t offset, std::size_t size)
		{
			std::size_t nbAdded = 0;

			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage_Type_AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest_Type_TypeGetTrackList);
			request.mutable_audio_collection_request()->mutable_get_tracks()->mutable_batch_parameter()->set_size(size);
			request.mutable_audio_collection_request()->mutable_get_tracks()->mutable_batch_parameter()->set_offset(offset);
			SearchFilterToRequest(filter, *request.mutable_audio_collection_request()->mutable_get_tracks()->mutable_search_filter());

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
				const Remote::AudioCollectionResponse_Track& respTrack = response.audio_collection_response().track_list().tracks(i);;

				TrackInfo track;
				track.id = respTrack.id();
				if (respTrack.has_mbid())
					track.mbid = respTrack.mbid();

				track.name = respTrack.name();
				track.duration = boost::posix_time::seconds(respTrack.duration_secs());

				if (respTrack.has_track_number())
					track.track_number = respTrack.track_number();

				if (respTrack.has_disc_number())
					track.disc_number = respTrack.disc_number();

				if (respTrack.has_release_date())
					track.date = respTrack.release_date();

				if (respTrack.has_original_release_date())
					track.original_date = respTrack.original_release_date();


				tracks.push_back( track );
				nbAdded++;
			}

			return nbAdded;
		}


		void getMediaAudio(uint64_t audioId, std::vector<unsigned char>& data)
		{

			// Prepare
			uint32_t handle = mediaAudioPrepare( audioId );

			// Get while available
			mediaGet( handle, data );

			// Terminate
			mediaTerminate( handle );
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

		std::string getRevision(void)
		{
			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage::AudioCollectionRequest );

			request.mutable_audio_collection_request()->set_type( Remote::AudioCollectionRequest::TypeGetRevision);

			sendMsg(request);

			// Receive responses
			Remote::ServerMessage response;
			recvMsg(response);

			// Process message
			if (!response.has_audio_collection_response())
				throw std::runtime_error("not an audio_collection_response!");

			if (!response.audio_collection_response().has_revision())
				throw std::runtime_error("not a revision!");

			return response.audio_collection_response().revision().rev();
		}

		bool login(const std::string& username, const std::string& password)
		{
			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage::AuthRequest );

			request.mutable_auth_request()->set_type( Remote::AuthRequest::TypePassword);
			request.mutable_auth_request()->mutable_password()->set_user_login( username );
			request.mutable_auth_request()->mutable_password()->set_user_password( password );

			sendMsg(request);

			// Receive responses
			Remote::ServerMessage response;
			recvMsg(response);

			// Process message
			if (!response.has_auth_response())
				throw std::runtime_error("not an auth response!");

			if (!response.auth_response().has_password_result())
				throw std::runtime_error("not a password result!");

			switch( response.auth_response().password_result().type())
			{
				case Remote::AuthResponse::PasswordResult::TypePasswordValid:
					return true;
				case Remote::AuthResponse::PasswordResult::TypePasswordInvalid:
					return false;
				case Remote::AuthResponse::PasswordResult::TypeLoginThrottling:
					if (response.auth_response().password_result().has_delay())
						std::cerr << "Has to wait for " << response.auth_response().password_result().delay()  << " seconds" << std::endl;

					return false;
				default:
					throw std::runtime_error("bad password result type");
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

		uint32_t mediaAudioPrepare(uint64_t audioId)
		{

			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage_Type_MediaRequest);

			request.mutable_media_request()->set_type( Remote::MediaRequest_Type_TypeMediaPrepare);
			request.mutable_media_request()->mutable_prepare()->set_type(Remote::MediaRequest_Prepare_Type_AudioRequest);

			// Set fields
			request.mutable_media_request()->mutable_prepare()->mutable_audio()->set_track_id( audioId );
			request.mutable_media_request()->mutable_prepare()->mutable_audio()->set_codec_type( Remote::MediaRequest::Prepare::AudioCodecTypeOGA );
			request.mutable_media_request()->mutable_prepare()->mutable_audio()->set_bitrate( Remote::MediaRequest::Prepare::AudioBitrate_64_kbps );

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

			if (!response.media_response().has_prepare_result())
				throw std::runtime_error("Prepare: not an prepare result msg!");

			if (!response.media_response().prepare_result().has_handle())
				throw std::runtime_error("Prepare: cannot get handle");

			return response.media_response().prepare_result().handle();
		}

		void mediaGet(uint32_t handle, std::vector<unsigned char>& data)
		{
			while (mediaGetPart(handle, data) > 0)
				;
		}


		std::size_t mediaGetPart(uint32_t handle, std::vector<unsigned char>& data)
		{
			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage::MediaRequest);

			request.mutable_media_request()->set_type( Remote::MediaRequest::TypeMediaGetPart);
			request.mutable_media_request()->mutable_get_part()->set_handle(handle);
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
			if (response.media_response().type() != Remote::MediaResponse::TypePartResult)
				throw std::runtime_error("GetPart: not a Part response!");

			if (!response.media_response().has_part_result())
				throw std::runtime_error("GetPart: does not have a Part result!");

			std::copy(response.media_response().part_result().data().begin(), response.media_response().part_result().data().end(), std::back_inserter(data));

			return response.media_response().part_result().data().size();
		}

		void mediaTerminate( uint32_t handle )
		{
			// Send request
			Remote::ClientMessage request;

			request.set_type( Remote::ClientMessage::MediaRequest);

			request.mutable_media_request()->set_type( Remote::MediaRequest::TypeMediaTerminate);
			request.mutable_media_request()->mutable_terminate()->set_handle(handle);

			sendMsg(request);

			// Receive response
			Remote::ServerMessage response;
			recvMsg(response);


			// Process message
			if (!response.has_media_response())
				throw std::runtime_error("Terminate: not an media response");

			if (!response.media_response().has_terminate_result())
				throw std::runtime_error("Terminate: not an terminate msg!");

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

enum class Test {
	ArtistFilters,
	ReleaseFilterArtist,
	ReleaseFilterGenre,
	TrackFilters,
	CoverByTrack,
	CoverByRelease,
	Transcode,
};

/* Uncomment test to be disabled */
std::set<Test>	tests = {
	Test::ArtistFilters,
	Test::ReleaseFilterArtist,
	Test::ReleaseFilterGenre,
	Test::TrackFilters,
//	Test::CoverByTrack,
	Test::CoverByRelease,
//	Test::Transcode,
};

bool test(Test t)
{
	return (tests.find(t) != tests.end());
}

int main()
{
	try {
		bool writeCovers = false;

		std::cout << "Running test..." << std::endl;

		// Client
		// connect to loopback TODO parametrize
		TestClient	client( boost::asio::ip::tcp::endpoint( boost::asio::ip::address_v4::loopback(), 5080));

		// Use a dumb account in order to login TODO parametrize
		if (!client.login("admin", "totoadmin"))
			throw std::runtime_error("login failed!");

		// **** REVISION ***
		std::cout << "Getting revision..." << std::endl;
		std::string rev = client.getRevision();
		std::cout << "Revision '" << rev << "'" << std::endl;

		// ****** Artists *********
		std::cout << "Getting artists..." << std::endl;
		std::vector<ArtistInfo>	artists;
		client.getArtists(artists);

		std::cout << "Got " << artists.size() << " artists!" << std::endl;
		for (const ArtistInfo& artist : artists)
			std::cout << "Artist: '" << artist << "'" << std::endl;

		// ***** Genres *********
		std::cout << "Getting genres..." << std::endl;
		std::vector<GenreInfo>	genres;
		client.getGenres(genres);

		std::cout << "Got " << genres.size() << " genres!" << std::endl;
		for (const GenreInfo& genre : genres)
			std::cout << "Genre: '" << genre << "'" << std::endl;

		// **** Releases ******
		std::cout << "Getting releases..." << std::endl;
		std::vector<ReleaseInfo> releases;
		client.getReleases(releases);
		for (const ReleaseInfo& release : releases)
			std::cout << "Release: '" << release << "'" << std::endl;

		// **** Tracks ******
		std::cout << "Getting tracks..." << std::endl;
		std::vector<TrackInfo> tracks;
		client.getTracks(tracks);

		std::cout << "Got " << tracks.size() << " tracks!" << std::endl;
		for (const TrackInfo& track : tracks)
			std::cout << "Track: '" << track << "'" << std::endl;

		// Caution: long test!
		if (test(Test::ArtistFilters))
		{
			std::cout << "Getting artist for each genre..." << std::endl;
			// Get the artists for each genre
			for (const GenreInfo& genre : genres)
			{
				std::cout << "Getting artists from genre '" << genre.name << "'... ";
				SearchFilter filter;
				filter.genreIds.push_back(genre.id);

				std::vector<ArtistInfo> artists;
				client.getArtists(artists, filter);

				std::cout << "Found " << artists.size() << " artists!" << std::endl;
				for (const ArtistInfo& artist : artists)
					std::cout << "Genre '" << genre.name << "' -> Artist: " << artist << std::endl;
			}
		}

		if (test(Test::ReleaseFilterArtist))
		{
			std::cout << "Getting release for each artist..." << std::endl;
			for (const ArtistInfo& artist : artists)
			{
				std::cout << "Getting release from artist '" << artist.name << "'... ";
				SearchFilter filter;
				filter.artistIds.push_back(artist.id);

				std::vector<ReleaseInfo> releases;
				client.getReleases(releases, filter);

				std::cout << "Found " << releases.size() << " releases!" << std::endl;
				for (const ReleaseInfo& release : releases)
					std::cout << "Artist '" << artist.name << "' -> Release: '" << release << "'" << std::endl;
			}
		}

		if (test(Test::ReleaseFilterGenre))
		{
			std::cout << "Getting release for each genre..." << std::endl;
			for (const GenreInfo& genre : genres)
			{
				std::cout << "Getting release from genre '" << genre.name << "'... ";
				SearchFilter filter;
				filter.genreIds.push_back(genre.id);

				std::vector<ReleaseInfo> releases;
				client.getReleases(releases, filter);

				std::cout << "Found " << releases.size() << " releases!" << std::endl;
				BOOST_FOREACH(const ReleaseInfo& release, releases)
					std::cout << "Genre '" << genre.name << "' -> Release: '" << release << "'" << std::endl;
			}
		}

		if (test(Test::TrackFilters))
		{
			std::cout << "Getting tracks for each artist..." << std::endl;
			// Get the tracks for each artist
			for (const ArtistInfo& artist : artists)
			{
				SearchFilter filter;
				filter.artistIds.push_back(artist.id);

				std::vector<TrackInfo> tracks;
				client.getTracks(tracks, filter);

				std::cout << "Artist '" << artist.name << "', nb tracks = " << tracks.size() << std::endl;
				for (const TrackInfo& track : tracks)
					std::cout << "Artist '" << artist.name << "', track: '" << track << "'" << std::endl;
			}
		}

		// ***** Covers *******
		if (test(Test::CoverByRelease))
		{
			std::cout << "Getting cover for each release..." << std::endl;
			for (const ReleaseInfo& release : releases)
			{
				std::vector<Cover> coverArts;
				client.getCoverRelease(coverArts, release.id);

				if (writeCovers)
				{
					boost::filesystem::create_directory("cover");
					for (const Cover coverArt : coverArts)
					{
						std::ostringstream oss; oss << "cover/" << release.name << ".jpeg";
						std::ofstream out(oss.str().c_str());
						for (unsigned char c : coverArt.data)
							out.put(c);
					}
				}

				std::cout << "Release '" << release << "', spotted " << coverArts.size() << " covers!" << std::endl;
			}
		}

		if (test(Test::CoverByTrack))
		{
			std::cout << "Getting cover for each track..." << std::endl;
			for (const TrackInfo& track : tracks)
			{
				std::vector<Cover> coverArt;
				client.getCoverTrack(coverArt, track.id);

				std::cout << "Track '" << track << "', spotted " << coverArt.size() << " covers!" << std::endl;
			}

		}

		if (test(Test::Transcode))
		{
			for (const TrackInfo& track : tracks)
			{
				std::vector<unsigned char> data;
				client.getMediaAudio(track.id, data);
				std::cout << "Media size = " << data.size() << std::endl;
			}
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


