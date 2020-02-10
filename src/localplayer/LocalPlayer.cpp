/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "LocalPlayer.hpp"

#include "AudioOutput.hpp"

LocalPlayer::LocalPlayer(Database::Db& db)
{
}

void
LocalPlayer::setAudioOutput(std::unique_ptr<AudioOutput> audioOutput)
{
	_audioOutput = std::move(audioOutput);
}

const AudioOutput*
LocalPlayer::getAudioOutput() const
{
	return _audioOutput.get();
}

void
LocalPlayer::start()
{
	_audioOutput->start();
}

void
LocalPlayer::stop()
{
	_audioOutput->stop();
}

