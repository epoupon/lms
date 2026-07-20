/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "Jukebox.hpp"

#include <functional>
#include <thread>

#include "core/Service.hpp"

#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "SubsonicResponse.hpp"
#include "responses/Song.hpp"
#include "services/jukebox/IJukeboxService.hpp"

namespace lms::api::subsonic
{
    namespace detail
    {
        void initJukeboxIfNeeded(jukebox::IJukeboxService& jukeboxService)
        {
            switch (jukeboxService.getState())
            {
            case jukebox::ServiceState::Uninitialized:
                jukeboxService.startInit();
                [[fallthrough]];

            case jukebox::ServiceState::Initializing:
                while (jukeboxService.getState() == jukebox::ServiceState::Initializing)
                    std::this_thread::yield(); // should be hopefully quite fast
                break;

            case jukebox::ServiceState::Failed:
            case jukebox::ServiceState::Ready:
                break;
            }
        }

        Response::Node createJukeboxStatusNode(const jukebox::IJukeboxService& jukeboxService)
        {
            Response::Node statusNode;

            statusNode.setAttribute("currentIndex", jukeboxService.getCurrentTrackIndex() ? *jukeboxService.getCurrentTrackIndex() : -1); // required
            statusNode.setAttribute("playing", !jukeboxService.isPaused());                                                               // required
            statusNode.setAttribute("position", std::chrono::duration_cast<std::chrono::seconds>(jukeboxService.getPlaybackTrackTime()).count());
            statusNode.setAttribute("gain", jukeboxService.getVolume());

            return statusNode;
        }

        Response handleJukeboxGet(RequestContext& context, jukebox::IJukeboxService& jukeboxService)
        {
            Response response{ Response::createOkResponse() };
            Response::Node jukeboxPlaylistNode{ createJukeboxStatusNode(jukeboxService) };

            {
                auto transaction{ context.getDbSession().createReadTransaction() };
                for (const db::TrackId trackId : jukeboxService.getTracks())
                {
                    if (const db::Track::pointer track{ db::Track::find(context.getDbSession(), trackId) })
                        jukeboxPlaylistNode.addArrayChild("entry", createSongNode(context, track, true));
                }
            }

            response.addNode("jukeboxPlaylist", std::move(jukeboxPlaylistNode));

            return response;
        }

        Response handleJukeboxStatus(RequestContext& /*context*/, jukebox::IJukeboxService& jukeboxService)
        {
            Response response{ Response::createOkResponse() };
            response.addNode("jukeboxStatus", createJukeboxStatusNode(jukeboxService));
            return response;
        }

        Response handleJukeboxSet(RequestContext& context, jukebox::IJukeboxService& jukeboxService)
        {
            const auto trackIds{ getMultiParametersAs<db::TrackId>(context.getParameters(), "id") };

            // set is similar to a clear followed by a add, but will not change the currently playing track
            jukeboxService.clearTracks();
            jukeboxService.appendTracks(trackIds);

            Response response{ Response::createOkResponse() };
            response.addNode("jukeboxStatus", createJukeboxStatusNode(jukeboxService));
            return response;
        }

        Response handleJukeboxStart(RequestContext& /*context*/, jukebox::IJukeboxService& jukeboxService)
        {
            jukeboxService.resume();

            Response response{ Response::createOkResponse() };
            response.addNode("jukeboxStatus", createJukeboxStatusNode(jukeboxService));
            return response;
        }

        Response handleJukeboxStop(RequestContext& /*context*/, jukebox::IJukeboxService& jukeboxService)
        {
            jukeboxService.pause();

            Response response{ Response::createOkResponse() };
            response.addNode("jukeboxStatus", createJukeboxStatusNode(jukeboxService));
            return response;
        }

        Response handleJukeboxSkip(RequestContext& context, jukebox::IJukeboxService& jukeboxService)
        {
            const auto index{ getMandatoryParameterAs<std::size_t>(context.getParameters(), "index") };
            const auto offset{ getParameterAs<std::chrono::seconds::rep>(context.getParameters(), "offset").value_or(0) };

            // do not report potential range error
            jukeboxService.play(index, std::chrono::seconds{ offset });

            Response response{ Response::createOkResponse() };
            response.addNode("jukeboxStatus", createJukeboxStatusNode(jukeboxService));
            return response;
        }

