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
#include <Wt/WHBoxLayout>
#include <Wt/WVBoxLayout>

#include "AudioMediaPlayer.hpp"

namespace UserInterface {

AudioMediaPlayer::AudioMediaPlayer( Wt::WContainerWidget *parent)
	: Wt::WContainerWidget(parent),
	_mediaResource(nullptr)
{
	this->setStyleClass("mediaplayer");
	this->setHeight(90);

	Wt::WVBoxLayout* mainLayout = new Wt::WVBoxLayout();
	this->setLayout(mainLayout);

	Wt::WHBoxLayout *sliderLayout = new Wt::WHBoxLayout();
	mainLayout->addLayout(sliderLayout);

	sliderLayout->addWidget(_curTime = new Wt::WText("00:00:00"));
	_curTime->setLineHeight(30);
	sliderLayout->addWidget(_timeSlider = new Wt::WSlider( ), 1);
	sliderLayout->addWidget(_duration = new Wt::WText("00:00:00"));
	_duration->setLineHeight(30);

	Wt::WHBoxLayout *controlsLayout = new Wt::WHBoxLayout();

	mainLayout->addLayout(controlsLayout);

	Wt::WContainerWidget *btnContainer = new Wt::WContainerWidget();
	// Do not allow button to wrap
	btnContainer->setMinimumSize(155, Wt::WLength::Auto);

	Wt::WTemplate *t = new Wt::WTemplate(Wt::WString::tr("mediaplayer-controls"), btnContainer);

	Wt::WPushButton *prevBtn = new Wt::WPushButton("<<");
	t->bindWidget("prev", prevBtn);
	prevBtn->setStyleClass("mediaplayer-controls");

	_playBtn = new Wt::WPushButton("Play");
	t->bindWidget("play", _playBtn);
	_playBtn->setWidth(70);
	_playBtn->setStyleClass("mediaplayer-controls");

	_pauseBtn = new Wt::WPushButton("Pause");
	t->bindWidget("pause", _pauseBtn);
	_pauseBtn->setWidth(70);
	_pauseBtn->setStyleClass("mediaplayer-controls");

	Wt::WPushButton *nextBtn = new Wt::WPushButton(">>");
	t->bindWidget("next", nextBtn);
	nextBtn->setStyleClass("mediaplayer-controls");

	controlsLayout->addWidget(btnContainer);

	_volumeSlider = new Wt::WSlider();
	_volumeSlider->setRange(0,100);
	_volumeSlider->setWidth(60);
	_volumeSlider->setMinimumSize(60, Wt::WLength::Auto);
	controlsLayout->addWidget(_volumeSlider, 1);

	Wt::WPushButton *loop = new Wt::WPushButton("Loop");
	loop->setCheckable(true);
	loop->setStyleClass("btn-xs");
	loop->checked().connect(std::bind([=] () { _loop.emit( true ); }));
	loop->unChecked().connect(std::bind([=] () { _loop.emit( false ); }));
	controlsLayout->addWidget(loop);

	Wt::WPushButton *shuffle = new Wt::WPushButton("Shuffle");
	shuffle->setCheckable(true);
	shuffle->setStyleClass("btn-xs");
	shuffle->checked().connect(std::bind([=] () { _shuffle.emit( true ); }));
	shuffle->unChecked().connect(std::bind([=] () { _shuffle.emit( false );}));
	controlsLayout->addWidget(shuffle);

	_mediaPlayer = new Wt::WMediaPlayer( Wt::WMediaPlayer::Audio, btnContainer );
	_mediaPlayer->addSource( Wt::WMediaPlayer::OGA, "" );
	_mediaPlayer->ended().connect(this, &AudioMediaPlayer::handleTrackEnded);

	_mediaPlayer->setControlsWidget( 0 );
	_mediaPlayer->setButton(Wt::WMediaPlayer::Play, _playBtn);
	_mediaPlayer->setButton(Wt::WMediaPlayer::Pause, _pauseBtn);

	_mediaPlayer->timeUpdated().connect(this, &AudioMediaPlayer::handleTimeUpdated);

	_volumeSlider->setValue(_mediaPlayer->volume() * 100);

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
	   _timeSlider->valueChanged().connect(this, &AudioMediaPlayer::handlePlayOffset);
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
