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


#include <Wt/WText>
#include <Wt/WAudio>
#include <Wt/WImage>
#include <Wt/WTemplate>
#include <Wt/WPushButton>

#include "common/InputRange.hpp"
#include "logger/Logger.hpp"

#include "LmsApplication.hpp"

#include "AudioPlayer.hpp"

namespace UserInterface {

bool
AudioPlayer::loadTrack(Database::Track::id_type trackId)
{
	Wt::Dbo::Transaction transaction(DboSession());

	Database::Track::pointer track = Database::Track::getById(DboSession(), trackId);
	if (!track)
	{
		LMS_LOG(UI, INFO) << "No track found for id " << trackId;
		return false;
	}

	bindString("track", Wt::WString::fromUTF8(track->getName()));
	bindString("artist", Wt::WString::fromUTF8(track->getArtist()->getName()));
	_cover->setImageLink(SessionCoverResource()->getTrackUrl(trackId, 64));
	_trackDuration->setText( boost::posix_time::to_simple_string( track->getDuration() ));

	// Analyse track, select the best media stream
	Av::MediaFile mediaFile(track->getPath());

	if (!mediaFile.open() || !mediaFile.scan())
	{
		LMS_LOG(UI, ERROR) << "Cannot open file '" << track->getPath();
		return false;
	}


	int audioBestStreamId = mediaFile.getBestStreamId(Av::Stream::Type::Audio);
	std::vector<std::size_t> streams;
	if (audioBestStreamId != -1)
		streams.push_back(audioBestStreamId);

	this->doJavaScript("\
			document.lms.audio.state = \"loaded\";\
			document.lms.audio.seekbar.min = " + std::to_string(0) + ";\
			document.lms.audio.seekbar.max = " + std::to_string(track->getDuration().total_seconds()) + ";\
			document.lms.audio.seekbar.value = 0;\
			document.lms.audio.seekbar.disabled = false;\
			document.lms.audio.offset = 0;\
			document.lms.audio.curTime = 0;\
			");

	//TODO, try to load everything in JS in order to prevent the WriteError bug?
	_audio->pause();
	_audio->clearSources();
	//TODO, encoding
	_audio->addSource(SessionTranscodeResource()->getUrl(trackId, Av::Encoding::MP3, 0, streams));
	_audio->setPreloadMode(Wt::WAudio::PreloadAuto);
	_audio->play();

	return true;
}

AudioPlayer::AudioPlayer(Wt::WContainerWidget *parent)
: Wt::WTemplate(parent)
{
	setTemplateText(Wt::WString::tr("wa-audio-player"));
	addStyleClass("mediaplayer");

	// TODO potential leak here
	_audio = new Wt::WAudio();
	_audio->setOptions(Wt::WAudio::Autoplay);
	_audio->setPreloadMode(Wt::WAudio::PreloadAuto);
	bindWidget("audio", _audio);

	_audio->ended().connect(std::bind([=] ()
	{
		_playbackEnded.emit();
	}));

	_cover = new Wt::WImage();
	bindWidget("cover", _cover);
	_cover->setImageLink(SessionCoverResource()->getUnknownTrackUrl(64));

	InputRange *seekbar = new InputRange();
	bindWidget("seekbar", seekbar);

	bindString("track", "Track");
	bindString("artist", "Artist");

	InputRange *volumeSlider = new InputRange();
	volumeSlider->addStyleClass("mediaplayer-volume");
	volumeSlider->setAttributeValue("orient", "vertical"); // firefox
	bindWidget("volume", volumeSlider);

	Wt::WText *playlistBtn = new Wt::WText("<i class=\"fa fa-list fa-2x\"></i>", Wt::XHTMLText);
	playlistBtn->addStyleClass("mediaplayer-btn");
	bindWidget("playlist", playlistBtn);

	Wt::WText *repeatBtn = new Wt::WText("<i class=\"fa fa-repeat fa-2x\"></i>", Wt::XHTMLText);
	repeatBtn->addStyleClass("mediaplayer-btn");
	bindWidget("repeat", repeatBtn);
	repeatBtn->clicked().connect(std::bind([=] ()
	{
		if (repeatBtn->hasStyleClass("mediaplayer-btn-active"))
		{
			repeatBtn->removeStyleClass("mediaplayer-btn-active");
			_loop.emit(false);
		}
		else
		{
			repeatBtn->addStyleClass("mediaplayer-btn-active");
			_loop.emit(true);
		}
	}));

	Wt::WText *shuffleBtn = new Wt::WText("<i class=\"fa fa-random fa-2x\"></i>", Wt::XHTMLText);
	shuffleBtn->addStyleClass("mediaplayer-btn");
	bindWidget("shuffle", shuffleBtn);
	shuffleBtn->clicked().connect(std::bind([=] ()
	{
		if (shuffleBtn->hasStyleClass("mediaplayer-btn-active"))
		{
			shuffleBtn->removeStyleClass("mediaplayer-btn-active");
			_shuffle.emit(false);
		}
		else
		{
			shuffleBtn->addStyleClass("mediaplayer-btn-active");
			_shuffle.emit(true);
		}
	}));

	Wt::WText *prevBtn = new Wt::WText("<i class=\"fa fa-step-backward fa-2x\"></i>", Wt::XHTMLText);
	prevBtn->addStyleClass("mediaplayer-btn hidden-xs");
	bindWidget("prev", prevBtn);
	prevBtn->clicked().connect(std::bind([=] ()
	{
		_playPrevious.emit();
	}));

	Wt::WText *nextBtn = new Wt::WText("<i class=\"fa fa-step-forward fa-2x\"></i>", Wt::XHTMLText);
	nextBtn->addStyleClass("mediaplayer-btn");
	bindWidget("next", nextBtn);
	nextBtn->clicked().connect(std::bind([=] ()
	{
		_playNext.emit();
	}));

	Wt::WText *playPauseBtn = new Wt::WText("<i class=\"fa fa-play fa-3x fa-fw\"></i>", Wt::XHTMLText);
	playPauseBtn->addStyleClass("mediaplayer-btn");
	bindWidget("play-pause", playPauseBtn);

	Wt::WText *trackCurrentTime = new Wt::WText("00:00");
	trackCurrentTime->addStyleClass("hidden-xs");
	bindWidget("curtime", trackCurrentTime);

	_trackDuration = new Wt::WText("00:00");
	_trackDuration->addStyleClass("hidden-xs");
	bindWidget("duration", _trackDuration);

	this->doJavaScript(
	"\
		document.lms = {};\
		document.lms.audio = {};\
		document.lms.audio.audio = " + _audio->jsRef() + ";\
		document.lms.audio.seekbar = " + seekbar->jsRef() +";\
		document.lms.audio.volumeSlider = " + volumeSlider->jsRef() + ";\
		document.lms.audio.curTimeText = " + trackCurrentTime->jsRef() + ";\
		document.lms.audio.playPause = " + playPauseBtn->jsRef() + ";\
	\
		document.lms.audio.offset = 0;\
		document.lms.audio.curTime = 0;\
		document.lms.audio.state = \"init\";\
		document.lms.audio.volume = 1;\
	\
		document.lms.audio.seekbar.value = 0;\
		document.lms.audio.seekbar.disabled = true;\
	\
		document.lms.audio.volumeSlider.min = 0;\
		document.lms.audio.volumeSlider.max = 100;\
		document.lms.audio.volumeSlider.value = 100;\
	\
		function updateUIPlaying() { \
			var icon = document.lms.audio.playPause.getElementsByTagName(\"i\")[0]; \
			icon.className = \"fa fa-pause fa-3x fa-fw\"; \
		} \
	\
		function updateUIStopped() { \
			var icon = document.lms.audio.playPause.getElementsByTagName(\"i\")[0]; \
			icon.className = \"fa fa-play fa-3x fa-fw\"; \
		} \
	\
		function updateUI() {\
			document.lms.audio.curTimeText.innerHTML = document.lms.audio.curTime;\
			document.lms.audio.seekbar.value = document.lms.audio.curTime;\
		}\
	\
		var mouseDown = 0;\
		function seekMouseDown(e) {\
			++mouseDown;\
		}\
		function seekMouseUp(e) {\
			--mouseDown;\
		}\
	\
		function seeking(e) {\
			if (document.lms.audio.state == \"init\")\
				return;\
	\
			document.lms.audio.curTimeText.innerHTML = document.lms.audio.seekbar.value;\
		}\
	\
		function seek(e) {\
			if (document.lms.audio.state == \"init\")\
				return;\
	\
			document.lms.audio.audio.pause(); \
			document.lms.audio.offset = parseInt(document.lms.audio.seekbar.value);\
			document.lms.audio.curTime = document.lms.audio.seekbar.value;\
			var audioSource = document.lms.audio.audio.getElementsByTagName(\"source\")[0];\
			var src = audioSource.src;\
			src = src.slice(0, src.lastIndexOf(\"=\") + 1);\
			audioSource.src = src + document.lms.audio.seekbar.value;\
			document.lms.audio.audio.load(); \
			document.lms.audio.audio.play(); \
			document.lms.audio.curTimeText.innerHTML = ~~document.lms.audio.curTime + \"        \";\
		}\
	\
		function volumeChanged() {\
			document.lms.audio.audio.volume = document.lms.audio.volumeSlider.value / 100;\
		}\
	\
		function updateCurTime() {\
			document.lms.audio.curTime = document.lms.audio.offset + ~~document.lms.audio.audio.currentTime; \
				if (mouseDown == 0)\
					updateUI();\
		} \
	\
		function playPause() { \
			if (document.lms.audio.state == \"init\") \
				return;\
	\
			if (document.lms.audio.audio.paused) \
				document.lms.audio.audio.play(); \
			else \
				document.lms.audio.audio.pause();\
	\
		}\
	\
		document.lms.audio.audio.addEventListener('timeupdate', updateCurTime); \
		document.lms.audio.audio.addEventListener('playing', updateUIPlaying); \
		document.lms.audio.audio.addEventListener('play', updateUIPlaying); \
		document.lms.audio.audio.addEventListener('pause', updateUIStopped); \
		document.lms.audio.audio.addEventListener('ended', updateUIStopped); \
		document.lms.audio.seekbar.addEventListener('change', seek);\
		document.lms.audio.seekbar.addEventListener('input', seeking);\
		document.lms.audio.seekbar.addEventListener('mousedown', seekMouseDown);\
		document.lms.audio.seekbar.addEventListener('mouseup', seekMouseUp);\
		document.lms.audio.volumeSlider.addEventListener('input', volumeChanged);\
		document.lms.audio.playPause.addEventListener('click', playPause);\
	"
	);
}


} // namespaceUserInterface
