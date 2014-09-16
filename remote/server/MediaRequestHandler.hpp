#ifndef REMOTE_MEDIA_REQUEST_HANDLER
#define REMOTE_MEDIA_REQUEST_HANDLER

#include <map>
#include <memory>

#include "messages/media.pb.h"

#include "database/DatabaseHandler.hpp"
#include "transcode/AvConvTranscoder.hpp"

namespace Remote {
namespace Server {

class MediaRequestHandler
{
	public:
		MediaRequestHandler(Database::Handler& db);

		bool process(const MediaRequest& request, MediaResponse& response);

	private:

		bool processAudioPrepare(const MediaRequest::Prepare::Audio& request, MediaResponse::PrepareResult& response);
		bool processGetPart(const MediaRequest::GetPart& request, MediaResponse::PartResult& response);
		bool processTerminate(const MediaRequest::Terminate& request, MediaResponse::TerminateResult& response);

//		bool processVideoPrepare(const AudioCollectionRequest::GetGenreList& request, AudioCollectionResponse::GenreList& response);

		std::map<uint32_t, std::shared_ptr<Transcode::AvConvTranscoder> > _transcoders;

		Database::Handler& _db;

		uint32_t _curHandle = 0;

		static const std::size_t _maxPartSize = 65536 - 128;
		static const std::size_t _maxTranscoders = 1;
};

} // namespace Remote
} // namespace Server

#endif

