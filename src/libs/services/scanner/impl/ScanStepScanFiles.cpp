/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "ScanStepScanFiles.hpp"

#include "metadata/IParser.hpp"
#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Db.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackFeatures.hpp"
#include "services/database/TrackArtistLink.hpp"
#include "utils/Exception.hpp"
#include "utils/IConfig.hpp"
#include "utils/Logger.hpp"
#include "utils/Path.hpp"

using namespace Database;

namespace
{
	Artist::pointer
	createArtist(Session& session, const MetaData::Artist& artistInfo)
	{
		Artist::pointer artist {session.create<Artist>(artistInfo.name)};

		if (artistInfo.mbid)
			artist.modify()->setMBID(*artistInfo.mbid);
		if (artistInfo.sortName)
			artist.modify()->setSortName(*artistInfo.sortName);

		return artist;
	}

	void
	updateArtistIfNeeded(Artist::pointer artist, const MetaData::Artist& artistInfo)
	{
		// Name may have been updated
		if (artist->getName() != artistInfo.name)
		{
			artist.modify()->setName(artistInfo.name);
		}

		// Sortname may have been updated
		if (artistInfo.sortName && *artistInfo.sortName != artist->getSortName() )
		{
			artist.modify()->setSortName(*artistInfo.sortName);
		}
	}

	std::vector<Artist::pointer>
	getOrCreateArtists(Session& session, const std::vector<MetaData::Artist>& artistsInfo, bool allowFallbackOnMBIDEntries)
	{
		std::vector<Artist::pointer> artists;

		for (const MetaData::Artist& artistInfo : artistsInfo)
		{
			Artist::pointer artist;

			// First try to get by MBID
			if (artistInfo.mbid)
			{
				artist = Artist::find(session, *artistInfo.mbid);
				if (!artist)
					artist = createArtist(session, artistInfo);
				else
					updateArtistIfNeeded(artist, artistInfo);

				artists.emplace_back(std::move(artist));
				continue;
			}

			// Fall back on artist name (collisions may occur)
			if (!artistInfo.name.empty())
			{
				for (const Artist::pointer& sameNamedArtist : Artist::find(session, artistInfo.name))
				{
					// Do not fallback on artist that is correctly tagged
					if (!allowFallbackOnMBIDEntries && sameNamedArtist->getMBID())
						continue;

					artist = sameNamedArtist;
					break;
				}

				// No Artist found with the same name and without MBID -> creating
				if (!artist)
					artist = createArtist(session, artistInfo);
				else
					updateArtistIfNeeded(artist, artistInfo);

				artists.emplace_back(std::move(artist));
				continue;
			}
		}

		return artists;
	}

	ReleaseTypePrimary convertReleaseTypePrimary(MetaData::Release::PrimaryType type)
	{
		switch (type)
		{
			case MetaData::Release::PrimaryType::Album:		return ReleaseTypePrimary::Album;
			case MetaData::Release::PrimaryType::Single:	return ReleaseTypePrimary::Single;
			case MetaData::Release::PrimaryType::EP:		return ReleaseTypePrimary::EP;
			case MetaData::Release::PrimaryType::Broadcast:	return ReleaseTypePrimary::Broadcast;
			case MetaData::Release::PrimaryType::Other:		return ReleaseTypePrimary::Other;
		}

		return ReleaseTypePrimary::Other;
	}

	EnumSet<ReleaseTypeSecondary> convertReleaseTypesSecondary(EnumSet<MetaData::Release::SecondaryType> types)
	{
		EnumSet<ReleaseTypeSecondary> res;

		for (MetaData::Release::SecondaryType type : types)
		{
			switch (type)
			{
				case MetaData::Release::SecondaryType::Compilation:
					res.insert(ReleaseTypeSecondary::Compilation);
					break;
				case MetaData::Release::SecondaryType::Soundtrack:
					res.insert(ReleaseTypeSecondary::Soundtrack);
					break;
				case MetaData::Release::SecondaryType::Spokenword:
					res.insert(ReleaseTypeSecondary::Spokenword);
					break;
				case MetaData::Release::SecondaryType::Interview:
					res.insert(ReleaseTypeSecondary::Interview);
					break;
				case MetaData::Release::SecondaryType::Audiobook:
					res.insert(ReleaseTypeSecondary::Audiobook);
					break;
				case MetaData::Release::SecondaryType::AudioDrama:
					res.insert(ReleaseTypeSecondary::AudioDrama);
					break;
				case MetaData::Release::SecondaryType::Live:
					res.insert(ReleaseTypeSecondary::Live);
					break;
				case MetaData::Release::SecondaryType::Remix:
					res.insert(ReleaseTypeSecondary::Remix);
					break;
				case MetaData::Release::SecondaryType::DJMix:
					res.insert(ReleaseTypeSecondary::DJMix);
					break;
				case MetaData::Release::SecondaryType::Mixtape_Street:
					res.insert(ReleaseTypeSecondary::Mixtape_Street);
					break;
				case MetaData::Release::SecondaryType::Demo:
					res.insert(ReleaseTypeSecondary::Demo);
					break;
			}
		}

		return res;
	}

