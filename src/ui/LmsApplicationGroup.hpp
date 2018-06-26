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

#pragma once

#include <map>
#include <mutex>

#include <Wt/WSignal.h>
#include <Wt/WEnvironment.h>

namespace UserInterface {


struct LmsApplicationInfo
{
	Wt::UserAgent userAgent;

	static LmsApplicationInfo fromEnvironment(const Wt::WEnvironment& env);
};

// LmsApplication instances are grouped by User
class LmsApplicationGroup
{
	public:
		void join(LmsApplicationInfo info);
		void leave();

		void postOthers(std::function<void()> func) const;

	private:

		mutable std::mutex _mutex;

		std::vector<std::string>  getOtherSessionIds() const;

		std::map<std::string, LmsApplicationInfo> _apps;
};

class LmsApplicationGroupContainer
{
	public:
		LmsApplicationGroup& get(Wt::WString identity);

	private:
		std::map<Wt::WString, LmsApplicationGroup> _apps;
		std::mutex _mutex;
};

} // UserInterface
