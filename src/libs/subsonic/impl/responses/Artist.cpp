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
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"

#include "RequestContext.hpp"
#include "SubsonicId.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    namespace utils
    {
        std::string joinArtistNames(const std::vector<TrackArtistLink::pointer>& links)
        {
            if (links.size() == 1)
                return std::string{ links.front()->getArtistName() };

            std::vector<std::string> names;
            names.resize(links.size());

            std::transform(std::cbegin(links), std::cend(links), std::begin(names),
                           [](const TrackArtistLink::pointer& link) {
                               return std::string{ link->getArtistName() };
                           });

            return core::stringUtils::joinStrings(names, ", ");
        }

        std::string_view toString(TrackArtistLinkType type)
        {
            return db::trackArtistLinkTypeToString(type).str();
        }
    } // namespace utils

    Response::Node createArtistNode(RequestContext& context, const Artist::pointer& artist)
    {
        LMS_SCOPED_TRACE_DETAILED("Subsonic", "CreateArtist");

        Response::Node artistNode{ createMinimalArtistNode(artist) };

        if (const db::ArtworkId artworkId{ artist->getPreferredArtworkId() }; artworkId.isValid())
            artistNode.setAttribute("coverArt", idToString(artworkId));

        bool hasAlbums{};
        {
            std::size_t count{ Release::getCount(context.getDbSession(), Release::FindParameters{}.setArtist(artist->getId())) };
            hasAlbums = count > 0;

            // TODO: not very efficient
            Release::find(context.getDbSession(), Release::FindParameters{}.setTrackArtist(artist->getId()), [&](const db::Release::pointer& release) {
                if (!release->hasArtist(artist->getId()))
                    count++;
            });

            artistNode.setAttribute("albumCount", count);
        }

        if (const Wt::WDateTime dateTime{ core::Service<feedback::IFeedbackService>::get()->getStarredDateTime(context.getUser()->getId(), artist->getId()) }; dateTime.isValid())
            artistNode.setAttribute("starred", core::stringUtils::toISO8601String(dateTime));

        if (const auto rating{ core::Service<feedback::IFeedbackService>::get()->getRating(context.getUser()->getId(), artist->getId()) })
            artistNode.setAttribute("userRating", *rating);

        // OpenSubsonic specific fields (must always be set)
        if (context.isOpenSubsonicEnabled())
        {
            artistNode.setAttribute("mediaType", "artist");

            {
                std::optional<core::UUID> mbid{ artist->getMBID() };
                artistNode.setAttribute("musicBrainzId", mbid ? mbid->toString() : "");
            }

            artistNode.setAttribute("sortName", artist->getSortName());

            Response::Node roles;
            artistNode.createEmptyArrayValue("roles");
            for (const TrackArtistLinkType linkType : TrackArtistLink::findUsedTypes(context.getDbSession(), artist->getId()))
                artistNode.addArrayValue("roles", utils::toString(linkType));

            if (hasAlbums)
                artistNode.addArrayValue("roles", "albumartist");
        }

        return artistNode;
    }

    Response::Node createMinimalArtistNode(const Artist::pointer& artist)
    {
        Response::Node artistNode;

        artistNode.setAttribute("id", idToString(artist->getId()));
        artistNode.setAttribute("name", artist->getName());

        return artistNode;
    }

    Response::Node createMinimalArtistNode(const db::ObjectPtr<db::ReleaseArtistLink>& artistLink)
    {
        Response::Node artistNode;

        artistNode.setAttribute("id", idToString(artistLink->getArtistId()));
        artistNode.setAttribute("name", artistLink->getArtistName());

        return artistNode;
    }

    Response::Node createMinimalArtistNode(const db::ObjectPtr<db::TrackArtistLink>& artistLink)
    {
        Response::Node artistNode;

        artistNode.setAttribute("id", idToString(artistLink->getArtistId()));
        artistNode.setAttribute("name", artistLink->getArtistName());

        return artistNode;
    }
} // namespace lms::api::subsonic