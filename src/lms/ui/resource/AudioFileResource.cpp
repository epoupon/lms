/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "AudioFileResource.hpp"

#include <fstream>
#include <Wt/Http/Response.h>

#include "av/AvInfo.hpp"
#include "database/Track.hpp"
#include "utils/Logger.hpp"
#include "utils/String.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

#define LOG(level)	LMS_LOG(UI, level) << "Audio file resource: "

AudioFileResource:: ~AudioFileResource()
{
	beingDeleted();
}

std::string
AudioFileResource::getUrl(Database::IdType trackId) const
{
	return url()+ "&trackid=" + std::to_string(trackId);
}

static
std::optional<std::filesystem::path>
getTrackPathFromTrackId(Database::IdType trackId)
{
	// DbSession are not thread safe
	Wt::WApplication::UpdateLock lock {LmsApp};

	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::Track::pointer track {Database::Track::getById(LmsApp->getDbSession(), trackId)};
	if (!track)
	{
		LOG(ERROR) << "Missing track";
		return std::nullopt;
	}

	return track->getPath();
}

static
std::optional<std::filesystem::path>
getTrackPathFromURLArgs(const Wt::Http::Request& request)
{
	const std::string* trackIdParameter {request.getParameter("trackid")};
	if (!trackIdParameter)
	{
		LOG(ERROR) << "Missing trackid URL parameter!";
		return std::nullopt;
	}

	const auto trackId {StringUtils::readAs<Database::IdType>(*trackIdParameter)};
	if (!trackId)
	{
		LOG(ERROR) << "Bad trackid URL parameter!";
		return std::nullopt;
	}

	return getTrackPathFromTrackId(*trackId);
}

void
AudioFileResource::handleRequest(const Wt::Http::Request& request,
		Wt::Http::Response& response)
{
	if (!request.continuation())
	{
		LOG(DEBUG) << "Initial request";

		auto trackPath {getTrackPathFromURLArgs(request)};
		if (!trackPath)
			return;

		ContinuationData continuationData;
		continuationData.path = *trackPath;
		continuationData.offset = 0;
		continuationData.beyondLastByte = 0;

		{
			std::error_code ec;
			continuationData.fileSize = std::filesystem::file_size(*trackPath, ec);

			if (ec)
			{
				LOG(ERROR) << "Cannot get file size for '" << *trackPath << "': " << ec.message();
				return;
			}
		}

		LOG(DEBUG) << "Initial request. File = '" << continuationData.path << "', size = " << continuationData.fileSize;

		handleRequestPiecewise(request, response, continuationData);

		const auto fileFormat {Av::guessMediaFileFormat(*trackPath)};
		const std::string mimeType {fileFormat ? fileFormat->mimeType : "application/octet-stream"};
		response.setMimeType(mimeType);

		LOG(DEBUG) << "Mime type set to '" << mimeType << "'";
	}
	else
	{
		ContinuationData continuationData {Wt::cpp17::any_cast<ContinuationData>(request.continuation()->data())};
		handleRequestPiecewise(request, response, continuationData);
	}

}


void
AudioFileResource::handleRequestPiecewise(const Wt::Http::Request& request,
		Wt::Http::Response& response,
		ContinuationData continuationData)
{
	LOG(DEBUG) << "Handling request. File = '" << continuationData.path << "', size = " << continuationData.fileSize << ", offset = " << continuationData.offset << ", beyondLastByte = " << continuationData.beyondLastByte;

	::uint64_t startByte {continuationData.offset};
	std::ifstream ifs {continuationData.path.string().c_str(), std::ios::in | std::ios::binary};

	if (startByte == 0)
	{
		if (!ifs)
		{
			response.setStatus(404);
			return;
		}
		else
		{
			response.setStatus(200);
		}

		const Wt::Http::Request::ByteRangeSpecifier ranges {request.getRanges(continuationData.fileSize)};
		if (!ranges.isSatisfiable())
		{
			std::ostringstream contentRange;
			contentRange << "bytes */" << continuationData.fileSize;
			response.setStatus(416); // Requested range not satisfiable
			response.addHeader("Content-Range", contentRange.str());
			return;
		}

		if (ranges.size() == 1)
		{
			response.setStatus(206);
			startByte = ranges[0].firstByte();
			continuationData.beyondLastByte = ranges[0].lastByte() + 1;

			std::ostringstream contentRange;
			contentRange << "bytes " << startByte << "-"
				<< continuationData.beyondLastByte - 1 << "/" << continuationData.fileSize;

			response.addHeader("Content-Range", contentRange.str());
			response.setContentLength(continuationData.beyondLastByte - startByte);
		}
		else
		{
			continuationData.beyondLastByte = continuationData.fileSize;
			response.setContentLength(continuationData.beyondLastByte);
		}
	}

	ifs.seekg(static_cast<std::istream::pos_type>(startByte));

	std::vector<char> buf;
	buf.resize(_chunkSize);

	::uint64_t restSize = continuationData.beyondLastByte - startByte;
	::uint64_t pieceSize = buf.size() > restSize ? restSize : buf.size();

	ifs.read(&buf[0], pieceSize);
	const ::uint64_t actualPieceSize {static_cast<::uint64_t>(ifs.gcount())};
	response.out().write(&buf[0], actualPieceSize);

	LOG(DEBUG) << "Written " << actualPieceSize << " bytes!";

	if (ifs.good() && actualPieceSize < restSize)
	{
		LOG(DEBUG) << "Still more to do";

		auto* continuation {response.createContinuation()};
		ContinuationData newContinuationData {continuationData};
		newContinuationData.offset = startByte + actualPieceSize;
		continuation->setData(newContinuationData);
	}
}


} // namespace UserInterface


