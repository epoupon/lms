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

#ifndef __VIDEO_MEDIA_PLAYER_WIDGET_HPP
#define __VIDEO_MEDIA_PLAYER_WIDGET_HPP

#include <Wt/WDialog>
#include <Wt/WSlider>
#include <Wt/WPushButton>
#include <Wt/WContainerWidget>
#include <Wt/WLink>

#include "VideoParametersDialog.hpp"

#include "transcode/Parameters.hpp"
#include "resource/AvConvTranscodeStreamResource.hpp"

namespace UserInterface {

class VideoMediaPlayerWidget : public Wt::WContainerWidget
{
	public:

		VideoMediaPlayerWidget( const Transcode::Parameters& parameters, Wt::WContainerWidget *parent = 0);


		Wt::Signal<void>&	close() { return _close; };


	private:

		// Signals
		Wt::Signal<void>	_close;

		void load(const Transcode::Parameters& parameters);

		// Player controls
		void handlePlayOffset(int offsetSecs);
		void handleTimeUpdated(void);
		void handleSliderMoved(int value);
		void handleFullscreen(void);
		void handleVolumeSliderMoved(int value);

		void handleClose(void);

		void handleParametersEdit(void);
		void handleParametersDone(Wt::WDialog::DialogCode);

		// Core
		Wt::WMediaPlayer*		_mediaPlayer;
		AvConvTranscodeStreamResource*	_mediaResource;
		Wt::WLink			_mediaInternalLink;

		// Controls
		Transcode::Parameters	_currentParameters;
		Wt::WPushButton* 	_playBtn;
		Wt::WPushButton* 	_pauseBtn;
		Wt::WSlider*		_timeSlider;
		Wt::WSlider*		_volumeSlider;
		Wt::WText*		_curTime;
		Wt::WText*		_duration;

		VideoParametersDialog*	_dialog;
};

} // namespace UserInterface

#endif

