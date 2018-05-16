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

#include <Wt/WJavaScript.h>
#include <Wt/WTemplate.h>

#include "database/Types.hpp"

namespace UserInterface {

class MediaPlayer : public Wt::WTemplate
{
	public:
		MediaPlayer();

		void stop();
		void playTrack(Database::IdType trackId);

		// Signals
		Wt::JSignal<>	playbackEnded;
		Wt::JSignal<> 	playPrevious;
		Wt::JSignal<> 	playNext;

	private:

};

} // namespace UserInterface

