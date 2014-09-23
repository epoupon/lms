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

#ifndef __MEDIA_PLAYER_WIDGET_HPP
#define __MEDIA_PLAYER_WIDGET_HPP

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

class AudioMediaPlayerWidget : public Wt::WContainerWidget
{
	public:

		AudioMediaPlayerWidget( Wt::WContainerWidget *parent = 0);

		void load(const Transcode::Parameters& parameters);

		// Signal slot Next
		Wt::Signal<void>&	playbackEnded() {return _playbackEnded;}
		// Signal Slot Previous
		// Signal slot Ended

	private:

		void handlePlayOffset(int offsetSecs);
		void handlePlayNext(void);
		void handlePlayPrev(void);
		void handleTrackEnded(void);

		void handleValueChanged(double);
		void handleTimeUpdated(void);
		void handleSliderMoved(int value);

		void handleVolumeSliderMoved(int value);

		void loadPlayer(void);

		// Signals
		Wt::Signal<void>	_playbackEnded;

		// Core
		Wt::WMediaPlayer*		_mediaPlayer;
		AvConvTranscodeStreamResource*	_mediaResource;
		Wt::WLink			_mediaInternalLink;

		// Controls
		std::shared_ptr<Transcode::Parameters>	_currentParameters;
		Wt::WPushButton* 	_playBtn;
		Wt::WPushButton* 	_pauseBtn;
		Wt::WPushButton* 	_nextBtn;
		Wt::WPushButton*	_prevBtn;
		Wt::WSlider*		_timeSlider;
		Wt::WSlider*		_volumeSlider;
		Wt::WText*		_curTime;
		Wt::WText*		_duration;

};

} // namespace UserInterface

#endif

