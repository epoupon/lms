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

#include <Wt/WApplication>
#include <Wt/WEnvironment>
#include <Wt/WTemplate>
#include <Wt/WPushButton>

#include "MobileAudioMediaPlayer.hpp"

#include "resource/AvConvTranscodeStreamResource.hpp"

namespace UserInterface {
namespace Mobile {

Wt::WMediaPlayer::Encoding
AudioMediaPlayer::getBestEncoding()
{
	// MP3 seems to be better supported everywhere
	return Wt::WMediaPlayer::MP3;
}

AudioMediaPlayer::AudioMediaPlayer(Database::Handler& db, Wt::WMediaPlayer::Encoding encoding, Wt::WContainerWidget *parent)
:
 Wt::WContainerWidget(parent),
_player(nullptr),
_encoding(encoding)
{
	_coverResource = new CoverResource(db, 56);

	Wt::WTemplate* container  = new Wt::WTemplate(this);
	container->setTemplateText(Wt::WString::tr("mobile-audio-player"));

	Wt::WPushButton *prev = new Wt::WPushButton("<<");
	container->bindWidget("prev", prev);

	Wt::WPushButton *play = new Wt::WPushButton("Play");
	container->bindWidget("play", play);

	Wt::WPushButton *pause = new Wt::WPushButton("Pause");
	container->bindWidget("pause", pause);

	Wt::WPushButton *next = new Wt::WPushButton(">>");
	container->bindWidget("next", next);

	_cover = new Wt::WImage();
	container->bindWidget("cover", _cover);

	Wt::WSlider *volume = new Wt::WSlider();
	container->bindWidget("volume", volume);

	_player = new Wt::WMediaPlayer(Wt::WMediaPlayer::Audio, this);
	_player->addSource( _encoding, "" );
	_player->setControlsWidget( 0 );
	_player->setButton(Wt::WMediaPlayer::Play, play);
	_player->setButton(Wt::WMediaPlayer::Pause, pause);

}

void
AudioMediaPlayer::play(Database::Track::id_type trackId, const Transcode::Parameters& parameters)
{
	// FIXME memleak here
	AvConvTranscodeStreamResource *resource = new AvConvTranscodeStreamResource( parameters, this );

	_player->clearSources();
	_player->addSource( _encoding, Wt::WLink(resource));

	_player->play();

	_cover->setImageLink( Wt::WLink (_coverResource->getTrackUrl(trackId)));
}

} // namespace UserInterface
} // namespace Mobile
