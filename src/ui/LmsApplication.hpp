/*
 * Copyright (C) 2013 Emeric Poupon
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

#ifndef LMS_APPLICATION_HPP
#define LMS_APPLICATION_HPP

#include <Wt/WApplication>

#include "common/SessionData.hpp"

namespace UserInterface {


class LmsApplication : public Wt::WApplication
{
	public:

		static Wt::WApplication *create(const Wt::WEnvironment& env, boost::filesystem::path dbPath);

		LmsApplication(const Wt::WEnvironment& env, boost::filesystem::path dbPath);

	private:

		void handleAuthEvent(void);
		void createFirstConnectionUI();
		void createLmsUI();

		SessionData	_sessionData;

};


} // namespace UserInterface

#endif

