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

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/User.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"

#include "SubsonicId.hpp"

namespace API::Subsonic
{
    using namespace Database;

    namespace Utils
    {
        std::string joinArtistNames(const std::vector<Artist::pointer>& artists)
        {
            if (artists.size() == 1)
                return artists.front()->getName();

            std::vector<std::string> names;
            names.resize(artists.size());

            std::transform(std::cbegin(artists), std::cend(artists), std::begin(names),
                [](const Artist::pointer& artist)
                {
                    return artist->getName();
                });

            return StringUtils::joinStrings(names, ", ");
        }
    }

    Response::Node createArtistNode(const Artist::pointer& artist, Session& session, const User::pointer& user, bool id3)
    {
        Response::Node artistNode;

        artistNode.setAttribute("id", idToString(artist->getId()));
        artistNode.setAttribute("name", artist->getName());

        if (id3)
        {
            const auto releases{ Release::find(session, Release::FindParameters {}.setArtist(artist->getId())) };
            artistNode.setAttribute("albumCount", releases.results.size());
        }

        if (const Wt::WDateTime dateTime{ Service<Scrobbling::IScrobblingService>::get()->getStarredDateTime(user->getId(), artist->getId()) }; dateTime.isValid())
            artistNode.setAttribute("starred", StringUtils::toISO8601String(dateTime));

        return artistNode;
    }
}