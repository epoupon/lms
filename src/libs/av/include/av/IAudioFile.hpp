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

/* This file contains some classes in order to get info from file using the libavconv */

#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

#include "Types.hpp"

namespace Av
{
	struct Picture
	{
		std::string			mimeType;
		const std::byte*	data {};
		std::size_t			dataSize {};
	};

	struct StreamInfo
	{
		size_t			index {};
		std::size_t     bitrate {};
		std::string 	codec;
	};

	class IAudioFile
	{
		public:
			virtual ~IAudioFile() = default;

			using MetadataMap = std::unordered_map<std::string, std::string>;

			virtual const std::filesystem::path& getPath() const = 0;
			virtual std::chrono::milliseconds	getDuration() const = 0;
			virtual MetadataMap					getMetaData() const = 0;
			virtual std::vector<StreamInfo>		getStreamInfo() const = 0;
			virtual std::optional<StreamInfo>	getBestStreamInfo() const = 0; // none if failure/unknown
			virtual std::optional<std::size_t>	getBestStreamIndex() const = 0; // none if failure/unknown
			virtual bool 						hasAttachedPictures() const = 0;
			virtual void 						visitAttachedPictures(std::function<void(const Picture&)> func) const = 0;
	};

	std::unique_ptr<IAudioFile> parseAudioFile(const std::filesystem::path& p);

	struct AudioFileFormat
	{
		std::string mimeType;
		std::string format;
	};

	std::optional<AudioFileFormat> guessAudioFileFormat(const std::filesystem::path& file);

} // namespace Av

