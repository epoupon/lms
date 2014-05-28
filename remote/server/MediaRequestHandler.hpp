#ifndef REMOTE_MEDIA_REQUEST_HANDLER
#define REMOTE_MEDIA_REQUEST_HANDLER

#include <memory>

#include "messages/media.pb.h"

#include "database/DatabaseHandler.hpp"
#include "transcode/AvConvTranscoder.hpp"

namespace Remote {
namespace Server {

class MediaRequestHandler
{
	public:
		MediaRequestHandler(DatabaseHandler& db);

		bool process(const MediaRequest& request, MediaResponse& response);

	private:

		bool processAudioPrepare(const MediaRequest::Prepare::Audio& request, MediaResponse& response);
		bool processGetPart(const MediaRequest::GetPart& request, MediaResponse& response);
		bool processTerminate(const MediaRequest::Terminate& request, MediaResponse& response);

//		bool processVideoPrepare(const AudioCollectionRequest::GetGenreList& request, AudioCollectionResponse::GenreList& response);

		std::shared_ptr<Transcode::AvConvTranscoder>	_transcoder;

		DatabaseHandler& _db;

		static const std::size_t _maxPartSize = 65536 - 128;
};

} // namespace Remote
} // namespace Server

#endif

