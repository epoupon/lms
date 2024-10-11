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

#include "MediaPlayer.hpp"

#include <Wt/Json/Object.h>
#include <Wt/Json/Serializer.h>
#include <Wt/Json/Value.h>
#include <Wt/WPushButton.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"
#include "core/Utils.hpp"
#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "database/Types.hpp"
#include "database/User.hpp"

#include "LmsApplication.hpp"
#include "Utils.hpp"
#include "resource/ArtworkResource.hpp"
#include "resource/AudioFileResource.hpp"
#include "resource/AudioTranscodingResource.hpp"

namespace lms::ui
{
    namespace
    {
        std::string settingsToJSString(const MediaPlayer::Settings& settings)
        {
            namespace Json = Wt::Json;

            Json::Object res;

            {
                Json::Object transcoding;
                transcoding["mode"] = static_cast<int>(settings.transcoding.mode);
                transcoding["format"] = static_cast<int>(settings.transcoding.format);
                transcoding["bitrate"] = static_cast<int>(settings.transcoding.bitrate);
                res["transcoding"] = std::move(transcoding);
            }

            {
                Json::Object replayGain;
                replayGain["mode"] = static_cast<int>(settings.replayGain.mode);
                replayGain["preAmpGain"] = settings.replayGain.preAmpGain;
                replayGain["preAmpGainIfNoInfo"] = settings.replayGain.preAmpGainIfNoInfo;
                res["replayGain"] = std::move(replayGain);
            }

            return Json::serialize(res);
        }

        std::optional<MediaPlayer::Settings::Transcoding::Mode> transcodingModeFromString(const std::string& str)
        {
            const auto value{ core::stringUtils::readAs<int>(str) };
            if (!value)
                return std::nullopt;

            MediaPlayer::Settings::Transcoding::Mode mode{ static_cast<MediaPlayer::Settings::Transcoding::Mode>(*value) };
            switch (mode)
            {
            case MediaPlayer::Settings::Transcoding::Mode::Never:
            case MediaPlayer::Settings::Transcoding::Mode::Always:
            case MediaPlayer::Settings::Transcoding::Mode::IfFormatNotSupported:
                return mode;
            }

            return std::nullopt;
        }

        std::optional<MediaPlayer::Format> formatFromString(const std::string& str)
        {
            const auto value{ core::stringUtils::readAs<int>(str) };
            if (!value)
                return std::nullopt;

            MediaPlayer::Format format{ static_cast<MediaPlayer::Format>(*value) };
            switch (format)
            {
            case MediaPlayer::Format::MP3:
            case MediaPlayer::Format::OGG_OPUS:
            case MediaPlayer::Format::MATROSKA_OPUS:
            case MediaPlayer::Format::OGG_VORBIS:
            case MediaPlayer::Format::WEBM_VORBIS:
                return format;
            }

            return std::nullopt;
        }

        std::optional<MediaPlayer::Bitrate> bitrateFromString(const std::string& str)
        {
            const auto value{ core::stringUtils::readAs<int>(str) };
            if (!value)
                return std::nullopt;

            if (!db::isAudioBitrateAllowed(*value))
                return std::nullopt;

            return *value;
        }

        std::optional<MediaPlayer::Settings::ReplayGain::Mode> replayGainModeFromString(const std::string& str)
        {
            const auto value{ core::stringUtils::readAs<int>(str) };
            if (!value)
                return std::nullopt;

            MediaPlayer::Settings::ReplayGain::Mode mode{ static_cast<MediaPlayer::Settings::ReplayGain::Mode>(*value) };
            switch (mode)
            {
            case MediaPlayer::Settings::ReplayGain::Mode::None:
            case MediaPlayer::Settings::ReplayGain::Mode::Auto:
            case MediaPlayer::Settings::ReplayGain::Mode::Track:
            case MediaPlayer::Settings::ReplayGain::Mode::Release:
                return mode;
            }

            return std::nullopt;
        }

