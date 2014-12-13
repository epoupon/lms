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


#include <Wt/WMediaPlayer>
#include <Wt/WProgressBar>
#include <Wt/WCheckBox>
#include <Wt/WTemplate>

#include "AudioMediaPlayer.hpp"

namespace UserInterface {

AudioMediaPlayer::AudioMediaPlayer( Wt::WContainerWidget *parent)
	: Wt::WContainerWidget(parent),
	_mediaResource(nullptr)
{

	this->setStyleClass("mediaplayer");

	_mediaPlayer = new Wt::WMediaPlayer( Wt::WMediaPlayer::Audio, this );
//	_mediaPlayer->setAlternativeContent (new Wt::WText("You don't have HTML5 audio support!"));
//	_mediaPlayer->setOptions( Wt::WMediaPlayer::Autoplay );
	_mediaPlayer->addSource( Wt::WMediaPlayer::OGA, "" );

	_mediaPlayer->ended().connect(this, &AudioMediaPlayer::handleTrackEnded);

	{
		Wt::WContainerWidget *container = new Wt::WContainerWidget(this);

		Wt::WCheckBox *loop = new Wt::WCheckBox("Loop", this);
		loop->changed().connect(std::bind([=] ()
		{
			_loop.emit( loop->checkState() == Wt::Checked);
		}));

		Wt::WCheckBox *shuffle = new Wt::WCheckBox("Shuffle", this);
		shuffle->changed().connect(std::bind([=] ()
		{
			_shuffle.emit( shuffle->checkState() == Wt::Checked);
		}));

		_curTime = new Wt::WText("00:00:00", container);
		_timeSlider = new Wt::WSlider( container );
		_duration = new Wt::WText("00:00:00", container);

		Wt::WPushButton *prevBtn = new Wt::WPushButton("<<", container );
		_playBtn = new Wt::WPushButton("Play", container );
		_pauseBtn = new Wt::WPushButton("Pause", container );
		Wt::WPushButton *nextBtn = new Wt::WPushButton(">>", container );


		_volumeSlider = new Wt::WSlider( container );
		_volumeSlider->setRange(0,100);
		_volumeSlider->setValue(_mediaPlayer->volume() * 100);

		_mediaPlayer->setControlsWidget( container );
		_mediaPlayer->setButton(Wt::WMediaPlayer::Play, _playBtn);
		_mediaPlayer->setButton(Wt::WMediaPlayer::Pause, _pauseBtn);

		_mediaPlayer->setText( Wt::WMediaPlayer::CurrentTime, _curTime);
		_mediaPlayer->setText( Wt::WMediaPlayer::Duration, _duration);

		_mediaPlayer->timeUpdated().connect(this, &AudioMediaPlayer::handleTimeUpdated);

		nextBtn->clicked().connect(std::bind([=] ()
		{
			_mediaPlayer->stop();
			_playNext.emit();
		}));

		prevBtn->clicked().connect(std::bind([=] ()
		{
			_mediaPlayer->stop();
			_playPrevious.emit();
		}));
	}

	_timeSlider->valueChanged().connect(this, &AudioMediaPlayer::handlePlayOffset);
	_timeSlider->sliderMoved().connect(this, &AudioMediaPlayer::handleSliderMoved);
	_timeSlider->setDisabled(true);

	_volumeSlider->sliderMoved().connect(this, &AudioMediaPlayer::handleVolumeSliderMoved);


}


void
AudioMediaPlayer::loadPlayer(void)
{
	_mediaPlayer->clearSources();

	_mediaInternalLink.setResource( nullptr );
	if (_mediaResource)
		delete _mediaResource;

	assert( _currentParameters );
	_mediaResource = new AvConvTranscodeStreamResource( *_currentParameters, this );
	_mediaInternalLink.setResource( _mediaResource );

	_mediaPlayer->addSource( Wt::WMediaPlayer::OGA, _mediaInternalLink );
}

void
AudioMediaPlayer::load(const Transcode::Parameters& parameters)
{
	_timeSlider->setDisabled(false);

	_currentParameters = std::make_shared<Transcode::Parameters>( parameters );

	loadPlayer();

	_timeSlider->setRange(0, parameters.getInputMediaFile().getDuration().total_seconds() );
	_timeSlider->setValue(0);

	_duration->setText( boost::posix_time::to_simple_string( parameters.getInputMediaFile().getDuration() ));

	_mediaPlayer->play();
}

void
AudioMediaPlayer::handlePlayOffset(int offsetSecs)
{
	if (!_currentParameters)
		return;

	_currentParameters->setOffset( boost::posix_time::seconds(offsetSecs) );

	loadPlayer();

	_mediaPlayer->play();
}

void
AudioMediaPlayer::handleTrackEnded(void)
{
	_playbackEnded.emit();
}

void
AudioMediaPlayer::handleValueChanged(double value)
{
	// TODO
}

void
AudioMediaPlayer::handleSliderMoved(int value)
{
	;
}

void
AudioMediaPlayer::handleTimeUpdated(void)
{
	if (!_currentParameters)
		return;

	boost::posix_time::time_duration currentTime ( boost::posix_time::seconds( _mediaPlayer->currentTime() + _currentParameters->getOffset().total_seconds()));

	_timeSlider->setValue( currentTime.total_seconds() );
	_curTime->setText( boost::posix_time::to_simple_string( currentTime) );
}

void
AudioMediaPlayer::handleVolumeSliderMoved(int value)
{
	_mediaPlayer->setVolume( value / 100. );
}

} // namespace UserInterface
