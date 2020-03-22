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

#include "Stream.hpp"

#include "database/Session.hpp"
#include "database/User.hpp"
#include "utils/Service.hpp"
#include "utils/Utils.hpp"
#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"
#include "localplayer/ILocalPlayer.hpp"

namespace API::Subsonic::Jukebox
{

enum class Action
{
	Get,
	Status,
	Set,
	Start,
	Stop,
	Skip,
	Add,
	Clear,
	Remove,
	Shuffle,
	SetGain,
};

} // ns API::Subsonic::Jukebox

namespace StringUtils
{
template<>
std::optional<API::Subsonic::Jukebox::Action>
StringUtils::readAs(const std::string& str)
{

	using namespace API::Subsonic::Jukebox;

	static const std::unordered_map<std::string_view, Action> actions
	{
		{"get",		Action::Get},
		{"status",	Action::Status},
		{"set",		Action::Set},
		{"start",	Action::Start},
		{"stop",	Action::Stop},
		{"skip",	Action::Skip},
		{"add",		Action::Add},
		{"clear",	Action::Clear},
		{"remove",	Action::Remove},
		{"shuffle",	Action::Shuffle},
		{"setGain",	Action::SetGain},
	};

	auto itAction {actions.find(str)};
	if (itAction == std::cend(actions))
		return std::nullopt;

	return itAction->second;
}
} // ns StringUtils

namespace API::Subsonic::Jukebox
{

static
Response
createStatusReponse()
{
	Response response {Response::createOkResponse()};
	Response::Node& statusResponse {response.createNode("jukeboxStatus")};

	ILocalPlayer::Status playerStatus {Service<ILocalPlayer>::get()->getStatus()};
	statusResponse.setAttribute("currentIndex", std::to_string(playerStatus.entryIdx ? *playerStatus.entryIdx : 0));
	statusResponse.setAttribute("playing", playerStatus.playState == ILocalPlayer::Status::PlayState::Playing ? "true" : "false");
	statusResponse.setAttribute("gain", "1.0");
	if (playerStatus.currentPlayTime)
		statusResponse.setAttribute("position", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(*playerStatus.currentPlayTime).count()));

	return response;
}

static
Response
handleAdd(const std::vector<Id>& trackIds)
{
	for (Id trackId : trackIds)
		Service<ILocalPlayer>::get()->addTrack(trackId.value);

	return createStatusReponse();
}

static
Response
handleClear()
{
	Service<ILocalPlayer>::get()->clearTracks();
	return createStatusReponse();
}

static
Response
handleSet(const std::vector<Id>& trackIds)
{
	Service<ILocalPlayer>::get()->clearTracks();

	for (Id trackId : trackIds)
		Service<ILocalPlayer>::get()->addTrack(trackId.value);

	return createStatusReponse();
}

static
Response
handleStart()
{
	Service<ILocalPlayer>::get()->play();

	return createStatusReponse();
}

static
Response
handleStop()
{
	Service<ILocalPlayer>::get()->pause();

	return createStatusReponse();
}

static
Response
handleSkip(int skip, std::optional<int> offset)
{
	Service<ILocalPlayer>::get()->playEntry(skip, std::chrono::milliseconds {offset ? *offset * 1000 : 0});
		
	return createStatusReponse();
}

Response
handle(RequestContext& context)
{
	const Action action {getMandatoryParameterAs<Action>(context.parameters, "action")};
	std::optional<int> index {getParameterAs<int>(context.parameters, "index")};
	if (index && *index < 0)
		throw BadParameterGenericError {"index"};

	std::optional<int> offset {getParameterAs<int>(context.parameters, "offset")};
	if (offset && *offset < 0)
		throw BadParameterGenericError {"offset"};

	std::vector<Id> trackIds {getMultiParametersAs<Id>(context.parameters, "id")};
	if (!std::all_of(std::cbegin(trackIds), std::cend(trackIds ), [](const Id& id) { return id.type == Id::Type::Track; }))
		throw BadParameterGenericError {"id"};

	std::optional<float> gain {getParameterAs<float>(context.parameters, "gain")};
	if (gain && (*gain < 0 || *gain > 1))
		throw BadParameterGenericError {"gain"};

	switch (action)
	{
		case Action::Add:
			return handleAdd(trackIds);
		case Action::Clear:
			return handleClear();
		case Action::Set:
			return handleSet(trackIds);

		case Action::Start:
			return handleStart();

		case Action::Stop:
			return handleStop();

		case Action::Skip:
			if (!index)
				throw RequiredParameterMissingError {};

			return handleSkip(*index, offset);

		case Action::Status:
			return createStatusReponse();

		case Action::Get:
		case Action::Remove:
		case Action::Shuffle:
		case Action::SetGain:
			throw NotImplementedGenericError {};
	}

	throw NotImplementedGenericError {};
}

}

