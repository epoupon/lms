#include "MediaRequestHandler.hpp"

#include "database/AudioTypes.hpp"

namespace Remote {
namespace Server {

MediaRequestHandler::MediaRequestHandler(DatabaseHandler& db)
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
					res = processAudioPrepare(request.prepare().audio(), response);
				else if (request.prepare().has_video())
					;// TODO;
				else
					std::cerr << "Bad MediaRequest::TypeMediaPrepare!" << std::endl;

			}
			else
				std::cerr << "Bad MediaRequest::TypeMediaPrepare!" << std::endl;
			break;

		case MediaRequest::TypeMediaGetPart:
			if (request.has_get_part())
				res = processGetPart(request.get_part(), response);
			else
				std::cerr << "Bad MediaRequest::TypeMediaGet!" << std::endl;
			break;

		case MediaRequest::TypeMediaTerminate:
			if (request.has_terminate())
				res = processTerminate(request.terminate(), response);
			else
				std::cerr << "Bad MediaRequest::TypeMediaTerminate!" << std::endl;
			break;

		default:
			std::cerr << "Unhandled MediaRequest type = " << request.type() << std::endl;
	}

	return res;
}

bool
MediaRequestHandler::processAudioPrepare(const MediaRequest::Prepare::Audio& request, MediaResponse& response)
{
	// TODO, get user default values
	Transcode::Format::Encoding	format	= Transcode::Format::OGA;
	std::size_t			bitrate	= 128000;

	if (request.has_codec_type())
	{
		switch( request.codec_type())
		{
			case AudioCodecType::CodecTypeOGA:
				format = Transcode::Format::OGA;
				break;
			default:
				std::cerr << "Unhandled codec type = " << request.codec_type() << std::endl;
				return false;
		}
	}
	if (request.has_bitrate())
		bitrate = request.bitrate();

	if (_transcoder)
	{
		response.mutable_error()->set_error(true);
		response.mutable_error()->set_message("Transcode already in progress");
		response.set_type(MediaResponse::TypeError);
		return true;
	}

	Wt::Dbo::Transaction transaction( _db.getSession());

	Track::pointer track = Track::getById( _db.getSession(), request.track_id() );

	if (!track)
	{
		response.mutable_error()->set_error(true);
		response.mutable_error()->set_message("Cannot find requested track!");
		response.set_type(MediaResponse::TypeError);
		return true;
	}

	try
	{
		Transcode::InputMediaFile inputFile(track->getPath());
		Transcode::Parameters parameters(inputFile, Transcode::Format::get( format ), bitrate);

		_transcoder = std::make_shared<Transcode::AvConvTranscoder>( parameters );

		response.mutable_error()->set_error(false);
		response.mutable_error()->set_message("");
		response.set_type(MediaResponse::TypeError);
	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		response.mutable_error()->set_error(true);
		response.mutable_error()->set_message("exception: " + std::string(e.what()));
		response.set_type(MediaResponse::TypeError);
	}

	return true;
}

bool
MediaRequestHandler::processGetPart(const MediaRequest::GetPart& request, MediaResponse& response)
{
	std::size_t dataSize = request.requested_data_size();
	if (dataSize > _maxPartSize)
		dataSize = _maxPartSize;

	if (!_transcoder)
	{
		response.mutable_error()->set_error(true);
		response.mutable_error()->set_message("No transcoder set!");
		response.set_type(MediaResponse::TypeError);
		return true;
	}

	while (!_transcoder->isComplete() && _transcoder->getOutputData().size() < dataSize)
		_transcoder->process();

	std::cout << "MediaRequestHandler::processGetPart, isComplete = " << std::boolalpha << _transcoder->isComplete() << ", size = " << _transcoder->getOutputData().size() << std::endl;

	Transcode::AvConvTranscoder::data_type::iterator itEnd;
	if (_transcoder->getOutputData().size() > dataSize)
		itEnd = _transcoder->getOutputData().begin() + dataSize;
	else
		itEnd = _transcoder->getOutputData().end();

	response.set_type(MediaResponse::TypePart);
	std::copy(_transcoder->getOutputData().begin(), itEnd, std::back_inserter(*response.mutable_part()->mutable_data()));

	// Consume sent bytes
	_transcoder->getOutputData().erase(_transcoder->getOutputData().begin(), itEnd);

	return true;
}


bool
MediaRequestHandler::processTerminate(const MediaRequest::Terminate& /*request*/, MediaResponse& response)
{
	std::cout << "MediaRequestHandler: resetting transcoder" << std::endl;
	_transcoder.reset();

	assert(!_transcoder);

	response.mutable_error()->set_error(false);
	response.mutable_error()->set_message("");
	response.set_type(MediaResponse::TypeError);

	return true;
}



} // namespace Remote
} // namespace Server


