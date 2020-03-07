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

#include "utils/FileResourceHandler.hpp"

#include <fstream>

#include "utils/Logger.hpp"

namespace FileResourceHandler
{

static constexpr std::size_t _chunkSize {262144};

static
std::optional<ContinuationData>
handleRequestPiecewise(const Wt::Http::Request& request,
				Wt::Http::Response& response,
				ContinuationData continuationData)
{
	::uint64_t startByte {continuationData.offset};
	std::ifstream ifs {continuationData.path.string().c_str(), std::ios::in | std::ios::binary};

	LMS_LOG(UTILS, DEBUG) << "startByte = " << startByte;

	if (startByte == 0)
	{
		if (!ifs)
		{
			LMS_LOG(UTILS, ERROR) << "Cannot open file stream for '" << continuationData.path.string() << "'";
			response.setStatus(404);
			return std::nullopt;
		}
		else
		{
			response.setStatus(200);
		}

		ifs.seekg(0, std::ios::end);
		const ::uint64_t fileSize {static_cast<::uint64_t>(ifs.tellg())};
		ifs.seekg(0, std::ios::beg);

		LMS_LOG(UTILS, DEBUG) << "fileSize = " << fileSize;

		const Wt::Http::Request::ByteRangeSpecifier ranges {request.getRanges(fileSize)};
		if (!ranges.isSatisfiable())
		{
			std::ostringstream contentRange;
			contentRange << "bytes */" << fileSize;
			response.setStatus(416); // Requested range not satisfiable
			response.addHeader("Content-Range", contentRange.str());

			LMS_LOG(UTILS, DEBUG) << "Range not satisfiable";
			return std::nullopt;
		}

		if (ranges.size() == 1)
		{
			LMS_LOG(UTILS, DEBUG) << "Range requested = " << ranges[0].firstByte() << "/" << ranges[0].lastByte();

			response.setStatus(206);
			startByte = ranges[0].firstByte();
			continuationData.beyondLastByte = ranges[0].lastByte() + 1;

			std::ostringstream contentRange;
			contentRange << "bytes " << startByte << "-"
				<< continuationData.beyondLastByte - 1 << "/" << fileSize;

			response.addHeader("Content-Range", contentRange.str());
			response.setContentLength(continuationData.beyondLastByte - startByte);
		}
		else
		{
			LMS_LOG(UTILS, DEBUG) << "No range requested";

			continuationData.beyondLastByte = fileSize;
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

	LMS_LOG(UTILS, DEBUG) << "Written " << actualPieceSize << " bytes";

	LMS_LOG(UTILS, DEBUG) << "Progress: " << actualPieceSize << "/" << restSize;
	if (ifs.good() && actualPieceSize < restSize)
	{
		ContinuationData newContinuationData {continuationData};
		newContinuationData.offset = startByte + actualPieceSize;

		LMS_LOG(UTILS, DEBUG) << "Job not complete! Next chunk offset = " << newContinuationData.offset;

		return newContinuationData;
	}
	else
	{
		LMS_LOG(UTILS, DEBUG) << "Job complete!";
	}

	return std::nullopt;
}


std::optional<ContinuationData>
handleInitialRequest(const Wt::Http::Request& request,
			Wt::Http::Response& response,
			const std::filesystem::path& path)
{
	ContinuationData continuationData;
	continuationData.path = path;
	continuationData.offset = 0;
	continuationData.beyondLastByte = 0;

	LMS_LOG(UTILS, DEBUG) << "Initial request for file '" << path << "'";

	return handleRequestPiecewise(request, response, continuationData);
}

std::optional<ContinuationData>
handleContinuationRequest(const Wt::Http::Request& request,
			Wt::Http::Response& response,
			const ContinuationData& continuationData)
{
	LMS_LOG(UTILS, DEBUG) << "Continuation request for file '" << continuationData.path << "', offset = " << continuationData.offset;
	return handleRequestPiecewise(request, response, continuationData);
}

} // ns FileResourceHandler



