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


#endif

