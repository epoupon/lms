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

#include "responses/Artist.hpp"

#include "core/ITraceLogger.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"

#include "CoverArtId.hpp"
#include "RequestContext.hpp"
#include "SubsonicId.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    namespace utils
    {
        std::string joinArtistNames(const std::vector<Artist::pointer>& artists)
        {
            if (artists.size() == 1)
                return artists.front()->getName();

            std::vector<std::string> names;
            names.resize(artists.size());

            std::transform(std::cbegin(artists), std::cend(artists), std::begin(names),
                [](const Artist::pointer& artist) {
                    return artist->getName();
                });

            return core::stringUtils::joinStrings(names, ", ");
        }

        std::string_view toString(TrackArtistLinkType type)
        {
            switch (type)
            {
            case TrackArtistLinkType::Arranger:
                return "arranger";
            case TrackArtistLinkType::Artist:
                return "artist";
            case TrackArtistLinkType::Composer:
                return "composer";
            case TrackArtistLinkType::Conductor:
                return "conductor";
            case TrackArtistLinkType::Lyricist:
                return "lyricist";
            case TrackArtistLinkType::Mixer:
                return "mixer";
            case TrackArtistLinkType::Performer:
                return "performer";
            case TrackArtistLinkType::Producer:
                return "producer";
            case TrackArtistLinkType::ReleaseArtist:
                return "albumartist";
            case TrackArtistLinkType::Remixer:
                return "remixer";
            case TrackArtistLinkType::Writer:
                return "writer";
            }

            return "unknown";
        }
    } // namespace utils

    Response::Node createArtistNode(RequestContext& context, const Artist::pointer& artist)
    {
        LMS_SCOPED_TRACE_DETAILED("Subsonic", "CreateArtist");

        Response::Node artistNode{ createArtistNode(artist) };

        artistNode.setAttribute("id", idToString(artist->getId()));
        artistNode.setAttribute("name", artist->getName());
        if (const auto artwork{ artist->getPreferredArtwork() })
        {
            CoverArtId coverArtId{ artwork->getId(), artwork->getLastWrittenTime().toTime_t() };
            artistNode.setAttribute("coverArt", idToString(coverArtId));
        }

        const std::size_t count{ Release::getCount(context.dbSession, Release::FindParameters{}.setArtist(artist->getId())) };
        artistNode.setAttribute("albumCount", count);

        if (const Wt::WDateTime dateTime{ core::Service<feedback::IFeedbackService>::get()->getStarredDateTime(context.user->getId(), artist->getId()) }; dateTime.isValid())
            artistNode.setAttribute("starred", core::stringUtils::toISO8601String(dateTime));

        if (const auto rating{ core::Service<feedback::IFeedbackService>::get()->getRating(context.user->getId(), artist->getId()) })
            artistNode.setAttribute("userRating", *rating);

        // OpenSubsonic specific fields (must always be set)
        if (context.enableOpenSubsonic)
        {
            artistNode.setAttribute("mediaType", "artist");

            {
                std::optional<core::UUID> mbid{ artist->getMBID() };
                artistNode.setAttribute("musicBrainzId", mbid ? mbid->getAsString() : "");
            }

            artistNode.setAttribute("sortName", artist->getSortName());

            // roles
            Response::Node roles;
            artistNode.createEmptyArrayValue("roles");
            for (const TrackArtistLinkType linkType : TrackArtistLink::findUsedTypes(context.dbSession, artist->getId()))
                artistNode.addArrayValue("roles", utils::toString(linkType));
        }

        return artistNode;
    }

    Response::Node createArtistNode(const Artist::pointer& artist)
    {
        Response::Node artistNode;

        artistNode.setAttribute("id", idToString(artist->getId()));
        artistNode.setAttribute("name", artist->getName());

        return artistNode;
    }
} // namespace lms::api::subsonic