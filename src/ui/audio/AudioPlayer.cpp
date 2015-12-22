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

#include "LmsApplication.hpp"

#include "AudioPlayer.hpp"

namespace UserInterface {

void
AudioPlayer::loadTrack(Database::Track::id_type trackId)
{
	Wt::Dbo::Transaction transaction(DboSession());

	Database::Track::pointer track = Database::Track::getById(DboSession(), trackId);
	if (!track)
	{
		std::cerr << "no track for this id!" << std::endl;
		return;
	}

	_trackName->setText(Wt::WString::fromUTF8(track->getName()));
	_artistName->setText( Wt::WString::fromUTF8(track->getArtist()->getName()));
	_releaseName->setText( Wt::WString::fromUTF8(track->getRelease()->getName()));
	_cover->setImageLink(SessionCoverResource()->getTrackUrl(trackId, 64));
	_trackDuration->setText( boost::posix_time::to_simple_string( track->getDuration() ));

	// Analyse track, select the best media stream
	Av::MediaFile mediaFile(track->getPath());

	if (!mediaFile.open() || !mediaFile.scan())
	{
		std::cerr << "cannot open file '" << track->getPath() << std::endl;
		return;
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
	//TODOencoding
	_audio->addSource(SessionTranscodeResource()->getUrl(trackId, Av::Encoding::MP3, 0, streams));
	_audio->play();
}

AudioPlayer::AudioPlayer(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent)
{
	Wt::WTemplate *t = new Wt::WTemplate(this);
	t->setTemplateText(Wt::WString::tr("wa-audio-player"));

	_audio = new Wt::WAudio(this);

	_cover = new Wt::WImage();
	t->bindWidget("cover", _cover);
	_cover->setImageLink(SessionCoverResource()->getUnknownTrackUrl(64));

	InputRange *seekbar = new InputRange();
	t->bindWidget("seekbar", seekbar);

	_trackName = new Wt::WText();
	t->bindWidget("track", _trackName);

	_artistName = new Wt::WText();
	t->bindWidget("artist", _artistName);

	_releaseName = new Wt::WText();
	t->bindWidget("release", _releaseName);

	InputRange *volumeSlider = new InputRange();
	t->bindWidget("volume", volumeSlider);

	Wt::WPushButton *playlistBtn = new Wt::WPushButton("<i class=\"fa fa-list fa-lg\"></i>", Wt::XHTMLText);
	t->bindWidget("playlist", playlistBtn);

	Wt::WPushButton *repeatBtn = new Wt::WPushButton("<i class=\"fa fa-repeat fa-lg\"></i>", Wt::XHTMLText);
	t->bindWidget("repeat", repeatBtn);

	Wt::WPushButton *shuffleBtn = new Wt::WPushButton("<i class=\"fa fa-random fa-lg\"></i>", Wt::XHTMLText);
	t->bindWidget("shuffle", shuffleBtn);

	Wt::WPushButton *prevBtn = new Wt::WPushButton("<i class=\"fa fa-step-backward fa-lg\"></i>", Wt::XHTMLText);
	t->bindWidget("prev", prevBtn);

	Wt::WPushButton *nextBtn = new Wt::WPushButton("<i class=\"fa fa-step-forward fa-lg\"></i>", Wt::XHTMLText);
	t->bindWidget("next", nextBtn);

	Wt::WPushButton *playPauseBtn = new Wt::WPushButton("<i class=\"fa fa-play fa-lg\"></i>", Wt::XHTMLText);
	t->bindWidget("play-pause", playPauseBtn);

	Wt::WText *trackCurrentTime = new Wt::WText("00:00");
	t->bindWidget("curtime", trackCurrentTime);

	_trackDuration = new Wt::WText("00:00");
	t->bindWidget("duration", _trackDuration);

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
		function playPause() {\
			if (document.lms.audio.state == \"init\") \
				return;\
					\
					if (document.lms.audio.audio.paused)\
						document.lms.audio.audio.play();\
					else\
						document.lms.audio.audio.pause();\
							\
		}\
	\
		document.lms.audio.audio.addEventListener('timeupdate', updateCurTime); \
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
