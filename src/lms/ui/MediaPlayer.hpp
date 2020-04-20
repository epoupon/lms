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

#pragma once

#include <Wt/WAnchor.h>
#include <Wt/WJavaScript.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include "database/Types.hpp"
#include "database/User.hpp"

namespace UserInterface {

class MediaPlayer : public Wt::WTemplate
{
	public:
		using Bitrate = Database::Bitrate;
		using Format = Database::AudioFormat;

		// Do not change this enum as it may be stored locally in browser
		// Keep it sync with LMS.mediaplayer js
		enum class TranscodeMode
		{
			Never			= 0,
			Always			= 1,
			IfFormatNotSupported	= 2,
		};
		static inline constexpr TranscodeMode defaultTranscodeMode {TranscodeMode::IfFormatNotSupported};
		static inline constexpr Format defaultTranscodeFormat {Format::OGG_OPUS};
		static inline constexpr Bitrate defaultTranscodeBitrate {128000};

		struct Settings
		{
			TranscodeMode	mode {defaultTranscodeMode};
			Format		format {defaultTranscodeFormat};
			Bitrate		bitrate {defaultTranscodeBitrate};
		};

		MediaPlayer();

		MediaPlayer(const MediaPlayer&) = delete;
		MediaPlayer(MediaPlayer&&) = delete;
		MediaPlayer& operator=(const MediaPlayer&) = delete;
		MediaPlayer& operator=(MediaPlayer&&) = delete;

		std::optional<Settings>	getSettings() const { return _settings; }
		void			setSettings(const Settings& settings);

		// Signals
		Wt::JSignal<>	playbackEnded;
		Wt::JSignal<> 	playPrevious;
		Wt::JSignal<> 	playNext;

	private:
		void stop();
		void loadTrack(Database::IdType trackId, bool play);

		std::optional<Settings>		_settings;

		Wt::JSignal<int, int, int> 	_settingsLoaded;
		Wt::WText*	_title;
		Wt::WAnchor*	_release;
		Wt::WAnchor*	_artist;
};

} // namespace UserInterface

