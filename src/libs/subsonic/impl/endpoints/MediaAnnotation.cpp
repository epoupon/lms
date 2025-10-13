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

#include <variant>
#include <vector>

#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/User.hpp"
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

        ReleaseId getReleaseIdFromDirectory(Session& session, DirectoryId directory)
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

        struct RatingParameters
        {
            std::variant<ArtistId, ReleaseId, TrackId, DirectoryId> id;
            std::optional<Rating> rating;
        };

        RatingParameters getRatingParameters(const Wt::Http::ParameterMap& parameters)
        {
            RatingParameters res;

            if (const auto artistId{ getParameterAs<ArtistId>(parameters, "id") })
                res.id = *artistId;
            else if (const auto releaseId{ getParameterAs<ReleaseId>(parameters, "id") })
                res.id = *releaseId;
            else if (const auto trackId{ getParameterAs<TrackId>(parameters, "id") })
                res.id = *trackId;
            else if (const auto directoryId{ getParameterAs<DirectoryId>(parameters, "id") })
                res.id = *directoryId;
            else
                throw RequiredParameterMissingError{ "id" };

            const int rating = getMandatoryParameterAs<int>(parameters, "rating"); // The rating between 1 and 5 (inclusive), or 0 to remove the rating
            if (rating < 0 || rating > 5)
                throw BadParameterGenericError{ "rating", "must be 0 or in range 1-5" };

            if (rating > 0)
                res.rating = rating;

            return res;
        }

    } // namespace

    Response handleStarRequest(RequestContext& context)
    {
        StarParameters params{ getStarParameters(context.getParameters()) };

        for (const DirectoryId id : params.directoryIds)
        {
            if (const ReleaseId releaseId{ getReleaseIdFromDirectory(context.getDbSession(), id) }; releaseId.isValid())
                core::Service<feedback::IFeedbackService>::get()->star(context.getUser()->getId(), releaseId);
        }

        for (const ArtistId id : params.artistIds)
            core::Service<feedback::IFeedbackService>::get()->star(context.getUser()->getId(), id);

        for (const ReleaseId id : params.releaseIds)
            core::Service<feedback::IFeedbackService>::get()->star(context.getUser()->getId(), id);

        for (const TrackId id : params.trackIds)
            core::Service<feedback::IFeedbackService>::get()->star(context.getUser()->getId(), id);

        return Response::createOkResponse(context.getServerProtocolVersion());
    }

    Response handleUnstarRequest(RequestContext& context)
    {
        const StarParameters params{ getStarParameters(context.getParameters()) };

        for (const DirectoryId id : params.directoryIds)
        {
            if (const ReleaseId releaseId{ getReleaseIdFromDirectory(context.getDbSession(), id) }; releaseId.isValid())
                core::Service<feedback::IFeedbackService>::get()->unstar(context.getUser()->getId(), releaseId);
        }

        for (const ArtistId id : params.artistIds)
            core::Service<feedback::IFeedbackService>::get()->unstar(context.getUser()->getId(), id);

        for (const ReleaseId id : params.releaseIds)
            core::Service<feedback::IFeedbackService>::get()->unstar(context.getUser()->getId(), id);

        for (const TrackId id : params.trackIds)
            core::Service<feedback::IFeedbackService>::get()->unstar(context.getUser()->getId(), id);

        return Response::createOkResponse(context.getServerProtocolVersion());
    }

    Response handleSetRating(RequestContext& context)
    {
        const RatingParameters params{ getRatingParameters(context.getParameters()) };

        if (const ArtistId * artistId{ std::get_if<ArtistId>(&params.id) })
            core::Service<feedback::IFeedbackService>::get()->setRating(context.getUser()->getId(), *artistId, params.rating);
        else if (const DirectoryId * directoryId{ std::get_if<DirectoryId>(&params.id) })
        {
            if (const ReleaseId releaseId{ getReleaseIdFromDirectory(context.getDbSession(), *directoryId) }; releaseId.isValid())
                core::Service<feedback::IFeedbackService>::get()->setRating(context.getUser()->getId(), releaseId, params.rating);
        }
        else if (const ReleaseId * releaseId{ std::get_if<ReleaseId>(&params.id) })
            core::Service<feedback::IFeedbackService>::get()->setRating(context.getUser()->getId(), *releaseId, params.rating);
        else if (const TrackId * trackId{ std::get_if<TrackId>(&params.id) })
            core::Service<feedback::IFeedbackService>::get()->setRating(context.getUser()->getId(), *trackId, params.rating);

        return Response::createOkResponse(context.getServerProtocolVersion());
    }

    Response handleScrobble(RequestContext& context)
    {
        const std::vector<TrackId> ids{ getMandatoryMultiParametersAs<TrackId>(context.getParameters(), "id") };
        const std::vector<unsigned long> times{ getMultiParametersAs<unsigned long>(context.getParameters(), "time") };
        const bool submission{ getParameterAs<bool>(context.getParameters(), "submission").value_or(true) };

        // playing now => only one at a time
        if (!submission && ids.size() > 1)
            throw BadParameterGenericError{ "id" };

        // if multiple submissions, must have all times
        if (ids.size() > 1 && ids.size() != times.size())
            throw BadParameterGenericError{ "time" };

        if (!submission)
        {
            core::Service<scrobbling::IScrobblingService>::get()->listenStarted({ context.getUser()->getId(), ids.front() });
        }
        else
        {
            if (times.empty())
            {
                core::Service<scrobbling::IScrobblingService>::get()->listenFinished({ context.getUser()->getId(), ids.front() });
            }
            else
            {
                for (std::size_t i{}; i < ids.size(); ++i)
                {
                    const TrackId trackId{ ids[i] };
                    const unsigned long time{ times[i] };
                    core::Service<scrobbling::IScrobblingService>::get()->addTimedListen({ { context.getUser()->getId(), trackId }, Wt::WDateTime::fromTime_t(static_cast<std::time_t>(time / 1000)) });
                }
            }
        }

        return Response::createOkResponse(context.getServerProtocolVersion());
    }
} // namespace lms::api::subsonic