        std::optional<float> replayGainPreAmpGainFromString(const std::string& str)
        {
            const auto value{ core::stringUtils::readAs<double>(str) };
            if (!value)
                return std::nullopt;

            return core::utils::clamp(*value, (double)MediaPlayer::Settings::ReplayGain::minPreAmpGain, (double)MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
        }

        MediaPlayer::Settings settingsfromJSString(const std::string& strSettings)
        {
            using Settings = MediaPlayer::Settings;
            namespace Json = Wt::Json;
            Json::Object parsedSettings;

            Json::parse(strSettings, parsedSettings);

            MediaPlayer::Settings settings;

            {
                const Json::Value transcodingValue{ parsedSettings.get("transcoding") };
                if (transcodingValue.type() == Json::Type::Object)
                {
                    const Json::Object transcoding{ transcodingValue };
                    settings.transcoding.mode = transcodingModeFromString(transcoding.get("mode").toString().orIfNull("")).value_or(Settings::Transcoding::defaultMode);
                    settings.transcoding.format = formatFromString(transcoding.get("format").toString().orIfNull("")).value_or(Settings::Transcoding::defaultFormat);
                    settings.transcoding.bitrate = bitrateFromString(transcoding.get("bitrate").toString().orIfNull("")).value_or(Settings::Transcoding::defaultBitrate);
                }
            }
            {
                const Json::Value replayGainValue{ parsedSettings.get("replayGain") };
                if (replayGainValue.type() == Json::Type::Object)
                {
                    const Json::Object replayGain{ replayGainValue };
                    settings.replayGain.mode = replayGainModeFromString(replayGain.get("mode").toString().orIfNull("")).value_or(Settings::ReplayGain::defaultMode);
                    settings.replayGain.preAmpGain = replayGainPreAmpGainFromString(replayGain.get("preAmpGain").toString().orIfNull("")).value_or(Settings::ReplayGain::defaultPreAmpGain);
                    settings.replayGain.preAmpGainIfNoInfo = replayGainPreAmpGainFromString(replayGain.get("preAmpGainIfNoInfo").toString().orIfNull("")).value_or(Settings::ReplayGain::defaultPreAmpGain);
                }
            }

            return settings;
        }
    } // namespace

    MediaPlayer::MediaPlayer()
        : Wt::WTemplate{ Wt::WString::tr("Lms.MediaPlayer.template") }
        , playPrevious{ this, "playPrevious" }
        , playNext{ this, "playNext" }
        , scrobbleListenNow{ this, "scrobbleListenNow" }
        , scrobbleListenFinished{ this, "scrobbleListenFinished" }
        , playbackEnded{ this, "playbackEnded" }
        , _settingsLoaded{ this, "settingsLoaded" }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);

        _audioTranscodingResource = std::make_unique<AudioTranscodingResource>();
        _audioFileResource = std::make_unique<AudioFileResource>();

        _title = bindNew<Wt::WText>("title");
        _artist = bindNew<Wt::WAnchor>("artist");
        _release = bindNew<Wt::WAnchor>("release");
        _separator = bindNew<Wt::WText>("separator");
        _playQueue = bindNew<Wt::WPushButton>("playqueue-btn", Wt::WString::tr("Lms.MediaPlayer.template.playqueue-btn").arg(0), Wt::TextFormat::XHTML);
        _playQueue->setLink(Wt::WLink{ Wt::LinkType::InternalPath, "/playqueue" });
        _playQueue->setToolTip(tr("Lms.PlayQueue.playqueue"));

        _settingsLoaded.connect([this](const std::string& settings) {
            LMS_LOG(UI, DEBUG, "Settings loaded! '" << settings << "'");

            _settings = settingsfromJSString(settings);

            settingsLoaded.emit();
        });

