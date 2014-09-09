#ifndef AVCONV_TRANSCODE_STREAM_RESOURCE_HPP
#define AVCONV_TRANSCODE_STREAM_RESOURCE_HPP

#include <iostream>
#include <memory>

#include <Wt/WResource>

#include "transcode/AvConvTranscoder.hpp"
#include "transcode/Parameters.hpp"

namespace UserInterface {

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

		static const std::size_t	_bufferSize = 8192*16;
};

} // namespace UserInterface

#endif

