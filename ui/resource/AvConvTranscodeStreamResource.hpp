#ifndef AVCONV_TRANSCODE_STREAM_RESOURCE_HPP
#define AVCONV_TRANSCODE_STREAM_RESOURCE_HPP

#include <deque>
#include <iostream>
#include <memory>

#include <boost/filesystem.hpp>

#include <Wt/WResource>

#include "transcode/AvConvTranscoder.hpp"

class AvConvTranscodeStreamResource : public Wt::WResource
{
	public:
		AvConvTranscodeStreamResource(const Transcode::Parameters& parameters, Wt::WObject *parent = 0);
		~AvConvTranscodeStreamResource();

		void handleRequest(const Wt::Http::Request& request,
				Wt::Http::Response& response);

	private:
		Transcode::Parameters		_parameters;
		std::shared_ptr<Transcode::AvConvTranscoder> 	_transcoder;

		std::streamsize			_beyondLastByte;
		static const std::size_t	_bufferSize = 8192*16;
};



#endif

