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
#include <boost/algorithm/string/replace.hpp>

#include "av/AvInfo.hpp"
#include "utils/Logger.hpp"

#include "resource/TranscodeResource.hpp"

#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"


namespace UserInterface {

MediaPlayer::MediaPlayer(Wt::WContainerWidget* parent)
: Wt::WTemplate(Wt::WString::tr("template-mediaplayer"), parent),
  playbackEnded(this, "playbackEnded"),
  playPrevious(this, "playPrevious"),
  playNext(this, "playNext")
{
	wApp->doJavaScript("LMS.mediaplayer.init(" + jsRef() + ")");
}

static std::string escape(const std::string& str)
{
	return boost::replace_all_copy(str, "\"", "\\\"");
}

void
MediaPlayer::playTrack(Database::Track::id_type trackId)
{
	LMS_LOG(UI, DEBUG) << "Playing track ID = " << trackId;

	Wt::Dbo::Transaction transaction(DboSession());
	auto track = Database::Track::getById(DboSession(), trackId);

	// Analyse track, select the best media stream
	try
	{
		Av::MediaFile mediaFile(track->getPath());

		auto resource = LmsApp->getTranscodeResource()->getUrl(trackId, Av::Encoding::MP3, boost::posix_time::seconds(0));

		std::ostringstream oss;
		oss
			<< "var params = {"
			<< " name: \"" << escape(track->getName()) + "\","
			<< " release: " << (track->getRelease() ? "\"" + escape(track->getRelease()->getName()) + "\"" : "undefined" ) << ","
			<< " artist: " << (track->getArtist() ? "\"" + escape(track->getArtist()->getName()) + "\"" : "undefined" ) << ","
			<< " resource: \"" << resource << "\","
			<< " duration: " << track->getDuration().total_seconds() << ","
			<< "};";
		oss << "LMS.mediaplayer.loadTrack(params, true)"; // true to autoplay

		LMS_LOG(UI, DEBUG) << "Runing js = '" << oss.str() << "'";

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