	void
	updateReleaseIfNeeded(Release::pointer release, const MetaData::Release& releaseInfo)
	{
		if (release->getName() != releaseInfo.name)
			release.modify()->setName(releaseInfo.name);
		if (release->getTotalDisc() != releaseInfo.mediumCount)
			release.modify()->setTotalDisc(releaseInfo.mediumCount);
		if (releaseInfo.primaryType)
		{
			const ReleaseTypePrimary primaryType {convertReleaseTypePrimary(*releaseInfo.primaryType)};
			if (release->getPrimaryType() != primaryType)
				release.modify()->setPrimaryType(primaryType);
		}
		const EnumSet<ReleaseTypeSecondary> secondaryTypes{ convertReleaseTypesSecondary(releaseInfo.secondaryTypes) };
		if (release->getSecondaryTypes() != secondaryTypes)
			release.modify()->setSecondaryTypes(secondaryTypes);
		if (release->getArtistDisplayName() != releaseInfo.artistDisplayName)
			release.modify()->setArtistDisplayName(releaseInfo.artistDisplayName);
	}

	Release::pointer
	getOrCreateRelease(Session& session, const MetaData::Release& releaseInfo)
	{
		Release::pointer release;

		// First try to get by MBID
		if (releaseInfo.mbid)
		{
			release = Release::find(session, *releaseInfo.mbid);
			if (!release)
				release = session.create<Release>(releaseInfo.name, releaseInfo.mbid);

			updateReleaseIfNeeded(release, releaseInfo);
			return release;
		}

		// Fall back on release name (collisions may occur)
		if (!releaseInfo.name.empty())
		{
			for (const Release::pointer& sameNamedRelease : Release::find(session, releaseInfo.name))
			{
				// do not fallback on properly tagged releases
				if (sameNamedRelease->getMBID())
					continue;

				release = sameNamedRelease;
				break;
			}

			// No release found with the same name and without MBID -> creating
			if (!release)
				release = session.create<Release>(releaseInfo.name);

			updateReleaseIfNeeded(release, releaseInfo);
			return release;
		}

		return Release::pointer{};
	}

	std::vector<Cluster::pointer>
	getOrCreateClusters(Session& session, const MetaData::Tags& tags)
	{
		std::vector<Cluster::pointer> clusters;

		for (const auto& [tag, values] : tags)
		{
			auto clusterType = ClusterType::find(session, tag);
			if (!clusterType)
				continue;

			for (auto clusterName : values)
			{
				auto cluster = clusterType->getCluster(clusterName);
				if (!cluster)
					cluster = session.create<Cluster>(clusterType, clusterName);

				clusters.push_back(cluster);
			}
		}

		return clusters;
	}

	MetaData::ParserReadStyle
	getParserReadStyle()
	{
		std::string_view readStyle {Service<IConfig>::get()->getString("scanner-parser-read-style", "accurate")};

		if (readStyle == "fast")
			return MetaData::ParserReadStyle::Fast;
		else if (readStyle == "average")
			return MetaData::ParserReadStyle::Average;
		else if (readStyle == "accurate")
			return MetaData::ParserReadStyle::Accurate;

		throw LmsException {"Invalid value for 'scanner-parser-read-style'"};
	}
} // namespace

namespace Scanner
{
	ScanStepScanFiles::ScanStepScanFiles(InitParams& initParams)
		: ScanStepBase {initParams}
	, _metadataParser {MetaData::createParser(MetaData::ParserType::TagLib, getParserReadStyle())} // For now, always use TagLib
	{
	}

