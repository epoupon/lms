#include "logger/Logger.hpp"

#include "MediaRequestHandler.hpp"

#include "database/AudioTypes.hpp"

namespace Remote {
namespace Server {

MediaRequestHandler::MediaRequestHandler(Database::Handler& db)
: _db(db)
{}

bool
MediaRequestHandler::process(const MediaRequest& request, MediaResponse& response)
{

	bool res = false;

	switch (request.type())
	{
		case MediaRequest::TypeMediaPrepare:
			if (request.has_prepare())
			{
				if (request.prepare().has_audio())
					res = processAudioPrepare(request.prepare().audio(), *response.mutable_prepare_result());
				else if (request.prepare().has_video())
					LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Video prepare not supported!";
				else
					LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad MediaRequest::TypeMediaPrepare!";

				if (res)
					response.set_type(MediaResponse::TypePrepareResult);
			}
			else
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad MediaRequest::TypeMediaPrepare!";
			break;

		case MediaRequest::TypeMediaGetPart:
			if (request.has_get_part())
			{
				res = processGetPart(request.get_part(), *response.mutable_part_result());
				if (res)
					response.set_type(MediaResponse::TypePartResult);
			}
			else
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad MediaRequest::TypeMediaGet!";

			break;

		case MediaRequest::TypeMediaTerminate:
			if (request.has_terminate())
			{
				res = processTerminate(request.terminate(), *response.mutable_terminate_result());
				if (res)
					response.set_type(MediaResponse::TypePartResult);
			}
			else
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad MediaRequest::TypeMediaTerminate!";

			break;

		default:
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Unhandled MediaRequest type = " << request.type();
	}

	return res;
}

bool
MediaRequestHandler::processAudioPrepare(const MediaRequest::Prepare::Audio& request, MediaResponse::PrepareResult& response)
{
	std::size_t			bitrate;
	Transcode::Format::Encoding	format;

	switch( request.codec_type())
	{
		case MediaRequest::Prepare::AudioCodecTypeOGA:	format = Transcode::Format::OGA; break;
		default:
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Unhandled codec type = " << request.codec_type();
			return false;
	}

	switch( request.bitrate() )
	{
		case MediaRequest::Prepare::AudioBitrate_32_kbps: bitrate = 32000; break;
		case MediaRequest::Prepare::AudioBitrate_64_kbps: bitrate = 64000; break;
		case MediaRequest::Prepare::AudioBitrate_96_kbps: bitrate = 96000; break;
		case MediaRequest::Prepare::AudioBitrate_128_kbps: bitrate = 128000; break;
		case MediaRequest::Prepare::AudioBitrate_192_kbps: bitrate = 192000; break;
		case MediaRequest::Prepare::AudioBitrate_256_kbps: bitrate = 256000; break;
		default:
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Unhandled bitrate type = " << request.bitrate();
			return false;
	}

	// TODO use user's bitrate limits!
	// TODO limit transcoder number by user?

	if (_transcoders.size() + 1 > _maxTranscoders)
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Transcoder limit reached!" << std::endl;
		// Just answer an empty response, dont delete existing trasncode jobs
		return true;
	}

	try
	{
		Wt::Dbo::Transaction transaction( _db.getSession());

		Database::Track::pointer track = Database::Track::getById( _db.getSession(), request.track_id() );
		if (!track)
		{
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Requested track does not exist" << std::endl;
			// Track does no longer exist, just answer an empty response
			return true;
		}

		Transcode::InputMediaFile inputFile(track->getPath());
		Transcode::Parameters parameters(inputFile, Transcode::Format::get( format ));
		parameters.setBitrate( Transcode::Stream::Audio, bitrate);

		std::shared_ptr<Transcode::AvConvTranscoder> transcoder = std::make_shared<Transcode::AvConvTranscoder>( parameters );

		// now get a unique id (relative to this connection!)
		uint32_t handle = _curHandle++;
		assert(_transcoders.find(handle) == _transcoders.end());

		_transcoders[handle] = transcoder;

		response.set_handle(handle);

		LMS_LOG(MOD_REMOTE, SEV_DEBUG) << "Set up new transcode, handle = " << handle;
	}
	catch(std::exception& e)
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Caught exception: " << e.what();
		return false;
	}

	return true;
}

bool
MediaRequestHandler::processGetPart(const MediaRequest::GetPart& request, MediaResponse::PartResult& response)
{
	std::size_t	dataSize = request.requested_data_size();
	if (dataSize > _maxPartSize)
		dataSize = _maxPartSize;

	if (_transcoders.find(request.handle()) == _transcoders.end())
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "No transcoder found for handle " << request.handle();
		return true;
	}

	std::shared_ptr<Transcode::AvConvTranscoder> transcoder = _transcoders[request.handle()];

	while (!transcoder->isComplete() && transcoder->getOutputData().size() < dataSize)
		transcoder->process();

	LMS_LOG(MOD_REMOTE, SEV_DEBUG) << "MediaRequestHandler::processGetPart, handle = " << request.handle() << ", isComplete = " << std::boolalpha << transcoder->isComplete() << ", size = " << transcoder->getOutputData().size();

	Transcode::AvConvTranscoder::data_type::iterator itEnd;
	if (transcoder->getOutputData().size() > dataSize)
		itEnd = transcoder->getOutputData().begin() + dataSize;
	else
		itEnd = transcoder->getOutputData().end();

	std::copy(transcoder->getOutputData().begin(), itEnd, std::back_inserter(*response.mutable_data()));

	// Consume sent bytes
	transcoder->getOutputData().erase(transcoder->getOutputData().begin(), itEnd);

	return true;
}


bool
MediaRequestHandler::processTerminate(const MediaRequest::Terminate& request, MediaResponse::TerminateResult& response)
{
	LMS_LOG(MOD_REMOTE, SEV_DEBUG) << "MediaRequestHandler: resetting transcoder for handle " << request.handle();

	if (_transcoders.find(request.handle()) == _transcoders.end())
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "No transcoder found for handle " << request.handle();
		return true;
	}
	else
	{
		_transcoders.erase(request.handle());
	}

	return true;
}



} // namespace Remote
} // namespace Server


