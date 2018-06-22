/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "LmsApplicationGroup.hpp"

#include <algorithm>

#include <Wt/WServer.h>
#include <Wt/WApplication.h>


namespace UserInterface {

LmsApplicationInfo
LmsApplicationInfo::fromEnvironment(const Wt::WEnvironment& env)
{
	LmsApplicationInfo info = {.userAgent = wApp->environment().agent()};
	return info;
}

void
LmsApplicationGroup::join(LmsApplicationInfo info)
{

	_apps.emplace(wApp->sessionId(), std::move(info));
}

void
LmsApplicationGroup::leave()
{
	_apps.erase(wApp->sessionId());
}

std::vector<std::string>
LmsApplicationGroup::getOtherSessionIds() const
{
	std::vector<std::string> res;

	for (auto const& app : _apps)
	{
		if (app.first != wApp->sessionId())
			res.push_back(app.first);
	}

	return res;
}

void
LmsApplicationGroup::postOthers(std::function<void()> func) const
{
	for (auto const& sessionId : getOtherSessionIds())
	{
		Wt::WServer::instance()->post(sessionId, [=]
		{
			func();
			wApp->triggerUpdate();
		});
	}
}


} // UserInterface
