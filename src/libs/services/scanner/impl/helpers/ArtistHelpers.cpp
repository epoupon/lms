/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "ArtistHelpers.hpp"

#include <ostream>

#include "core/ILogger.hpp"
#include "database/Session.hpp"
#include "database/objects/ArtistInfo.hpp"

#include "types/TrackMetadata.hpp"

namespace lms::scanner
{
    std::ostream& operator<<(std::ostream& os, const db::Artist::pointer& artist)
    {
        os << "'" << artist->getName() << "'";
        if (const auto mbid{ artist->getMBID() })
            os << " [" << mbid->toString() << "]";

        return os;
    }
} // namespace lms::scanner

namespace lms::scanner::helpers
{
    namespace
    {
        db::Artist::pointer createArtist(db::Session& session, const Artist& artistInfo)
        {
            db::Artist::pointer artist{ session.create<db::Artist>(artistInfo.name) };

            if (artistInfo.mbid)
                artist.modify()->setMBID(artistInfo.mbid);

            if (artistInfo.sortName)
                artist.modify()->setSortName(*artistInfo.sortName);

            return artist;
        }

        void updateSortNameIfNeeded(db::Session& session, db::Artist::pointer& artist, const Artist& artistInfo)
        {
            if (!artistInfo.sortName || artist->getSortName() == *artistInfo.sortName)
                return;

            // an artist info file, if any, is the authoritative source and must not be overridden by a track/release tag
            bool hasArtistInfo{};
            db::ArtistInfo::find(session, artist->getId(), db::Range{ .offset = 0, .size = 1 }, [&](const db::ArtistInfo::pointer&) {
                hasArtistInfo = true;
            });
            if (hasArtistInfo)
                return;

            LMS_LOG(DBUPDATER, DEBUG, "Updating artist " << artist << " sort name from '" << artist->getSortName() << "' to '" << *artistInfo.sortName << "'");
            artist.modify()->setSortName(*artistInfo.sortName);
        }
    } // namespace

    db::Artist::pointer getOrCreateArtistByMBID(db::Session& session, const Artist& artistInfo, AllowFallbackOnMBIDEntry allowFallbackOnMBIDEntries)
    {
        assert(artistInfo.mbid.has_value());

        db::Artist::pointer artist{ db::Artist::find(session, *artistInfo.mbid) };
        if (!artist)
        {
            if (allowFallbackOnMBIDEntries.value())
            {
                // an artist with the same name may already exist, let's recycle it
                for (const db::Artist::pointer& artistWithSameName : db::Artist::find(session, artistInfo.name))
                {
                    assert(artistWithSameName->getMBID() != artistInfo.mbid);
                    if (!artistWithSameName->hasMBID())
                    {
                        artist = artistWithSameName;
                        artist.modify()->setMBID(artistInfo.mbid);
                        break;
                    }
                }
            }

            if (!artist)
                artist = createArtist(session, artistInfo);
        }

        updateSortNameIfNeeded(session, artist, artistInfo);

        return artist;
    }

    db::Artist::pointer getOrCreateArtistByName(db::Session& session, const Artist& artistInfo, AllowFallbackOnMBIDEntry allowFallbackOnMBIDEntries)
    {
        assert(artistInfo.mbid == std::nullopt);

        db::Artist::pointer artist;

        // Here we can have only one artist with no MBID, others all have mbids
        const std::vector<db::Artist::pointer> artistsWithSameName{ db::Artist::find(session, artistInfo.name) };
        const auto itArtistWithoutMBID{ std::find_if(std::begin(artistsWithSameName), std::end(artistsWithSameName), [](const db::Artist::pointer& artist) { return !artist->hasMBID(); }) };
        const std::size_t artistCountWithMBID{ artistsWithSameName.size() - (itArtistWithoutMBID != std::end(artistsWithSameName) ? 1 : 0) };

        if (!allowFallbackOnMBIDEntries.value() || artistCountWithMBID > 1)
        {
            if (itArtistWithoutMBID != std::end(artistsWithSameName))
                artist = *itArtistWithoutMBID;
            else
                artist = createArtist(session, artistInfo);
        }
        else
        {
            const auto itArtistWithMBID{ std::find_if(std::begin(artistsWithSameName), std::end(artistsWithSameName), [](const db::Artist::pointer& artist) { return artist->hasMBID(); }) };

            if (itArtistWithMBID != std::end(artistsWithSameName))
                artist = *itArtistWithMBID;
            else if (itArtistWithoutMBID != std::end(artistsWithSameName))
                artist = *itArtistWithoutMBID;
            else
                artist = createArtist(session, artistInfo);
        }

        // don't override a sort name coming from a higher-confidence MBID-matched artist, but do fill in a missing one
        if (!artist->hasMBID() || artist->getSortName().empty())
            updateSortNameIfNeeded(session, artist, artistInfo);

        return artist;
    }

    db::Artist::pointer getOrCreateArtist(db::Session& session, const Artist& artistInfo, AllowFallbackOnMBIDEntry allowFallbackOnMBIDEntries)
    {
        // First try to get by MBID
        if (artistInfo.mbid)
            return getOrCreateArtistByMBID(session, artistInfo, allowFallbackOnMBIDEntries);

        // Fall back on artist name (collisions may occur)
        return getOrCreateArtistByName(session, artistInfo, allowFallbackOnMBIDEntries);
    }
} // namespace lms::scanner::helpers