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

