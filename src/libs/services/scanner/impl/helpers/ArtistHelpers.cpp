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

#include "database/ISession.hpp"

#include "types/TrackMetadata.hpp"

namespace lms::scanner
{
    std::ostream& operator<<(std::ostream& os, const db::Artist& artist)
    {
        os << "'" << artist.getName() << "'";
        if (const auto mbid{ artist.getMBID() })
            os << " [" << mbid->toString() << "]";

        return os;
    }
} // namespace lms::scanner

namespace lms::scanner::helpers
{
    namespace
    {
        db::Artist createArtist(db::ISession& session, const Artist& artistInfo)
        {
            db::Artist artist{ db::Artist::create(session, artistInfo.name) };

            if (artistInfo.mbid)
                artist.setMBID(artistInfo.mbid);

            if (artistInfo.sortName)
                artist.setSortName(*artistInfo.sortName);

            db::Artist::update(session, artist);
            return artist;
        }
    } // namespace

    db::Artist getOrCreateArtistByMBID(db::ISession& session, const Artist& artistInfo, AllowFallbackOnMBIDEntry allowFallbackOnMBIDEntries)
    {
        assert(artistInfo.mbid.has_value());

        std::optional<db::Artist> artist{ db::Artist::find(session, *artistInfo.mbid) };
        if (!artist)
        {
            if (allowFallbackOnMBIDEntries.value())
            {
                // an artist with the same name may already exist, let's recycle it
                for (const db::Artist& artistWithSameName : db::Artist::find(session, artistInfo.name))
                {
                    assert(artistWithSameName.getMBID() != artistInfo.mbid);
                    if (!artistWithSameName.hasMBID())
                    {
                        artist = artistWithSameName;
                        artist->setMBID(artistInfo.mbid);
                        db::Artist::update(session, *artist);
                        break;
                    }
                }
            }

            if (!artist)
                artist = createArtist(session, artistInfo);
        }

        return *artist;
    }

    db::Artist getOrCreateArtistByName(db::ISession& session, const Artist& artistInfo, AllowFallbackOnMBIDEntry allowFallbackOnMBIDEntries)
    {
        assert(artistInfo.mbid == std::nullopt);

        std::optional<db::Artist> artist;

        // Here we can have only one artist with no MBID, others all have mbids
        const std::vector<db::Artist> artistsWithSameName{ db::Artist::find(session, artistInfo.name) };
        const auto itArtistWithoutMBID{ std::find_if(std::begin(artistsWithSameName), std::end(artistsWithSameName), [](const db::Artist& artist) { return !artist.hasMBID(); }) };
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
            const auto itArtistWithMBID{ std::find_if(std::begin(artistsWithSameName), std::end(artistsWithSameName), [](const db::Artist& artist) { return artist.hasMBID(); }) };

            if (itArtistWithMBID != std::end(artistsWithSameName))
                artist = *itArtistWithMBID;
            else if (itArtistWithoutMBID != std::end(artistsWithSameName))
                artist = *itArtistWithoutMBID;
            else
                artist = createArtist(session, artistInfo);
        }

        return *artist;
    }

    db::Artist getOrCreateArtist(db::ISession& session, const Artist& artistInfo, AllowFallbackOnMBIDEntry allowFallbackOnMBIDEntries)
    {
        // First try to get by MBID
        if (artistInfo.mbid)
            return getOrCreateArtistByMBID(session, artistInfo, allowFallbackOnMBIDEntries);

        // Fall back on artist name (collisions may occur)
        return getOrCreateArtistByName(session, artistInfo, allowFallbackOnMBIDEntries);
    }
} // namespace lms::scanner::helpers