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

AudioMediaPlayer::AudioMediaPlayer(Wt::WMediaPlayer::Encoding encoding, Wt::WContainerWidget *parent)
:
 Wt::WContainerWidget(parent),
_player(nullptr),
_encoding(encoding)
{
	_player = new Wt::WMediaPlayer(Wt::WMediaPlayer::Audio, this);
	_player->addSource( _encoding, "" );
}

void
AudioMediaPlayer::play(const Transcode::Parameters& parameters)
{
	// FIXME memleak here
	AvConvTranscodeStreamResource *resource = new AvConvTranscodeStreamResource( parameters, this );

	_player->clearSources();
	_player->addSource( _encoding, Wt::WLink(resource));

	_player->play();
}

} // namespace UserInterface
} // namespace Mobile