        Response handleJukeboxAdd(RequestContext& context, jukebox::IJukeboxService& jukeboxService)
        {
            const auto trackIds{ getMandatoryMultiParametersAs<db::TrackId>(context.getParameters(), "id") };

            jukeboxService.appendTracks(trackIds);

            Response response{ Response::createOkResponse() };
            response.addNode("jukeboxStatus", createJukeboxStatusNode(jukeboxService));
            return response;
        }

        Response handleJukeboxClear(RequestContext& /*context*/, jukebox::IJukeboxService& jukeboxService)
        {
            jukeboxService.clearTracks();

            Response response{ Response::createOkResponse() };
            response.addNode("jukeboxStatus", createJukeboxStatusNode(jukeboxService));
            return response;
        }

        Response handleJukeboxRemove(RequestContext& context, jukebox::IJukeboxService& jukeboxService)
        {
            const auto index{ getMandatoryParameterAs<std::size_t>(context.getParameters(), "index") };
            jukeboxService.removeTrack(index);

            Response response{ Response::createOkResponse() };
            response.addNode("jukeboxStatus", createJukeboxStatusNode(jukeboxService));
            return response;
        }

        Response handleJukeboxShuffle(RequestContext& /*context*/, jukebox::IJukeboxService& jukeboxService)
        {
            jukeboxService.shuffleTracks();

            Response response{ Response::createOkResponse() };
            response.addNode("jukeboxStatus", createJukeboxStatusNode(jukeboxService));
            return response;
        }

        Response handleJukeboxSetGain(RequestContext& context, jukebox::IJukeboxService& jukeboxService)
        {
            const auto gain{ getMandatoryParameterAs<float>(context.getParameters(), "gain") };
            if (gain < 0 || gain > 1)
                throw BadParameterGenericError{ "gain", "gain must be between 0.0 and 1.0" };

            jukeboxService.setVolume(gain); // consider gain is linear

            Response response{ Response::createOkResponse() };
            response.addNode("jukeboxStatus", createJukeboxStatusNode(jukeboxService));
            return response;
        }

        using Actionhandler = std::function<Response(RequestContext& context, jukebox::IJukeboxService& jukeboxService)>;
        static const std::unordered_map<std::string, Actionhandler> actionHandlers{
            { "get", detail::handleJukeboxGet },
            { "status", detail::handleJukeboxStatus },
            { "set", detail::handleJukeboxSet },
            { "start", detail::handleJukeboxStart },
            { "stop", detail::handleJukeboxStop },
            { "skip", detail::handleJukeboxSkip },
            { "add", detail::handleJukeboxAdd },
            { "clear", detail::handleJukeboxClear },
            { "remove", detail::handleJukeboxRemove },
            { "shuffle", detail::handleJukeboxShuffle },
            { "setGain", detail::handleJukeboxSetGain },
        };

    } // namespace detail

    Response handleJukeboxControl(RequestContext& context)
    {
        const std::string action{ getMandatoryParameterAs<std::string>(context.getParameters(), "action") };

        jukebox::IJukeboxService* jukeboxService{ core::Service<jukebox::IJukeboxService>::get() };
        if (!jukeboxService)
            throw InternalErrorGenericError{ "Jukebox service disabled" };

        if (!context.getUser()->isAdmin())
            throw UserNotAuthorizedError{};

        detail::initJukeboxIfNeeded(*jukeboxService);

        switch (jukeboxService->getState())
        {
        case jukebox::ServiceState::Failed:
            throw InternalErrorGenericError{ "Jukebox service failed" };

        case jukebox::ServiceState::Ready:
            break;

        default:
            throw InternalErrorGenericError{ "Bad jukebox state" };
        }

        auto itActionHandler{ detail::actionHandlers.find(action) };
        if (itActionHandler == std::end(detail::actionHandlers))
            throw BadParameterGenericError{ "action" };

        return itActionHandler->second(context, *jukeboxService);
    }
} // namespace lms::api::subsonic