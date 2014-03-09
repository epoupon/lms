
#include <Wt/WMediaPlayer>
#include <Wt/WProgressBar>

#include "AudioMediaPlayerWidget.hpp"


AudioMediaPlayerWidget::AudioMediaPlayerWidget( Wt::WContainerWidget *parent)
	: Wt::WContainerWidget(parent),
	_mediaResource(nullptr)
{
	_mediaPlayer = new Wt::WMediaPlayer( Wt::WMediaPlayer::Audio, this );
//	_mediaPlayer->setAlternativeContent (new Wt::WText("You don't have HTML5 audio support!"));
//	_mediaPlayer->setOptions( Wt::WMediaPlayer::Autoplay );
	_mediaPlayer->addSource( Wt::WMediaPlayer::OGA, "" );

	_mediaPlayer->ended().connect(this, &AudioMediaPlayerWidget::handleTrackEnded);

	{
		Wt::WContainerWidget *container = new Wt::WContainerWidget(this);

		_prevBtn = new Wt::WPushButton("<<", container );
		_playBtn = new Wt::WPushButton("Play", container );
		_pauseBtn = new Wt::WPushButton("Pause", container );
		_nextBtn = new Wt::WPushButton(">>", container );

		_curTime = new Wt::WText(container);
		_timeSlider = new Wt::WSlider( container );
		_duration = new Wt::WText(container);

		_volumeSlider = new Wt::WSlider( container );
		_volumeSlider->setRange(0,100);
		_volumeSlider->setValue(_mediaPlayer->volume() * 100);

		_mediaPlayer->setControlsWidget( container );
		_mediaPlayer->setButton(Wt::WMediaPlayer::Play, _playBtn);
		_mediaPlayer->setButton(Wt::WMediaPlayer::Pause, _pauseBtn);

		_mediaPlayer->setText( Wt::WMediaPlayer::CurrentTime, _curTime);
		_mediaPlayer->setText( Wt::WMediaPlayer::Duration, _duration);

		_mediaPlayer->timeUpdated().connect(this, &AudioMediaPlayerWidget::handleTimeUpdated);
	}

	_timeSlider->valueChanged().connect(this, &AudioMediaPlayerWidget::handlePlayOffset);
	_timeSlider->sliderMoved().connect(this, &AudioMediaPlayerWidget::handleSliderMoved);
	_timeSlider->setDisabled(true);

	_volumeSlider->sliderMoved().connect(this, &AudioMediaPlayerWidget::handleVolumeSliderMoved);

	_nextBtn->clicked().connect(this, &AudioMediaPlayerWidget::handlePlayNext);
	_prevBtn->clicked().connect(this, &AudioMediaPlayerWidget::handlePlayPrev);

}


void
AudioMediaPlayerWidget::loadPlayer(void)
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
AudioMediaPlayerWidget::load(const Transcode::Parameters& parameters)
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
AudioMediaPlayerWidget::handlePlayOffset(int offsetSecs)
{
	if (!_currentParameters)
		return;

	std::cout << "Want to play at offset " << offsetSecs << std::endl;;

	_currentParameters->setOffset( boost::posix_time::seconds(offsetSecs) );

	loadPlayer();

	_mediaPlayer->play();
}


void
AudioMediaPlayerWidget::handlePlayNext(void)
{
	std::cout << "Want to play next!" << std::endl;
}

void
AudioMediaPlayerWidget::handlePlayPrev(void)
{
	std::cout << "Want to play prev!" << std::endl;
}


void
AudioMediaPlayerWidget::handleTrackEnded(void)
{
	std::cout << "Track playback ended!" << std::endl;
	_playbackEnded.emit();
}

void
AudioMediaPlayerWidget::handleValueChanged(double value)
{
	std::cout << "Value changed!" << std::endl;

}

void
AudioMediaPlayerWidget::handleSliderMoved(int value)
{
	std::cout << "Slider moved to " << value << std::endl;
}

void
AudioMediaPlayerWidget::handleTimeUpdated(void)
{
	std::cout << "Time updated to " << _mediaPlayer->currentTime() << std::endl;

	if (!_currentParameters)
		return;

	boost::posix_time::time_duration currentTime ( boost::posix_time::seconds( _mediaPlayer->currentTime() + _currentParameters->getOffset().total_seconds()));

	_timeSlider->setValue( currentTime.total_seconds() );
	_curTime->setText( boost::posix_time::to_simple_string( currentTime) );
}

void
AudioMediaPlayerWidget::handleVolumeSliderMoved(int value)
{
	_mediaPlayer->setVolume( value / 100. );
}
