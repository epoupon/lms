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

#include "database/Db.hpp"
#include "database/Session.hpp"
#include "database/Types.hpp"
#include "localplayer/ILocalPlayer.hpp"
#include "utils/Exception.hpp"


class IAudioOutput;

class LocalPlayer final : public ILocalPlayer
{
	public:
		LocalPlayer(Database::Db& db);

		void			setAudioOutput(std::unique_ptr<IAudioOutput> audioOutput) override;
		const IAudioOutput*	getAudioOutput() const override;

		void start() override;
		void stop() override;

		void play() override {}
		void pause() override {}

		void addTrack(Database::IdType /*trackId*/) override {}

	private:

		std::unique_ptr<IAudioOutput> _audioOutput;
};