        {
            Settings defaultSettings;

            std::ostringstream oss;
            oss << "LMS.mediaplayer.init("
                << jsRef()
                << ", defaultSettings = " << settingsToJSString(defaultSettings)
                << ")";

            LMS_LOG(UI, DEBUG, "Running js = '" << oss.str() << "'");
            doJavaScript(oss.str());
        }
    }

    void MediaPlayer::loadTrack(db::TrackId trackId, bool play, float replayGain)
    {
        LMS_LOG(UI, DEBUG, "Playing track ID = " << trackId.toString());

        std::ostringstream oss;
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const auto track{ db::Track::find(LmsApp->getDbSession(), trackId) };
            if (!track)
                return;

            const std::string transcodingResource{ _audioTranscodingResource->getUrl(trackId) };
            const std::string nativeResource{ _audioFileResource->getUrl(trackId) };

            const auto artists{ track->getArtists({ db::TrackArtistLinkType::Artist }) };

            oss
                << "var params = {"
                << " trackId :\"" << trackId.toString() << "\","
                << " nativeResource: \"" << nativeResource << "\","
                << " transcodingResource: \"" << transcodingResource << "\","
                << " duration: " << std::chrono::duration_cast<std::chrono::duration<float>>(track->getDuration()).count() << ","
                << " replayGain: " << replayGain << ","
                << " title: \"" << core::stringUtils::jsEscape(track->getName()) << "\","
                << " artist: \"" << (!artists.empty() ? core::stringUtils::jsEscape(track->getArtistDisplayName()) : "") << "\","
                << " release: \"" << (track->getRelease() ? core::stringUtils::jsEscape(track->getRelease()->getName()) : "") << "\","
                << " artwork: ["
                << "   { src: \"" << LmsApp->getArtworkResource()->getTrackImageUrl(trackId, ArtworkResource::Size::Small) << "\", sizes: \"128x128\",	type: \"image/jpeg\" },"
                << "   { src: \"" << LmsApp->getArtworkResource()->getTrackImageUrl(trackId, ArtworkResource::Size::Large) << "\", sizes: \"512x512\",	type: \"image/jpeg\" },"
                << " ]"
                << "};";
            // Update 'sizes' above to match this:
            static_assert(static_cast<std::underlying_type_t<ArtworkResource::Size>>(ArtworkResource::Size::Small) == 128);
            static_assert(static_cast<std::underlying_type_t<ArtworkResource::Size>>(ArtworkResource::Size::Large) == 512);
            oss << "LMS.mediaplayer.loadTrack(params, " << (play ? "true" : "false") << ")"; // true to autoplay

            _title->setTextFormat(Wt::TextFormat::Plain);
            _title->setText(Wt::WString::fromUTF8(track->getName()));

            bool needSeparator{ true };

            if (!artists.empty())
            {
                _artist->setTextFormat(Wt::TextFormat::Plain);
                _artist->setText(Wt::WString::fromUTF8(artists.front()->getName()));
                _artist->setLink(utils::createArtistLink(artists.front()));
            }
            else
            {
                _artist->setText("");
                _artist->setLink({});
                needSeparator = false;
            }

            if (const db::Release::pointer release{ track->getRelease() })
            {
                _release->setTextFormat(Wt::TextFormat::Plain);
                _release->setText(Wt::WString::fromUTF8(std::string{ release->getName() }));
                _release->setLink(utils::createReleaseLink(release));
            }
            else
            {
                _release->setText("");
                _release->setLink({});
                needSeparator = false;
            }

            if (needSeparator)
                _separator->setText(" — ");
            else
                _separator->setText("");
        }

        LMS_LOG(UI, DEBUG, "Running js = '" << oss.str() << "'");
        doJavaScript(oss.str());

        _trackIdLoaded = trackId;
        trackLoaded.emit(*_trackIdLoaded);
    }

    void MediaPlayer::stop()
    {
        doJavaScript("LMS.mediaplayer.stop()");
    }

    void MediaPlayer::setSettings(const Settings& settings)
    {
        _settings = settings;

        {
            std::ostringstream oss;
            oss << "LMS.mediaplayer.setSettings(settings = " << settingsToJSString(settings) << ")";

            LMS_LOG(UI, DEBUG, "Running js = '" << oss.str() << "'");
            doJavaScript(oss.str());
        }
    }

    void MediaPlayer::onPlayQueueUpdated(std::size_t trackCount)
    {
        _playQueue->setText(Wt::WString::tr("Lms.MediaPlayer.template.playqueue-btn").arg(trackCount));
    }

} // namespace lms::ui
