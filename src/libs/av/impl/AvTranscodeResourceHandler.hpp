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

#pragma once

#include <filesystem>
#include "av/AvTranscoder.hpp"
#include "utils/IResourceHandler.hpp"

namespace Av
{

	class TranscodeResourceHandler final : public IResourceHandler
	{
		public:
			TranscodeResourceHandler(const std::filesystem::path& trackPath, const TranscodeParameters& parameters);

		private:

			void processRequest(const Wt::Http::Request& request, Wt::Http::Response& reponse) override;
			bool isFinished() const override;

			static constexpr std::size_t _chunkSize {262144};
			const std::filesystem::path _trackPath;
			Transcoder _transcoder;
	};
}

