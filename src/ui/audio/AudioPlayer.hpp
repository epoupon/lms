/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "common/InputRange.hpp"
#include "database/Types.hpp"

namespace UserInterface {

class AudioPlayer : public Wt::WContainerWidget
{
	public:

		AudioPlayer(Wt::WContainerWidget *parent = 0);

		void loadTrack(Database::Track::id_type trackId);

		// Slots

	private:

		Wt::WAudio*	_audio;
		Wt::WText*	_trackDuration;
		Wt::WText*	_trackName;
		Wt::WText*	_artistName;
		Wt::WText*	_releaseName;
		Wt::WImage*	_cover;
};

} // namespace UserInterface
