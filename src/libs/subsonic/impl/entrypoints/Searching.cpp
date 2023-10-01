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

#include "Searching.hpp"

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "responses/Album.hpp"
#include "responses/Artist.hpp"
#include "responses/Song.hpp"
#include "ParameterParsing.hpp"
 
#include "ParameterParsing.hpp"

namespace API::Subsonic
{
    using namespace Database;

    namespace
    {
        Response handleSearchRequestCommon(RequestContext& context, bool id3)
        {
            // Mandatory params
            std::string queryString{ getMandatoryParameterAs<std::string>(context.parameters, "query") };
            std::string_view query{ queryString };

            // Symfonium adds extra ""
            if (context.clientInfo.name == "Symfonium")
                query = StringUtils::stringTrim(query, "\"");

            std::vector<std::string_view> keywords{ StringUtils::splitString(query, " ") };

            // Optional params
            std::size_t artistCount{ getParameterAs<std::size_t>(context.parameters, "artistCount").value_or(20) };
            std::size_t artistOffset{ getParameterAs<std::size_t>(context.parameters, "artistOffset").value_or(0) };
            std::size_t albumCount{ getParameterAs<std::size_t>(context.parameters, "albumCount").value_or(20) };
            std::size_t albumOffset{ getParameterAs<std::size_t>(context.parameters, "albumOffset").value_or(0) };
            std::size_t songCount{ getParameterAs<std::size_t>(context.parameters, "songCount").value_or(20) };
            std::size_t songOffset{ getParameterAs<std::size_t>(context.parameters, "songOffset").value_or(0) };

            auto transaction{ context.dbSession.createSharedTransaction() };

            User::pointer user{ User::find(context.dbSession, context.userId) };
            if (!user)
                throw UserNotAuthorizedError{};

            Response response{ Response::createOkResponse(context.serverProtocolVersion) };
            Response::Node& searchResult2Node{ response.createNode(id3 ? "searchResult3" : "searchResult2") };

            if (artistCount > 0)
            {
                Artist::FindParameters params;
                params.setKeywords(keywords);
                params.setSortMethod(ArtistSortMethod::BySortName);
                params.setRange({ artistOffset, artistCount });

                RangeResults<ArtistId> artistIds{ Artist::find(context.dbSession, params) };
                for (const ArtistId artistId : artistIds.results)
                {
                    const auto artist{ Artist::find(context.dbSession, artistId) };
                    searchResult2Node.addArrayChild("artist", createArtistNode(artist, context.dbSession, user, id3));
                }
            }

            if (albumCount > 0)
            {
                Release::FindParameters params;
                params.setKeywords(keywords);
                params.setSortMethod(ReleaseSortMethod::Name);
                params.setRange({ albumOffset, albumCount });

                RangeResults<ReleaseId> releaseIds{ Release::find(context.dbSession, params) };
                for (const ReleaseId releaseId : releaseIds.results)
                {
                    const auto release{ Release::find(context.dbSession, releaseId) };
                    searchResult2Node.addArrayChild("album", createAlbumNode(release, context.dbSession, user, id3));
                }
            }

            if (songCount > 0)
            {
                Track::FindParameters params;
                params.setKeywords(keywords);
                params.setRange({ songOffset, songCount });

                RangeResults<TrackId> trackIds{ Track::find(context.dbSession, params) };
                for (const TrackId trackId : trackIds.results)
                {
                    const auto track{ Track::find(context.dbSession, trackId) };
                    searchResult2Node.addArrayChild("song", createSongNode(track, context.dbSession, user));
                }
            }

            return response;
        }
    }

    Response handleSearch2Request(RequestContext& context)
    {
        return handleSearchRequestCommon(context, false /* no id3 */);
    }

    Response handleSearch3Request(RequestContext& context)
    {
        return handleSearchRequestCommon(context, true /* id3 */);
    }

}