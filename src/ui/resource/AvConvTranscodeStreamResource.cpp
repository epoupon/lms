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


#include <Wt/Http/Request>
#include <Wt/Http/Response>

#include "logger/Logger.hpp"

#include "AvConvTranscodeStreamResource.hpp"

namespace UserInterface {

AvConvTranscodeStreamResource::AvConvTranscodeStreamResource(const Transcode::Parameters& parameters, Wt::WObject *parent)
: Wt::WResource(parent),
	_parameters( parameters )
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "CONSTRUCTING RESOURCE";
}

AvConvTranscodeStreamResource::~AvConvTranscodeStreamResource()
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "DESTRUCTING RESOURCE";
	beingDeleted();
}

void
AvConvTranscodeStreamResource::handleRequest(const Wt::Http::Request& request,
		Wt::Http::Response& response)
{
	// see if this request is for a continuation:
	Wt::Http::ResponseContinuation *continuation = request.continuation();

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Handling new request. Continuation = " << continuation;

	std::shared_ptr<Transcode::AvConvTranscoder> transcoder;
	if (continuation)
		transcoder = boost::any_cast<std::shared_ptr<Transcode::AvConvTranscoder> >(continuation->data());

	if (!transcoder)
	{
		LMS_LOG(MOD_UI, SEV_DEBUG) << "Launching transcoder";
		transcoder = std::make_shared<Transcode::AvConvTranscoder>( _parameters);

		LMS_LOG(MOD_UI, SEV_DEBUG) << "Mime type set to '" << _parameters.getOutputFormat().getMimeType() << "'";
		response.setMimeType(_parameters.getOutputFormat().getMimeType());
	}

	if (!transcoder->isComplete())
	{
		std::vector<unsigned char> data;
		data.reserve(_bufferSize);

		transcoder->process(data, _bufferSize);

		// Give the client all the output data
		response.out().write(reinterpret_cast<char*>(&data[0]), data.size());

		LMS_LOG(MOD_UI, SEV_DEBUG) << "Written " << data.size() << " bytes! complete = " << std::boolalpha << transcoder->isComplete() << ", produced bytes = " << transcoder->getOutputBytes();

		if (!response.out())
			LMS_LOG(MOD_UI, SEV_ERROR) << "Write failed!";
	}

	if (!transcoder->isComplete() && response.out()) {
		continuation = response.createContinuation();
		continuation->setData(transcoder);
	}
	else
		LMS_LOG(MOD_UI, SEV_DEBUG) << "No more data!";
}

} // namespace UserInterface


