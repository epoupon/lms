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

#include <map>

#include <Wt/WStackedWidget>
#include <Wt/WTemplate>
#include <Wt/WText>

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

#include "audio/AudioPlayer.hpp"
#include "LmsApplication.hpp"

#include "ArtistSearchView.hpp"
#include "PreviewSearchView.hpp"
#include "ReleaseSearch.hpp"
#include "TrackSearch.hpp"
#include "ArtistView.hpp"
#include "ReleaseView.hpp"

#include "MobileAudio.hpp"


namespace UserInterface {
namespace Mobile {

enum WidgetIdx
{
	WidgetIdxSearchPreview	= 0,
	WidgetIdxSearchArtist	= 1,
	WidgetIdxSearchRelease	= 2,
	WidgetIdxSearchTrack	= 3,
	WidgetIdxArtist		= 4,
	WidgetIdxRelease	= 5,
};

using namespace Database;

static void playTrack(AudioPlayer *audioPlayer, Database::Track::id_type trackId)
{
	LMS_LOG(UI, DEBUG) << "Playing track id " << trackId;

	Wt::Dbo::Transaction transaction(DboSession());

	Track::pointer track = Track::getById(DboSession(), trackId);

	if (track)
		audioPlayer->loadTrack(track.id());
}

void
Audio::search(std::string text)
{
	wApp->setInternalPath("/audio/search/preview/" + stringToUTF8(text), true);
}

Audio::Audio(Wt::WContainerWidget *parent)
: UserInterface::Audio(parent)
{
	// Root div has to be a "container"
	this->setStyleClass("container-fluid");
	this->setPadding(60, Wt::Bottom);

	Wt::WStackedWidget *stack = new Wt::WStackedWidget(this);

	// Same order as WidgetIdxXXX
	stack->addWidget(new PreviewSearchView());
	stack->addWidget(new ArtistSearchView());
	stack->addWidget(new ReleaseSearch());
	stack->addWidget(new TrackSearch());
	stack->addWidget(new ArtistView());
	stack->addWidget(new ReleaseView());

	wApp->internalPathChanged().connect(std::bind([=] (std::string path)
	{
		// order is important
		static std::map<std::string, int> indexes =
		{
			{ "/audio/search/preview", WidgetIdxSearchPreview},
			{ "/audio/search/artist", WidgetIdxSearchArtist},
			{ "/audio/search/release", WidgetIdxSearchRelease},
			{ "/audio/search/track", WidgetIdxSearchTrack},
			{ "/audio/artist", WidgetIdxArtist},
			{ "/audio/release", WidgetIdxRelease},
		};

		for (auto index : indexes)
		{
			if (wApp->internalPathMatches(index.first))
				stack->setCurrentIndex(index.second);
		}

	}, std::placeholders::_1));

	Wt::WTemplate* footer = new Wt::WTemplate(this);
	footer->setTemplateText(Wt::WString::tr("mobile-audio-footer"));

	AudioPlayer* audioPlayer = new AudioPlayer();
	footer->bindWidget("player", audioPlayer);

}

} // namespace Mobile
} // namespace UserInterface

