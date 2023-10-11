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

#include "MediaAnnotation.hpp"

#include <vector>

#include "services/database/ArtistId.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/TrackId.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "utils/Service.hpp"
#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"

namespace API::Subsonic
{
    using namespace Database;

    namespace
    {
        struct StarParameters
        {
            std::vector<ArtistId> artistIds;
            std::vector<ReleaseId> releaseIds;
            std::vector<TrackId> trackIds;
        };

        StarParameters getStarParameters(const Wt::Http::ParameterMap& parameters)
        {
            StarParameters res;

            // TODO handle parameters for legacy file browsing
            res.trackIds = getMultiParametersAs<TrackId>(parameters, "id");
            res.artistIds = getMultiParametersAs<ArtistId>(parameters, "artistId");
            res.releaseIds = getMultiParametersAs<ReleaseId>(parameters, "albumId");

            return res;
        }
    }

    Response handleStarRequest(RequestContext& context)
    {
        StarParameters params{ getStarParameters(context.parameters) };

        for (const ArtistId id : params.artistIds)
            Service<Scrobbling::IScrobblingService>::get()->star(context.userId, id);

        for (const ReleaseId id : params.releaseIds)
            Service<Scrobbling::IScrobblingService>::get()->star(context.userId, id);

        for (const TrackId id : params.trackIds)
            Service<Scrobbling::IScrobblingService>::get()->star(context.userId, id);

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleUnstarRequest(RequestContext& context)
    {
        StarParameters params{ getStarParameters(context.parameters) };

        for (const ArtistId id : params.artistIds)
            Service<Scrobbling::IScrobblingService>::get()->unstar(context.userId, id);

        for (const ReleaseId id : params.releaseIds)
            Service<Scrobbling::IScrobblingService>::get()->unstar(context.userId, id);

        for (const TrackId id : params.trackIds)
            Service<Scrobbling::IScrobblingService>::get()->unstar(context.userId, id);

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleScrobble(RequestContext& context)
    {
        const std::vector<TrackId> ids{ getMandatoryMultiParametersAs<TrackId>(context.parameters, "id") };
        const std::vector<unsigned long> times{ getMultiParametersAs<unsigned long>(context.parameters, "time") };
        const bool submission{ getParameterAs<bool>(context.parameters, "submission").value_or(true) };

        // playing now => no time to be provided
        if (!submission && !times.empty())
            throw BadParameterGenericError{ "time" };

        // playing now => only one at a time
        if (!submission && ids.size() > 1)
            throw BadParameterGenericError{ "id" };

        // if multiple submissions, must have times
        if (ids.size() > 1 && ids.size() != times.size())
            throw BadParameterGenericError{ "time" };

        if (!submission)
        {
            Service<Scrobbling::IScrobblingService>::get()->listenStarted({ context.userId, ids.front() });
        }
        else
        {
            if (times.empty())
            {
                Service<Scrobbling::IScrobblingService>::get()->listenFinished({ context.userId, ids.front() });
            }
            else
            {
                for (std::size_t i{}; i < ids.size(); ++i)
                {
                    const TrackId trackId{ ids[i] };
                    const unsigned long time{ times[i] };
                    Service<Scrobbling::IScrobblingService>::get()->addTimedListen({ {context.userId, trackId}, Wt::WDateTime::fromTime_t(static_cast<std::time_t>(time / 1000)) });
                }
            }
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }
}