	void
	ScanStepScanFiles::process(ScanContext& context)
	{
		_metadataParser->setClusterTypeNames(_settings.clusterTypeNames);

		context.currentStepStats.totalElems = context.stats.filesScanned;

		PathUtils::exploreFilesRecursive(context.directory, [&](std::error_code ec, const std::filesystem::path& path)
		{
			if (_abortScan)
				return false;

			if (ec)
			{
				LMS_LOG(DBUPDATER, ERROR) << "Cannot process entry '" << path.string() << "': " << ec.message();
				context.stats.errors.emplace_back(ScanError {path, ScanErrorType::CannotReadFile, ec.message()});
			}
			else if (PathUtils::hasFileAnyExtension(path, _settings.supportedExtensions))
			{
				scanAudioFile(path, context);

				context.currentStepStats.processedElems++;
				_progressCallback(context.currentStepStats);
			}

			return true;
		}, &excludeDirFileName);
	}

	void
	ScanStepScanFiles::scanAudioFile(const std::filesystem::path& file, ScanContext& context)
	{
		ScanStats& stats {context.stats};
		Wt::WDateTime lastWriteTime;
		try
		{
			lastWriteTime = PathUtils::getLastWriteTime(file);
		}
		catch (LmsException& e)
		{
			LMS_LOG(DBUPDATER, ERROR) << e.what();
			stats.skips++;
			return;
		}

		if (!context.forceScan)
		{
			// Skip file if last write is the same
			Database::Session& dbSession {_db.getTLSSession()};
			auto transaction {_db.getTLSSession().createSharedTransaction()};

			const Track::pointer track {Track::findByPath(dbSession, file)};

			if (track && track->getLastWriteTime().toTime_t() == lastWriteTime.toTime_t()
					&& track->getScanVersion() == _settings.scanVersion)
			{
				stats.skips++;
				return;
			}
		}

		std::optional<MetaData::Track> trackInfo {_metadataParser->parse(file)};
		if (!trackInfo)
		{
			context.stats.errors.emplace_back(file, ScanErrorType::CannotParseFile);
			return;
		}

		stats.scans++;

		Database::Session& dbSession {_db.getTLSSession()};
		auto uniqueTransaction {dbSession.createUniqueTransaction()};

		Track::pointer track {Track::findByPath(dbSession, file) };

		if (trackInfo->mbid && (!track || _settings.skipDuplicateMBID))
		{
			std::vector<Track::pointer> duplicateTracks {Track::findByMBID(dbSession, *trackInfo->mbid)};

			// find for existing MBIDs as the file may have just been moved
			if (!track && duplicateTracks.size() == 1)
			{
				Track::pointer otherTrack {duplicateTracks.front()};
				std::error_code ec;
				if (!std::filesystem::exists(otherTrack->getPath(), ec))
				{
					LMS_LOG(DBUPDATER, DEBUG) << "Considering track '" << file.string() << "' moved from '" << otherTrack->getPath() << "'";
					track = otherTrack;
					track.modify()->setPath(file);
				}
			}

			// Skip duplicate track MBID
			if (_settings.skipDuplicateMBID)
			{
				for (Track::pointer otherTrack : duplicateTracks)
				{
					// Skip ourselves
					if (track && track->getId() == otherTrack->getId())
						continue;

					// Skip if duplicate files no longer in media root: as it will be removed later, we will end up with no file
					if (!PathUtils::isPathInRootPath(file, _settings.mediaDirectory, &excludeDirFileName))
						continue;

					LMS_LOG(DBUPDATER, DEBUG) << "Skipped '" << file.string() << "' (similar MBID in '" << otherTrack->getPath().string() << "')";
					// As this MBID already exists, just remove what we just scanned
					if (track)
					{
						track.remove();
						stats.deletions++;
					}
					return;
				}
			}
		}

		// We estimate this is an audio file if:
		// - we found a least one audio stream
		// - the duration is not null
		if (trackInfo->audioStreams.empty())
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Skipped '" << file.string() << "' (no audio stream found)";

			// If Track exists here, delete it!
			if (track)
			{
				track.remove();
				stats.deletions++;
			}
			stats.errors.emplace_back(ScanError {file, ScanErrorType::NoAudioTrack});
			return;
		}
		if (trackInfo->duration == std::chrono::milliseconds::zero())
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Skipped '" << file.string() << "' (duration is 0)";

