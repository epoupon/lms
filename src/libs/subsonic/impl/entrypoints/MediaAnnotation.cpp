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

#include "core/Service.hpp"
#include "database/ArtistId.hpp"
#include "database/Release.hpp"
#include "database/ReleaseId.hpp"
#include "database/Session.hpp"
#include "database/TrackId.hpp"
#include "database/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    namespace
    {
        struct StarParameters
        {
            std::vector<ArtistId> artistIds;
            std::vector<ReleaseId> releaseIds;
            std::vector<TrackId> trackIds;
            std::vector<DirectoryId> directoryIds;
        };

        StarParameters getStarParameters(const Wt::Http::ParameterMap& parameters)
        {
            StarParameters res;

            // id could be either a trackId or a directory id
            res.directoryIds = getMultiParametersAs<DirectoryId>(parameters, "id");
            res.trackIds = getMultiParametersAs<TrackId>(parameters, "id");
            res.artistIds = getMultiParametersAs<ArtistId>(parameters, "artistId");
            res.releaseIds = getMultiParametersAs<ReleaseId>(parameters, "albumId");

            return res;
        }

        ReleaseId getReleaseFromDirectory(Session& session, DirectoryId directory)
        {
            auto transaction{ session.createReadTransaction() };

            Release::FindParameters params;
            params.setDirectory(directory);
            params.setRange(Range{ 0, 1 }); // consider one directory <-> one release

            Release::pointer res;
            Release::find(session, params, [&](const Release::pointer& release) {
                res = release;
            });

            return res ? res->getId() : ReleaseId{};
        }

    } // namespace

    Response handleStarRequest(RequestContext& context)
    {
        StarParameters params{ getStarParameters(context.parameters) };

        for (const DirectoryId id : params.directoryIds)
        {
            if (const ReleaseId releaseId{ getReleaseFromDirectory(context.dbSession, id) }; releaseId.isValid())
                core::Service<feedback::IFeedbackService>::get()->star(context.user->getId(), releaseId);
        }

        for (const ArtistId id : params.artistIds)
            core::Service<feedback::IFeedbackService>::get()->star(context.user->getId(), id);

        for (const ReleaseId id : params.releaseIds)
            core::Service<feedback::IFeedbackService>::get()->star(context.user->getId(), id);

        for (const TrackId id : params.trackIds)
            core::Service<feedback::IFeedbackService>::get()->star(context.user->getId(), id);

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleUnstarRequest(RequestContext& context)
    {
        StarParameters params{ getStarParameters(context.parameters) };

        for (const DirectoryId id : params.directoryIds)
        {
            if (const ReleaseId releaseId{ getReleaseFromDirectory(context.dbSession, id) }; releaseId.isValid())
                core::Service<feedback::IFeedbackService>::get()->unstar(context.user->getId(), releaseId);
        }

        for (const ArtistId id : params.artistIds)
            core::Service<feedback::IFeedbackService>::get()->unstar(context.user->getId(), id);

        for (const ReleaseId id : params.releaseIds)
            core::Service<feedback::IFeedbackService>::get()->unstar(context.user->getId(), id);

        for (const TrackId id : params.trackIds)
            core::Service<feedback::IFeedbackService>::get()->unstar(context.user->getId(), id);

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleScrobble(RequestContext& context)
    {
        const std::vector<TrackId> ids{ getMandatoryMultiParametersAs<TrackId>(context.parameters, "id") };
        const std::vector<unsigned long> times{ getMultiParametersAs<unsigned long>(context.parameters, "time") };
        const bool submission{ getParameterAs<bool>(context.parameters, "submission").value_or(true) };

        // playing now => only one at a time
        if (!submission && ids.size() > 1)
            throw BadParameterGenericError{ "id" };

        // if multiple submissions, must have all times
        if (ids.size() > 1 && ids.size() != times.size())
            throw BadParameterGenericError{ "time" };

        if (!submission)
        {
            core::Service<scrobbling::IScrobblingService>::get()->listenStarted({ context.user->getId(), ids.front() });
        }
        else
        {
            if (times.empty())
            {
                core::Service<scrobbling::IScrobblingService>::get()->listenFinished({ context.user->getId(), ids.front() });
            }
            else
            {
                for (std::size_t i{}; i < ids.size(); ++i)
                {
                    const TrackId trackId{ ids[i] };
                    const unsigned long time{ times[i] };
                    core::Service<scrobbling::IScrobblingService>::get()->addTimedListen({ { context.user->getId(), trackId }, Wt::WDateTime::fromTime_t(static_cast<std::time_t>(time / 1000)) });
                }
            }
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }
} // namespace lms::api::subsonic