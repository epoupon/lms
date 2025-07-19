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

#include <optional>

#include <Wt/WAnchor.h>
#include <Wt/WJavaScript.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include "database/Types.hpp"
#include "database/objects/TrackId.hpp"

namespace lms::ui
{
    class AudioFileResource;
    class AudioTranscodingResource;

    class MediaPlayer : public Wt::WTemplate
    {
    public:
        using Bitrate = db::Bitrate;
        using Format = db::TranscodingOutputFormat;
        using Gain = float;

        // Do not change enum values as they may be stored locally in browser
        // Keep it sync with LMS.mediaplayer js

        struct Settings
        {
            struct Transcoding
            {
                enum class Mode
                {
                    Never = 0,
                    Always = 1,
                    IfFormatNotSupported = 2,
                };
                static inline constexpr Mode defaultMode{ Mode::IfFormatNotSupported };
                static inline constexpr Format defaultFormat{ Format::OGG_OPUS };
                static inline constexpr Bitrate defaultBitrate{ 128000 };

                Mode mode{ defaultMode };
                Format format{ defaultFormat };
                Bitrate bitrate{ defaultBitrate };
            };

            struct ReplayGain
            {
                enum class Mode
                {
                    None = 0,
                    Auto = 1,
                    Track = 2,
                    Release = 3,
                };

                static inline constexpr Mode defaultMode{ Mode::None };
                static inline constexpr Gain defaultPreAmpGain{};
                static inline constexpr Gain minPreAmpGain{ -15 };
                static inline constexpr Gain maxPreAmpGain{ 15 };

                Mode mode{ defaultMode };
                Gain preAmpGain{ defaultPreAmpGain };
                Gain preAmpGainIfNoInfo{ defaultPreAmpGain };
            };

            Transcoding transcoding;
            ReplayGain replayGain;
        };

        MediaPlayer();
        ~MediaPlayer() override;
        MediaPlayer(const MediaPlayer&) = delete;
        MediaPlayer& operator=(const MediaPlayer&) = delete;

        std::optional<db::TrackId> getTrackLoaded() const { return _trackIdLoaded; }

        void loadTrack(db::TrackId trackId, bool play, float replayGain);
        void stop();

        std::optional<Settings> getSettings() const { return _settings; }
        void setSettings(const Settings& settings);

        void onPlayQueueUpdated(std::size_t trackCount);

        // Signals
        Wt::JSignal<> playPrevious;
        Wt::JSignal<> playNext;
        Wt::Signal<db::TrackId> trackLoaded;
        Wt::Signal<> settingsLoaded;

        Wt::JSignal<db::TrackId::ValueType> scrobbleListenNow;
        Wt::JSignal<db::TrackId::ValueType, unsigned /* ms */> scrobbleListenFinished;

        Wt::JSignal<> playbackEnded;

    private:
        std::unique_ptr<AudioFileResource> _audioFileResource;
        std::unique_ptr<AudioTranscodingResource> _audioTranscodingResource;

        std::optional<db::TrackId> _trackIdLoaded;
        std::optional<Settings> _settings;

        Wt::JSignal<std::string> _settingsLoaded;

        Wt::WText* _title{};
        Wt::WAnchor* _release{};
        Wt::WText* _separator{};
        Wt::WAnchor* _artist{};
        Wt::WPushButton* _playQueue{};
    };

} // namespace lms::ui
