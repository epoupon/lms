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

#pragma once

#include <memory>

#include "utils/Exception.hpp"
#include "database/Types.hpp"

class LocalPlayerException : public ::LmsException
{
	using ::LmsException::LmsException;
};

class IAudioOutput;
class ILocalPlayer
{
	public:
		virtual ~ILocalPlayer() = default;

		virtual void			setAudioOutput(std::unique_ptr<IAudioOutput> audioOutput) = 0;
		virtual const IAudioOutput*	getAudioOutput() const = 0;

		virtual void play() = 0;
		virtual void pause() = 0;

		virtual void addTrack(Database::IdType trackId) = 0;
};

namespace Database
{
	class Db;
}

std::unique_ptr<ILocalPlayer> createLocalPlayer(Database::Db& db);

