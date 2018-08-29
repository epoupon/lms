/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "MediaPlayer.hpp"

#include "av/AvInfo.hpp"
#include "utils/Logger.hpp"

#include "database/Track.hpp"


#include "resource/ImageResource.hpp"
#include "resource/TranscodeResource.hpp"

#include "LmsApplication.hpp"

namespace UserInterface {

MediaPlayer::MediaPlayer()
: Wt::WTemplate(Wt::WString::tr("Lms.MediaPlayer.template")),
  playbackEnded(this, "playbackEnded"),
  playPrevious(this, "playPrevious"),
  playNext(this, "playNext")
{
	_title = bindNew<Wt::WText>("title");
	_title->setTextFormat(Wt::TextFormat::Plain);

	_artist = bindNew<Wt::WAnchor>("artist");
	_artist->setTextFormat(Wt::TextFormat::Plain);

	_release = bindNew<Wt::WAnchor>("release");
	_release->setTextFormat(Wt::TextFormat::Plain);

	wApp->doJavaScript("LMS.mediaplayer.init(" + jsRef() + ")");
}

void
MediaPlayer::playTrack(Database::IdType trackId)
{
	LMS_LOG(UI, DEBUG) << "Playing track ID = " << trackId;

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());
	auto track = Database::Track::getById(LmsApp->getDboSession(), trackId);

	try
	{
		Av::MediaFile mediaFile(track->getPath());

		auto resource = LmsApp->getTranscodeResource()->getUrl(trackId, Av::Encoding::MP3);
		auto imgResource = LmsApp->getImageResource()->getTrackUrl(trackId, 64);

		std::ostringstream oss;
		oss
			<< "var params = {"
			<< " resource: \"" << resource << "\","
			<< " duration: " << std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count() << ","
			<< " imgResource: \"" << imgResource << "\","
			<< "};";
		oss << "LMS.mediaplayer.loadTrack(params, true)"; // true to autoplay

		LMS_LOG(UI, DEBUG) << "Runing js = '" << oss.str() << "'";

		_title->setText(Wt::WString::fromUTF8(track->getName()));

		if (track->getArtist())
		{
			_artist->setText(Wt::WString::fromUTF8(track->getArtist()->getName()));
			_artist->setLink(LmsApp->createArtistLink(track->getArtist()));
		}

		if (track->getRelease())
		{
			_release->setText(Wt::WString::fromUTF8(track->getRelease()->getName()));
			_release->setLink(LmsApp->createReleaseLink(track->getRelease()));
		}

		wApp->doJavaScript(oss.str());
	}
	catch (Av::MediaFileException& e)
	{
		LMS_LOG(UI, ERROR) << "MediaFileException: " << e.what();
	}
}

void
MediaPlayer::stop()
{
	wApp->doJavaScript("LMS.mediaplayer.stop()");
}

} // namespace UserInterface

