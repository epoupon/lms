/*
 * Copyright (C) 2015 Emeric Poupon
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
#include "database/DatabaseHandler.hpp"
#include "transcode/Format.hpp"
#include "transcode/InputMediaFile.hpp"
#include "transcode/Parameters.hpp"
#include "transcode/AvConvTranscoder.hpp"

#include "Resource.hpp"


namespace RestAPI {

Resource::Resource(boost::filesystem::path dbPath)
: _dbPath(dbPath)
{

}

void
Resource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
	static const std::size_t _bufferSize = 65536;
	LMS_LOG(MOD_REST_API, SEV_DEBUG) << "Handle request...";

	// see if this request is for a continuation:
	Wt::Http::ResponseContinuation *continuation = request.continuation();

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Handling request. Continuation = " << std::boolalpha << continuation;

	std::shared_ptr<Transcode::AvConvTranscoder> transcoder;
	if (continuation)
	{
		transcoder = boost::any_cast<std::shared_ptr<Transcode::AvConvTranscoder> >(continuation->data());
	}
	else
	{
		const Wt::Http::ParameterMap& parameters = request.getParameterMap();

		for (auto itParameter : parameters)
		{
			LMS_LOG(MOD_REST_API, SEV_DEBUG) << "Param name: '" << itParameter.first << "'";

			for (auto value : itParameter.second)
			{
				LMS_LOG(MOD_REST_API, SEV_DEBUG) << "\tvalue: '" << value << "'";
			}
		}

		auto itParamMediaId = parameters.find("mediaid");
		if (itParamMediaId == parameters.end())
		{
			LMS_LOG(MOD_REST_API, SEV_DEBUG) << "Cannot find parameter mediaid";
			return;
		}

		std::string mediaId = itParamMediaId->second.front();
		LMS_LOG(MOD_REST_API, SEV_DEBUG) << "MediaId = " << mediaId;

		try
		{
			Database::Handler db(_dbPath);

			Wt::Dbo::Transaction transaction(db.getSession());

			Database::Track::pointer track = Database::Track::getById(db.getSession(), std::stol(mediaId));

			LMS_LOG(MOD_UI, SEV_DEBUG) << "Launching transcoder";
			Transcode::InputMediaFile input(track->getPath());
			Transcode::Parameters parameters(input, Transcode::Format::get(Transcode::Format::OGA));

			transcoder = std::make_shared<Transcode::AvConvTranscoder>(parameters);

			LMS_LOG(MOD_UI, SEV_DEBUG) << "Mime type set to '" << parameters.getOutputFormat().getMimeType() << "'";
			response.setMimeType(parameters.getOutputFormat().getMimeType());

		}
		catch(std::exception &e)
		{
			LMS_LOG(MOD_UI, SEV_DEBUG) << "Caught exception: " << e.what();
			return;
		}
	}

	if (!transcoder)
	{
		LMS_LOG(MOD_UI, SEV_ERROR) << "No transcoder ?!";
		return;
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

} // namespace API
