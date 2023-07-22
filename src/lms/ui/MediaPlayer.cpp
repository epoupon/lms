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
#include <Wt/Json/Value.h>
#include <Wt/Json/Serializer.h>
#include <Wt/WPushButton.h>

#include "utils/Logger.hpp"

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/Types.hpp"
#include "services/database/User.hpp"

#include "resource/CoverResource.hpp"
#include "resource/AudioTranscodeResource.hpp"
#include "resource/AudioFileResource.hpp"

#include "utils/String.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"
#include "Utils.hpp"

namespace UserInterface {

static std::string settingsToJSString(const MediaPlayer::Settings& settings)
{
	namespace Json = Wt::Json;

	Json::Object res;

	{
		Json::Object transcode;
		transcode["mode"] = static_cast<int>(settings.transcode.mode);
		transcode["format"] = static_cast<int>(settings.transcode.format);
		transcode["bitrate"] = static_cast<int>(settings.transcode.bitrate);
		res["transcode"] = std::move(transcode);
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

static
std::optional<MediaPlayer::Settings::Transcode::Mode>
transcodeModeFromString(const std::string& str)
{
	const auto value {StringUtils::readAs<int>(str)};
	if (!value)
		return std::nullopt;

	MediaPlayer::Settings::Transcode::Mode mode {static_cast<MediaPlayer::Settings::Transcode::Mode>(*value)};
	switch (mode)
	{
		case MediaPlayer::Settings::Transcode::Mode::Never:
		case MediaPlayer::Settings::Transcode::Mode::Always:
		case MediaPlayer::Settings::Transcode::Mode::IfFormatNotSupported:
			return mode;
	}

	return std::nullopt;
}

static
std::optional<MediaPlayer::Format>
formatFromString(const std::string& str)
{
	const auto value {StringUtils::readAs<int>(str)};
	if (!value)
		return std::nullopt;

	MediaPlayer::Format format {static_cast<MediaPlayer::Format>(*value)};
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

static
std::optional<MediaPlayer::Bitrate>
bitrateFromString(const std::string& str)
{
	const auto value {StringUtils::readAs<int>(str)};
	if (!value)
		return std::nullopt;

	if (!Database::isAudioBitrateAllowed(*value))
		return std::nullopt;

	return *value;
}

static
std::optional<MediaPlayer::Settings::ReplayGain::Mode>
replayGainModeFromString(const std::string& str)
{
	const auto value {StringUtils::readAs<int>(str)};
	if (!value)
		return std::nullopt;

	MediaPlayer::Settings::ReplayGain::Mode mode {static_cast<MediaPlayer::Settings::ReplayGain::Mode>(*value)};
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

static
std::optional<float>
replayGainPreAmpGainFromString(const std::string& str)
{
	const auto value {StringUtils::readAs<double>(str)};
	if (!value)
		return std::nullopt;

	return ::Utils::clamp(*value, (double)MediaPlayer::Settings::ReplayGain::minPreAmpGain, (double)MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
}

static MediaPlayer::Settings settingsfromJSString(const std::string& strSettings)
{
	using Settings = MediaPlayer::Settings;
	namespace Json = Wt::Json;
	Json::Object parsedSettings;

	Json::parse(strSettings, parsedSettings);

	MediaPlayer::Settings settings;

	{
		const Json::Value transcodeValue {parsedSettings.get("transcode")};
		if (transcodeValue.type() == Json::Type::Object)
		{
			const Json::Object transcode {transcodeValue};
			settings.transcode.mode = transcodeModeFromString(transcode.get("mode").toString().orIfNull("")).value_or(Settings::Transcode::defaultMode);
			settings.transcode.format = formatFromString(transcode.get("format").toString().orIfNull("")).value_or(Settings::Transcode::defaultFormat);
			settings.transcode.bitrate = bitrateFromString(transcode.get("bitrate").toString().orIfNull("")).value_or(Settings::Transcode::defaultBitrate);
		}
	}
	{
		const Json::Value replayGainValue {parsedSettings.get("replayGain")};
		if (replayGainValue.type() == Json::Type::Object)
		{
			const Json::Object replayGain {replayGainValue};
			settings.replayGain.mode = replayGainModeFromString(replayGain.get("mode").toString().orIfNull("")).value_or(Settings::ReplayGain::defaultMode);
			settings.replayGain.preAmpGain = replayGainPreAmpGainFromString(replayGain.get("preAmpGain").toString().orIfNull("")).value_or(Settings::ReplayGain::defaultPreAmpGain);
			settings.replayGain.preAmpGainIfNoInfo = replayGainPreAmpGainFromString(replayGain.get("preAmpGainIfNoInfo").toString().orIfNull("")).value_or(Settings::ReplayGain::defaultPreAmpGain);
		}
	}

	return settings;
}

MediaPlayer::MediaPlayer()
: Wt::WTemplate {Wt::WString::tr("Lms.MediaPlayer.template")}
, playPrevious {this, "playPrevious"}
, playNext {this, "playNext"}
, scrobbleListenNow {this, "scrobbleListenNow"}
, scrobbleListenFinished {this, "scrobbleListenFinished"}
, playbackEnded {this, "playbackEnded"}
, _settingsLoaded {this, "settingsLoaded"}
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_audioTranscodeResource = std::make_unique<AudioTranscodeResource>();
	_audioFileResource = std::make_unique<AudioFileResource>();

	_title = bindNew<Wt::WText>("title");
	_artist = bindNew<Wt::WAnchor>("artist");
	_release = bindNew<Wt::WAnchor>("release");
	_separator = bindNew<Wt::WText>("separator");
	_playQueue = bindNew<Wt::WPushButton>("playqueue-btn", Wt::WString::tr("Lms.MediaPlayer.template.playqueue-btn").arg(0), Wt::TextFormat::XHTML);
	_playQueue->setLink(Wt::WLink {Wt::LinkType::InternalPath, "/playqueue"});
	_playQueue->setToolTip(tr("Lms.PlayQueue.playqueue"));

	_settingsLoaded.connect([this](const std::string& settings)
	{
		LMS_LOG(UI, DEBUG) << "Settings loaded! '" << settings << "'";

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

		LMS_LOG(UI, DEBUG) << "Running js = '" << oss.str() << "'";
		doJavaScript(oss.str());
	}
}

void
MediaPlayer::loadTrack(Database::TrackId trackId, bool play, float replayGain)
{
	LMS_LOG(UI, DEBUG) << "Playing track ID = " << trackId.toString();

	std::ostringstream oss;
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const auto track {Database::Track::find(LmsApp->getDbSession(), trackId)};
		if (!track)
			return;

		const std::string transcodeResource {_audioTranscodeResource->getUrl(trackId)};
		const std::string nativeResource {_audioFileResource->getUrl(trackId)};

		const auto artists {track->getArtists({Database::TrackArtistLinkType::Artist})};

		oss
			<< "var params = {"
			<< " trackId :\"" << trackId.toString() << "\","
			<< " nativeResource: \"" << nativeResource << "\","
			<< " transcodeResource: \"" << transcodeResource << "\","
			<< " duration: " << std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count() << ","
			<< " replayGain: " << replayGain << ","
			<< " title: \"" << StringUtils::jsEscape(track->getName()) << "\","
			<< " artist: \"" << (!artists.empty() ? StringUtils::jsEscape(artists.front()->getName()) : "") << "\","
			<< " release: \"" << (track->getRelease() ? StringUtils::jsEscape(track->getRelease()->getName()) : "") << "\","
			<< " artwork: ["
			<< "   { src: \"" << LmsApp->getCoverResource()->getTrackUrl(trackId, CoverResource::Size::Small) << "\", sizes: \"128x128\",	type: \"image/jpeg\" },"
			<< "   { src: \"" << LmsApp->getCoverResource()->getTrackUrl(trackId, CoverResource::Size::Large) << "\", sizes: \"512x512\",	type: \"image/jpeg\" },"
			<< " ]"
			<< "};";
		oss << "LMS.mediaplayer.loadTrack(params, " << (play ? "true" : "false") << ")"; // true to autoplay

		_title->setTextFormat(Wt::TextFormat::Plain);
		_title->setText(Wt::WString::fromUTF8(track->getName()));

		bool needSeparator {true};

		if (!artists.empty())
		{
			_artist->setTextFormat(Wt::TextFormat::Plain);
			_artist->setText(Wt::WString::fromUTF8(artists.front()->getName()));
			_artist->setLink(Utils::createArtistLink(artists.front()));
		}
		else
		{
			_artist->setText("");
			_artist->setLink({});
			needSeparator = false;
		}

		if (track->getRelease())
		{
			_release->setTextFormat(Wt::TextFormat::Plain);
			_release->setText(Wt::WString::fromUTF8(track->getRelease()->getName()));
			_release->setLink(Utils::createReleaseLink(track->getRelease()));
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

	LMS_LOG(UI, DEBUG) << "Running js = '" << oss.str() << "'";
	doJavaScript(oss.str());

	_trackIdLoaded = trackId;
	trackLoaded.emit(*_trackIdLoaded);
}

void
MediaPlayer::stop()
{
	doJavaScript("LMS.mediaplayer.stop()");
}

void
MediaPlayer::setSettings(const Settings& settings)
{
	_settings = settings;

	{
		std::ostringstream oss;
		oss << "LMS.mediaplayer.setSettings(settings = " << settingsToJSString(settings) << ")";

		LMS_LOG(UI, DEBUG) << "Running js = '" << oss.str() << "'";
		doJavaScript(oss.str());
	}
}

void
MediaPlayer::onPlayQueueUpdated(std::size_t trackCount)
{
	_playQueue->setText(Wt::WString::tr("Lms.MediaPlayer.template.playqueue-btn").arg(trackCount));
}

} // namespace UserInterface

