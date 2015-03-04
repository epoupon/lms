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

#include "MobileAudioMediaPlayer.hpp"

#include "resource/AvConvTranscodeStreamResource.hpp"

namespace UserInterface {
namespace Mobile {

Wt::WMediaPlayer::Encoding
AudioMediaPlayer::getEncoding()
{
	const Wt::WEnvironment& env = Wt::WApplication::instance()->environment();

	if (env.agentIsIE())
		return Wt::WMediaPlayer::MP3;
	else
		return Wt::WMediaPlayer::OGA;
}

AudioMediaPlayer::AudioMediaPlayer(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent)
{
	_player = new Wt::WMediaPlayer(Wt::WMediaPlayer::Audio, this);
	_player->addSource( getEncoding(), "" );
}

void
AudioMediaPlayer::play(const Transcode::Parameters& parameters)
{
	AvConvTranscodeStreamResource *resource = new AvConvTranscodeStreamResource( parameters, this );

	_player->clearSources();
	_player->addSource( getEncoding(), Wt::WLink(resource));

	_player->play();
}

} // namespace UserInterface
} // namespace Mobile
