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

#ifndef __AUDIO_MEDIA_PLAYER_HPP
#define __AUDIO_MEDIA_PLAYER_HPP

#include <memory>

#include <Wt/WSlider>
#include <Wt/WPushButton>
#include <Wt/WContainerWidget>
#include <Wt/WMediaPlayer>
#include <Wt/WLink>
#include <Wt/WText>

#include "transcode/Parameters.hpp"
#include "resource/AvConvTranscodeStreamResource.hpp"

namespace UserInterface {

class AudioMediaPlayer : public Wt::WContainerWidget
{
	public:

		AudioMediaPlayer( Wt::WContainerWidget *parent = 0);

		void load(const Transcode::Parameters& parameters);

		// Signal slot Ended
		Wt::Signal<void>&	playbackEnded() {return _playbackEnded;}
		// Signal slot Next
		Wt::Signal<void>&	playNext()	{return _playNext;}
		// Signal Slot Previous
		Wt::Signal<void>&	playPrevious()	{return _playPrevious;}

	private:

		void handlePlayOffset(int offsetSecs);
		void handleTrackEnded(void);

		void handleValueChanged(double);
		void handleTimeUpdated(void);
		void handleSliderMoved(int value);

		void handleVolumeSliderMoved(int value);

		void loadPlayer(void);

		// Signals
		Wt::Signal<void>	_playbackEnded;
		Wt::Signal<void>	_playNext;
		Wt::Signal<void>	_playPrevious;

		// Core
		Wt::WMediaPlayer*		_mediaPlayer;
		AvConvTranscodeStreamResource*	_mediaResource;
		Wt::WLink			_mediaInternalLink;

		// Controls
		std::shared_ptr<Transcode::Parameters>	_currentParameters;
		Wt::WPushButton* 	_playBtn;
		Wt::WPushButton* 	_pauseBtn;
		Wt::WSlider*		_timeSlider;
		Wt::WSlider*		_volumeSlider;
		Wt::WText*		_curTime;
		Wt::WText*		_duration;

};

} // namespace UserInterface

#endif

