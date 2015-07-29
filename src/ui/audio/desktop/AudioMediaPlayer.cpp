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

#include "logger/Logger.hpp"

#include "LmsApplication.hpp"

#include "AudioMediaPlayer.hpp"

namespace UserInterface {
namespace Desktop {

Wt::WMediaPlayer::Encoding
AudioMediaPlayer::getBestEncoding()
{
	// MP3 seems to be better supported everywhere
	return Wt::WMediaPlayer::MP3;
}

AudioMediaPlayer::AudioMediaPlayer(Wt::WContainerWidget *parent)
	: Wt::WContainerWidget(parent),
	_mediaResource(nullptr)
{
	this->setStyleClass("mediaplayer");

	Wt::WVBoxLayout* mainLayout = new Wt::WVBoxLayout();
	this->setLayout(mainLayout);

	// Determine the encoding to be used
	{
		Wt::Dbo::Transaction transaction(DboSession());
		switch (CurrentUser()->getAudioEncoding())
		{
			case Database::AudioEncoding::MP3: _encoding = Wt::WMediaPlayer::MP3; break;
			case Database::AudioEncoding::WEBMA: _encoding = Wt::WMediaPlayer::WEBMA; break;
			case Database::AudioEncoding::OGA: _encoding = Wt::WMediaPlayer::OGA; break;
			case Database::AudioEncoding::FLA: _encoding = Wt::WMediaPlayer::FLA; break;
			case Database::AudioEncoding::AUTO:
			default:
				_encoding = getBestEncoding();
		}
	}

	LMS_LOG(MOD_UI, SEV_INFO) << "Audio player using encoding " << _encoding;

	// Current Media info
	Wt::WHBoxLayout *currentMediaLayout = new Wt::WHBoxLayout();
	mainLayout->addLayout(currentMediaLayout, 1);

	currentMediaLayout->addWidget( _mediaCover = new Wt::WImage());
	_mediaCover->setImageLink( LmsApplication::instance()->getCoverResource()->getUnknownTrackUrl(72));
	_mediaCover->setStyleClass("mediaplayer-current-cover");

	Wt::WVBoxLayout* mediaInfoLayout = new Wt::WVBoxLayout();
	currentMediaLayout->addLayout(mediaInfoLayout, 1);

	mediaInfoLayout->addWidget( _mediaTitle = new Wt::WText("---"));
	mediaInfoLayout->addWidget( _mediaArtistRelease = new Wt::WText("---"));
	_mediaTitle->setStyleClass("mediaplayer-current-track vertical-align");
	_mediaArtistRelease->setStyleClass("mediaplayer-current-artist vertical-align");

	// Time control
	Wt::WHBoxLayout *sliderLayout = new Wt::WHBoxLayout();
	mainLayout->addLayout(sliderLayout);

	sliderLayout->addWidget(_curTime = new Wt::WText("00:00:00"));
	sliderLayout->addWidget(_timeSlider = new Wt::WSlider( ), 1);
	sliderLayout->addWidget(_duration = new Wt::WText("00:00:00"));
	_timeSlider->setHeight(26); // Default is too big (50)
	_curTime->setStyleClass("vertical-align");
	_duration->setStyleClass("vertical-align");

	// Controls
	Wt::WHBoxLayout *controlsLayout = new Wt::WHBoxLayout();
	mainLayout->addLayout(controlsLayout);


	Wt::WPushButton *prevBtn = new Wt::WPushButton("<<");
	controlsLayout->addWidget(prevBtn);
	prevBtn->setStyleClass("mediaplayer-btn-controls");

	Wt::WContainerWidget *btnContainer = new Wt::WContainerWidget();;

	_playBtn = new Wt::WPushButton("Play");
	btnContainer->addWidget(_playBtn);
	_playBtn->setWidth(70);
	_playBtn->setStyleClass("mediaplayer-btn-controls");

	_pauseBtn = new Wt::WPushButton("Pause");
	btnContainer->addWidget(_pauseBtn);
	_pauseBtn->setWidth(70);
	_pauseBtn->setStyleClass("mediaplayer-btn-controls");

	controlsLayout->addWidget(btnContainer);

	Wt::WPushButton *nextBtn = new Wt::WPushButton(">>");
	controlsLayout->addWidget(nextBtn);
	nextBtn->setStyleClass("mediaplayer-btn-controls");

	_volumeSlider = new Wt::WSlider();
	_volumeSlider->setRange(0,100);
	_volumeSlider->setWidth(60); // Default is too big (150)
	_volumeSlider->setHeight(26); // Default is too big (50)
	_volumeSlider->setMinimumSize(50, Wt::WLength::Auto);
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
	_mediaPlayer->addSource( _encoding, "" );
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

	if (_mediaResource)
		delete _mediaResource;

	assert( _currentParameters );
	_mediaResource = new AvConvTranscodeStreamResource( *_currentParameters, this );

	_mediaPlayer->addSource( getEncoding(), Wt::WLink(_mediaResource));
}

void
AudioMediaPlayer::load(Database::Track::id_type trackId)
{
	std::size_t bitrate = 0;
	boost::filesystem::path trackPath;

	{
		Wt::Dbo::Transaction transaction(DboSession());

		Database::Track::pointer track = Database::Track::getById(DboSession(), trackId);

		bitrate = CurrentUser()->getAudioBitrate();
		trackPath = track->getPath();
		_mediaTitle->setText ( Wt::WString::fromUTF8(track->getName()) );
		_mediaArtistRelease->setText ( Wt::WString::fromUTF8(track->getArtist()->getName()) + " - " + Wt::WString::fromUTF8(track->getRelease()->getName()) );
		_mediaCover->setImageLink( Wt::WLink (LmsApplication::instance()->getCoverResource()->getTrackUrl(trackId, 72)));
	}

	Transcode::Format::Encoding encoding;
	switch (_encoding)
	{
		case Wt::WMediaPlayer::MP3: encoding = Transcode::Format::MP3; break;
		case Wt::WMediaPlayer::FLA: encoding = Transcode::Format::FLA; break;
		case Wt::WMediaPlayer::OGA: encoding = Transcode::Format::OGA; break;
		case Wt::WMediaPlayer::WEBMA: encoding = Transcode::Format::WEBMA; break;
		default:
					      encoding = Transcode::Format::MP3;
	}

	_timeSlider->setDisabled(false);

	try
	{
		Transcode::Parameters parameters(trackPath, Transcode::Format::get(encoding));
		parameters.setBitrate(Transcode::Stream::Audio, bitrate);

		_currentParameters = std::make_shared<Transcode::Parameters>( parameters );
	}
	catch(std::exception &e)
	{
		LMS_LOG(MOD_UI, SEV_ERROR) << "Cannot load input file '" << trackPath << "'";
		return;
	}

	loadPlayer();

	_timeSlider->setRange(0, _currentParameters->getInputMediaFile().getDuration().total_seconds() );
	_timeSlider->setValue(0);

	_duration->setText( boost::posix_time::to_simple_string( _currentParameters->getInputMediaFile().getDuration() ));

	// Auto play
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

} // namespace Desktop
} // namespace UserInterface
