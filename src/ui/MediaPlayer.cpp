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
#include <Wt/WImage>
#include <Wt/WSlider>
#include <Wt/WTemplate>
#include <Wt/WText>

#include "MediaPlayer.hpp"

namespace UserInterface {

MediaPlayer::MediaPlayer(Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent)
{
	auto player = new Wt::WTemplate(Wt::WString::tr("template-mediaplayer"), this);

	auto playPauseBtn = new Wt::WText(Wt::WString::tr("btn-mediaplayer-play"), Wt::XHTMLText);
	player->bindWidget("play-pause", playPauseBtn);

	auto prevBtn = new Wt::WText(Wt::WString::tr("btn-mediaplayer-prev"), Wt::XHTMLText);
	player->bindWidget("previous", prevBtn);

	auto nextBtn = new Wt::WText(Wt::WString::tr("btn-mediaplayer-next"), Wt::XHTMLText);
	player->bindWidget("next", nextBtn);

	auto shuffleBtn = new Wt::WText(Wt::WString::tr("btn-mediaplayer-shuffle"), Wt::XHTMLText);
	player->bindWidget("shuffle", shuffleBtn);

	auto repeatBtn = new Wt::WText(Wt::WString::tr("btn-mediaplayer-repeat"), Wt::XHTMLText);
	player->bindWidget("repeat", repeatBtn);

	player->bindString("current-time", "00:00");
	player->bindString("total-time", "00:00");

	auto cover = new Wt::WImage();
	player->bindWidget("cover", cover);

	auto trackName = new Wt::WText("---");
	player->bindWidget("track-name", trackName);

}

} // namespace UserInterface

