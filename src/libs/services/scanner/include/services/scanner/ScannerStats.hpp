/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <Wt/WDateTime.h>

#include <filesystem>
#include <vector>

#include "services/database/TrackId.hpp"

namespace Scanner
{
	enum class ScanErrorType
	{
		CannotReadFile,			// cannot read file
		CannotParseFile,		// cannot parse file
		NoAudioTrack,			// no audio track found
		BadDuration,			// bad duration
	};

	enum class DuplicateReason
	{
		SameHash,
		SameTrackMBID,
	};

	struct ScanError
	{
		std::filesystem::path	file;
		ScanErrorType		error;
		std::string		systemError;

		ScanError(const std::filesystem::path& file, ScanErrorType error, const std::string& systemError = "");
	};

	struct ScanDuplicate
	{
		Database::TrackId	trackId;
		DuplicateReason		reason;
	};

	enum class ScanStep
	{
		DiscoveringFiles,
		ScanningFiles,
		ChekingForMissingFiles,
		CheckingForDuplicateFiles,
		FetchingTrackFeatures,
		ReloadingSimilarityEngine,
	};
	static inline constexpr unsigned ScanProgressStepCount {5};

	// reduced scan stats
	struct ScanStepStats
	{
		Wt::WDateTime   startTime;

		ScanStep currentStep;

		std::size_t	totalElems {};
		std::size_t	processedElems {};

		unsigned		progress() const;
	};

	struct ScanStats
	{
		Wt::WDateTime	startTime;
		Wt::WDateTime	stopTime;

		std::size_t	filesScanned {};	// Total number of files scanned (estimated)

		std::size_t	skips {};			// no change since last scan
		std::size_t	scans {};			// actually scanned filed

		std::size_t	additions {};		// added in DB
		std::size_t	deletions {};		// removed from DB
		std::size_t	updates {};			// updated file in DB

		std::size_t	featuresFetched {};	// features fetched in DB

		std::vector<ScanError>		errors;
		std::vector<ScanDuplicate>	duplicates;

		std::size_t	nbFiles() const;
		std::size_t	nbChanges() const;
	};
} // namespace Scanner

