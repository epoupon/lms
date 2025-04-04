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

#include "core/ILogger.hpp"
#include "database/Session.hpp"
#include "metadata/Types.hpp"

namespace lms::scanner::helpers
{
    namespace
    {
        db::Artist::pointer createArtist(db::Session& session, const metadata::Artist& artistInfo)
        {
            db::Artist::pointer artist{ session.create<db::Artist>(artistInfo.name) };

            if (artistInfo.mbid)
                artist.modify()->setMBID(artistInfo.mbid);

            artist.modify()->setSortName(artistInfo.sortName ? *artistInfo.sortName : artistInfo.name);

            return artist;
        }

        std::string optionalMBIDAsString(const std::optional<core::UUID>& uuid)
        {
            return uuid ? std::string{ uuid->getAsString() } : "<no MBID>";
        }

        void updateArtistIfNeeded(db::Artist::pointer artist, const metadata::Artist& artistInfo)
        {
            // MBID may be set
            if (artist->getMBID() != artistInfo.mbid)
                artist.modify()->setMBID(artistInfo.mbid);

            // Name may have been updated
            if (artist->getName() != artistInfo.name)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Artist [" << optionalMBIDAsString(artist->getMBID()) << "], updated name from '" << artist->getName() << "' to '" << artistInfo.name << "'");
                artist.modify()->setName(artistInfo.name);
            }

            // Sortname may have been updated
            // As the sort name is quite often not filled in, we update it only if already set (for now?)
            if (artistInfo.sortName && *artistInfo.sortName != artist->getSortName())
            {
                LMS_LOG(DBUPDATER, DEBUG, "Artist [" << optionalMBIDAsString(artist->getMBID()) << "], updated sort name from '" << artist->getSortName() << "' to '" << *artistInfo.sortName << "'");
                artist.modify()->setSortName(*artistInfo.sortName);
            }
        }

    } // namespace

    db::Artist::pointer getOrCreateArtistByMBID(db::Session& session, const metadata::Artist& artistInfo, AllowFallbackOnMBIDEntry allowFallbackOnMBIDEntries)
    {
        assert(artistInfo.mbid.has_value());
        db::Artist::pointer artist{ db::Artist::find(session, *artistInfo.mbid) };
        if (artist)
        {
            updateArtistIfNeeded(artist, artistInfo);
        }
        else
        {
            if (allowFallbackOnMBIDEntries.value())
            {
                // an artist with the same name may already exist, let's recycle it
                for (const db::Artist::pointer& artistWithSameName : db::Artist::find(session, artistInfo.name))
                {
                    if (!artistWithSameName->hasMBID())
                    {
                        artist = artistWithSameName;
                        updateArtistIfNeeded(artist, artistInfo);
                        break;
                    }
                }
            }

            if (!artist)
                artist = createArtist(session, artistInfo);
        }

        return artist;
    }

    db::Artist::pointer getOrCreateArtistByName(db::Session& session, const metadata::Artist& artistInfo, AllowFallbackOnMBIDEntry allowFallbackOnMBIDEntries)
    {
        db::Artist::pointer artist;

        // Here we can have only one artist with no MBID, others all have mbids
        const std::vector<db::Artist::pointer> artistsWithSameName{ db::Artist::find(session, artistInfo.name) };
        const auto itArtistWithoutMBID{ std::find_if(std::begin(artistsWithSameName), std::end(artistsWithSameName), [](const db::Artist::pointer& artist) { return !artist->hasMBID(); }) };
        const std::size_t artistCountWithMBID{ artistsWithSameName.size() - (itArtistWithoutMBID != std::end(artistsWithSameName) ? 1 : 0) };

        if (!allowFallbackOnMBIDEntries.value() || artistCountWithMBID > 1)
        {
            if (itArtistWithoutMBID != std::end(artistsWithSameName))
            {
                artist = *itArtistWithoutMBID;
                updateArtistIfNeeded(artist, artistInfo);
            }
            else
                artist = createArtist(session, artistInfo);
        }
        else
        {
            const auto itArtistWithMBID{ std::find_if(std::begin(artistsWithSameName), std::end(artistsWithSameName), [](const db::Artist::pointer& artist) { return artist->hasMBID(); }) };

            if (itArtistWithMBID != std::end(artistsWithSameName))
            {
                artist = *itArtistWithMBID;
                // not updating artist here: consider metadata quality is less good
            }
            else if (itArtistWithoutMBID != std::end(artistsWithSameName))
            {
                artist = *itArtistWithoutMBID;
                updateArtistIfNeeded(artist, artistInfo);
            }
            else
                artist = createArtist(session, artistInfo);
        }

        return artist;
    }

    db::Artist::pointer getOrCreateArtist(db::Session& session, const metadata::Artist& artistInfo, AllowFallbackOnMBIDEntry allowFallbackOnMBIDEntries)
    {
        // First try to get by MBID
        if (artistInfo.mbid)
            return getOrCreateArtistByMBID(session, artistInfo, allowFallbackOnMBIDEntries);

        // Fall back on artist name (collisions may occur)
        return getOrCreateArtistByName(session, artistInfo, allowFallbackOnMBIDEntries);
    }
} // namespace lms::scanner::helpers