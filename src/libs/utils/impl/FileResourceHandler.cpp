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

#include "FileResourceHandler.hpp"

#include <fstream>

#include "utils/Logger.hpp"

std::unique_ptr<IResourceHandler>
createFileResourceHandler(const std::filesystem::path& path)
{
	return std::make_unique<FileResourceHandler>(path);
}


FileResourceHandler::FileResourceHandler(const std::filesystem::path& path)
: _path {path}
{
}


Wt::Http::ResponseContinuation*
FileResourceHandler::processRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
	::uint64_t startByte {_offset};
	std::ifstream ifs {_path.string().c_str(), std::ios::in | std::ios::binary};

	if (startByte == 0)
	{
		if (!ifs)
		{
			LMS_LOG(UTILS, ERROR) << "Cannot open file stream for '" << _path.string() << "'";
			response.setStatus(404);
			return {};
		}
		else
		{
			response.setStatus(200);
		}

		ifs.seekg(0, std::ios::end);
		const ::uint64_t fileSize {static_cast<::uint64_t>(ifs.tellg())};
		ifs.seekg(0, std::ios::beg);

		LMS_LOG(UTILS, DEBUG) << "File '" << _path.string() << "', fileSize = " << fileSize;

		const Wt::Http::Request::ByteRangeSpecifier ranges {request.getRanges(fileSize)};
		if (!ranges.isSatisfiable())
		{
			std::ostringstream contentRange;
			contentRange << "bytes */" << fileSize;
			response.setStatus(416); // Requested range not satisfiable
			response.addHeader("Content-Range", contentRange.str());

			LMS_LOG(UTILS, DEBUG) << "Range not satisfiable";
			return {};
		}

		if (ranges.size() == 1)
		{
			LMS_LOG(UTILS, DEBUG) << "Range requested = " << ranges[0].firstByte() << "/" << ranges[0].lastByte();

			response.setStatus(206);
			startByte = ranges[0].firstByte();
			_beyondLastByte = ranges[0].lastByte() + 1;

			std::ostringstream contentRange;
			contentRange << "bytes " << startByte << "-"
				<< _beyondLastByte - 1 << "/" << fileSize;

			response.addHeader("Content-Range", contentRange.str());
			response.setContentLength(_beyondLastByte - startByte);
		}
		else
		{
			LMS_LOG(UTILS, DEBUG) << "No range requested";

			_beyondLastByte = fileSize;
			response.setContentLength(_beyondLastByte);
		}
	}
	else if (!ifs)
	{
		LMS_LOG(UTILS, ERROR) << "Cannot reopen file stream for '" << _path.string() << "'";
		return {};
	}

	ifs.seekg(static_cast<std::istream::pos_type>(startByte));

	std::vector<char> buf;
	buf.resize(_chunkSize);

	::uint64_t restSize = _beyondLastByte - startByte;
	::uint64_t pieceSize = buf.size() > restSize ? restSize : buf.size();

	ifs.read(&buf[0], pieceSize);
	const ::uint64_t actualPieceSize {static_cast<::uint64_t>(ifs.gcount())};
	response.out().write(&buf[0], actualPieceSize);

	LMS_LOG(UTILS, DEBUG) << "Written " << actualPieceSize << " bytes";

	LMS_LOG(UTILS, DEBUG) << "Progress: " << actualPieceSize << "/" << restSize;
	if (ifs.good() && actualPieceSize < restSize)
	{
		_offset = startByte + actualPieceSize;
		LMS_LOG(UTILS, DEBUG) << "Job not complete! Next chunk offset = " << _offset;

		return response.createContinuation();
	}

	LMS_LOG(UTILS, DEBUG) << "Job complete!";
	return nullptr;
}