			// If Track exists here, delete it!
			if (track)
			{
				track.remove();
				stats.deletions++;
			}
			stats.errors.emplace_back(ScanError {file, ScanErrorType::BadDuration});
			return;
		}

		// ***** Title
		std::string title;
		if (!trackInfo->title.empty())
			title = trackInfo->title;
		else
		{
			// TODO parse file name guess track etc.
			// For now juste use file name as title
			title = file.filename().string();
		}

		// If file already exists, update its data
		// Otherwise, create it
		if (!track)
		{
			track = dbSession.create<Track>(file);
			LMS_LOG(DBUPDATER, DEBUG) << "Adding '" << file.string() << "'";
			stats.additions++;
		}
		else
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Updating '" << file.string() << "'";

			stats.updates++;
		}

		// Track related data
		assert(track);

		track.modify()->clearArtistLinks();
		// Do not fallback on artists with the same name but having a MBID for artist and releaseArtists, as it may be corrected by properly tagging files
		for (const Artist::pointer& artist : getOrCreateArtists(dbSession, trackInfo->artists, false))
			track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, artist, TrackArtistLinkType::Artist));

		if (trackInfo->medium && trackInfo->medium->release)
		{
			for (const Artist::pointer& releaseArtist : getOrCreateArtists(dbSession, trackInfo->medium->release->artists, false))
				track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, releaseArtist, TrackArtistLinkType::ReleaseArtist));
		}

		// Allow fallbacks on artists with the same name even if they have MBID, since there is no tag to indicate the MBID of these artists
		// We could ask MusicBrainz to get all the information, but that would heavily slow down the import process
		for (const Artist::pointer& conductor : getOrCreateArtists(dbSession, trackInfo->conductorArtists, true))
			track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, conductor, TrackArtistLinkType::Conductor));

		for (const Artist::pointer& composer : getOrCreateArtists(dbSession, trackInfo->composerArtists, true))
			track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, composer, TrackArtistLinkType::Composer));

		for (const Artist::pointer& lyricist : getOrCreateArtists(dbSession, trackInfo->lyricistArtists, true))
			track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, lyricist, TrackArtistLinkType::Lyricist));

		for (const Artist::pointer& mixer : getOrCreateArtists(dbSession, trackInfo->mixerArtists, true))
			track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, mixer, TrackArtistLinkType::Mixer));

		for (const auto& [role, performers] : trackInfo->performerArtists)
		{
			for (const Artist::pointer& performer : getOrCreateArtists(dbSession, performers, true))
				track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, performer, TrackArtistLinkType::Performer, role));
		}

		for (const Artist::pointer& producer : getOrCreateArtists(dbSession, trackInfo->producerArtists, true))
			track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, producer, TrackArtistLinkType::Producer));

		for (const Artist::pointer& remixer : getOrCreateArtists(dbSession, trackInfo->remixerArtists, true))
			track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, remixer, TrackArtistLinkType::Remixer));

		track.modify()->setScanVersion(_settings.scanVersion);
		if (trackInfo->medium && trackInfo->medium->release)
			track.modify()->setRelease(getOrCreateRelease(dbSession, *trackInfo->medium->release));
		else
			track.modify()->setRelease({});
		track.modify()->setTotalTrack(trackInfo->medium ? trackInfo->medium->trackCount : std::nullopt);
		track.modify()->setReleaseReplayGain(trackInfo->medium ? trackInfo->medium->replayGain : std::nullopt);
		track.modify()->setDiscSubtitle(trackInfo->medium ? trackInfo->medium->name : "");
		track.modify()->setClusters(getOrCreateClusters(dbSession, trackInfo->tags));
		track.modify()->setLastWriteTime(lastWriteTime);
		track.modify()->setName(title);
		track.modify()->setDuration(trackInfo->duration);
		track.modify()->setAddedTime(Wt::WDateTime::currentDateTime());
		track.modify()->setTrackNumber(trackInfo->position);
		track.modify()->setDiscNumber(trackInfo->medium ? trackInfo->medium->position : std::nullopt);
		track.modify()->setDate(trackInfo->date);
		track.modify()->setOriginalDate(trackInfo->originalDate);

		// If a file has an OriginalYear but no Year, set it to ease filtering
		if (!trackInfo->date.isValid() && trackInfo->originalDate.isValid())
			track.modify()->setDate(trackInfo->originalDate);

		track.modify()->setRecordingMBID(trackInfo->recordingMBID);
		track.modify()->setTrackMBID(trackInfo->mbid);
		if (auto trackFeatures {TrackFeatures::find(dbSession, track->getId())})
			trackFeatures.remove(); // TODO: only if MBID changed?
		track.modify()->setHasCover(trackInfo->hasCover);
		track.modify()->setCopyright(trackInfo->copyright);
		track.modify()->setCopyrightURL(trackInfo->copyrightURL);
		track.modify()->setTrackReplayGain(trackInfo->replayGain);
		track.modify()->setArtistDisplayName(trackInfo->artistDisplayName);
	}
}
