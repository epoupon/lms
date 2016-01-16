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

#include <Wt/WTemplate>
#include <Wt/WSignal>

#include "common/InputRange.hpp"
#include "database/Types.hpp"

namespace UserInterface {

class AudioPlayer : public Wt::WTemplate
{
	public:

		AudioPlayer(Wt::WContainerWidget *parent = 0);

		bool loadTrack(Database::Track::id_type trackId);

		// Signals
		Wt::Signal<void>&	playbackEnded() {return _playbackEnded;}
		Wt::Signal<void>&	playNext()	{return _playNext;}
		Wt::Signal<void>&	playPrevious()	{return _playPrevious;}
		Wt::Signal<bool>&	shuffle()	{return _shuffle;}
		Wt::Signal<bool>&	loop()		{return _loop;}

	private:

		// Signals
		Wt::Signal<void>	_playbackEnded;
		Wt::Signal<void>	_playNext;
		Wt::Signal<void>	_playPrevious;
		Wt::Signal<bool>	_shuffle;
		Wt::Signal<bool>	_loop;

		Wt::WAudio*	_audio;
		Wt::WText*	_trackDuration;
		Wt::WText*	_trackName;
		Wt::WImage*	_cover;
};

} // namespace UserInterface
