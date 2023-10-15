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

#include "SettingsView.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WDoubleSpinBox.h>
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WTemplateFormView.h>

#include "common/DoubleValidator.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/PasswordValidator.hpp"
#include "common/UUIDValidator.hpp"
#include "common/ValueStringModel.hpp"

#include "services/auth/IPasswordService.hpp"
#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "utils/IConfig.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"

namespace UserInterface {

using namespace Database;

class SettingsModel : public Wt::WFormModel
{
	public:
		// Associate each field with a unique string literal.
		static inline const Field TranscodeModeField {"transcode-mode"};
		static inline const Field TranscodeFormatField {"transcode-format"};
		static inline const Field TranscodeBitrateField {"transcode-bitrate"};
		static inline const Field ReplayGainModeField {"replaygain-mode"};
		static inline const Field ReplayGainPreAmpGainField {"replaygain-preamp"};
		static inline const Field ReplayGainPreAmpGainIfNoInfoField {"replaygain-preamp-no-rg-info"};
		static inline const Field SubsonicArtistListModeField {"subsonic-artist-list-mode"};
		static inline const Field SubsonicTranscodeEnableField {"subsonic-transcode-enable"};
		static inline const Field SubsonicTranscodeFormatField {"subsonic-transcode-format"};
		static inline const Field SubsonicTranscodeBitrateField {"subsonic-transcode-bitrate"};
		static inline const Field ScrobblerField {"scrobbler"};
		static inline const Field ListenBrainzTokenField {"listenbrainz-token"};
		static inline const Field PasswordOldField {"password-old"};
		static inline const Field PasswordField {"password"};
		static inline const Field PasswordConfirmField {"password-confirm"};

		using TranscodeModeModel = ValueStringModel<MediaPlayer::Settings::Transcode::Mode>;
		using ReplayGainModeModel = ValueStringModel<MediaPlayer::Settings::ReplayGain::Mode>;
		using ScrobblerModel = ValueStringModel<Scrobbler>;

		SettingsModel(::Auth::IPasswordService* authPasswordService, bool withOldPassword)
			: _authPasswordService {authPasswordService}
			, _withOldPassword {withOldPassword}
		{
			initializeModels();

			addField(TranscodeModeField);
			addField(TranscodeBitrateField);
			addField(TranscodeFormatField);
			addField(ReplayGainModeField);
			addField(ReplayGainPreAmpGainField);
			addField(ReplayGainPreAmpGainIfNoInfoField);
			addField(SubsonicTranscodeEnableField);
			addField(SubsonicTranscodeBitrateField);
			addField(SubsonicTranscodeFormatField);
			addField(ScrobblerField);
			addField(ListenBrainzTokenField);
			setValidator(ListenBrainzTokenField, createUUIDValidator());

			if (_authPasswordService)
			{
				if (_withOldPassword)
				{
					addField(PasswordOldField);
					setValidator(PasswordOldField, createPasswordCheckValidator());
				}

				addField(PasswordField);
				setValidator(PasswordField, createPasswordStrengthValidator([] { return ::Auth::PasswordValidationContext {LmsApp->getUserLoginName(), LmsApp->getUserType()}; }));
				addField(PasswordConfirmField);
			}

			setValidator(TranscodeModeField, createMandatoryValidator());
			setValidator(TranscodeBitrateField, createMandatoryValidator());
			setValidator(TranscodeFormatField, createMandatoryValidator());
			setValidator(ReplayGainModeField, createMandatoryValidator());
			auto createPreAmpValidator {[]
			{
				return createDoubleValidator(MediaPlayer::Settings::ReplayGain::minPreAmpGain, MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
			}};

			setValidator(ReplayGainPreAmpGainField, createPreAmpValidator());
			setValidator(ReplayGainPreAmpGainIfNoInfoField, createPreAmpValidator());
			setValidator(SubsonicTranscodeBitrateField, createMandatoryValidator());
			setValidator(SubsonicTranscodeFormatField, createMandatoryValidator());

			loadData();
		}

		std::shared_ptr<TranscodeModeModel>		getTranscodeModeModel() { return _transcodeModeModel; }
		std::shared_ptr<Wt::WAbstractItemModel>	getTranscodeBitrateModel() { return _transcodeBitrateModel; }
		std::shared_ptr<Wt::WAbstractItemModel>	getTranscodeFormatModel() { return _transcodeFormatModel; }
		std::shared_ptr<ReplayGainModeModel>	getReplayGainModeModel() { return _replayGainModeModel; }
		std::shared_ptr<Wt::WAbstractItemModel>	getSubsonicArtistListModeModel() { return _subsonicArtistListModeModel; }
		std::shared_ptr<ScrobblerModel>			getScrobblerModel() { return _scrobblerModel; }

		void saveData()
		{
			auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

			User::pointer user {LmsApp->getUser()};

			{
				MediaPlayer::Settings settings;

				auto transcodeModeRow {_transcodeModeModel->getRowFromString(valueText(TranscodeModeField))};
				if (transcodeModeRow)
					settings.transcode.mode = _transcodeModeModel->getValue(*transcodeModeRow);

				auto transcodeFormatRow {_transcodeFormatModel->getRowFromString(valueText(TranscodeFormatField))};
				if (transcodeFormatRow)
					settings.transcode.format = _transcodeFormatModel->getValue(*transcodeFormatRow);

				auto transcodeBitrateRow {_transcodeBitrateModel->getRowFromString(valueText(TranscodeBitrateField))};
				if (transcodeBitrateRow)
					settings.transcode.bitrate = _transcodeBitrateModel->getValue(*transcodeBitrateRow);

				auto replayGainModeRow {_replayGainModeModel->getRowFromString(valueText(ReplayGainModeField))};
				if (replayGainModeRow)
					settings.replayGain.mode = _replayGainModeModel->getValue(*replayGainModeRow);

				settings.replayGain.preAmpGain = Wt::asNumber(value(ReplayGainPreAmpGainField));
				settings.replayGain.preAmpGainIfNoInfo = Wt::asNumber(value(ReplayGainPreAmpGainIfNoInfoField));

				LmsApp->getMediaPlayer().setSettings(settings);
			}

			{
				auto subsonicTranscodeBitrateRow {_transcodeBitrateModel->getRowFromString(valueText(SubsonicTranscodeBitrateField))};
				if (subsonicTranscodeBitrateRow)
					user.modify()->setSubsonicDefaultTranscodeBitrate(_transcodeBitrateModel->getValue(*subsonicTranscodeBitrateRow));

				auto subsonicTranscodeFormatRow {_transcodeFormatModel->getRowFromString(valueText(SubsonicTranscodeFormatField))};
				if (subsonicTranscodeFormatRow)
					user.modify()->setSubsonicDefaultTranscodeFormat(_transcodeFormatModel->getValue(*subsonicTranscodeFormatRow));

				auto subsonicArtistListModeRow {_subsonicArtistListModeModel->getRowFromString(valueText(SubsonicArtistListModeField))};
				if (subsonicArtistListModeRow)
					user.modify()->setSubsonicArtistListMode(_subsonicArtistListModeModel->getValue(*subsonicArtistListModeRow));

			}

			{
				if (auto scrobblerRow {_scrobblerModel->getRowFromString(valueText(ScrobblerField))})
					user.modify()->setScrobbler(_scrobblerModel->getValue(*scrobblerRow));

				user.modify()->setListenBrainzToken(UUID::fromString(Wt::asString(value(ListenBrainzTokenField)).toUTF8()));
			}

			if (_authPasswordService && !valueText(PasswordField).empty())
			{
				_authPasswordService->setPassword(user->getId(), valueText(PasswordField).toUTF8());
			}
		}

		void loadData()
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			User::pointer user {LmsApp->getUser()};

			{
				const auto settings {*LmsApp->getMediaPlayer().getSettings()};

				auto transcodeModeRow {_transcodeModeModel->getRowFromValue(settings.transcode.mode)};
				if (transcodeModeRow)
					setValue(TranscodeModeField, _transcodeModeModel->getString(*transcodeModeRow));

				auto transcodeFormatRow {_transcodeFormatModel->getRowFromValue(settings.transcode.format)};
				if (transcodeFormatRow)
					setValue(TranscodeFormatField, _transcodeFormatModel->getString(*transcodeFormatRow));

				auto transcodeBitrateRow {_transcodeBitrateModel->getRowFromValue(settings.transcode.bitrate)};
				if (transcodeBitrateRow)
					setValue(TranscodeBitrateField, _transcodeBitrateModel->getString(*transcodeBitrateRow));

				{
					const bool usesTranscode {settings.transcode.mode != MediaPlayer::Settings::Transcode::Mode::Never};
					setReadOnly(SettingsModel::TranscodeFormatField, !usesTranscode);
					setReadOnly(SettingsModel::TranscodeBitrateField, !usesTranscode);
				}

				auto replayGainModeRow {_replayGainModeModel->getRowFromValue(settings.replayGain.mode)};
				if (replayGainModeRow)
					setValue(ReplayGainModeField, _replayGainModeModel->getString(*replayGainModeRow));

				setValue(ReplayGainPreAmpGainField, settings.replayGain.preAmpGain);
				setValue(ReplayGainPreAmpGainIfNoInfoField, settings.replayGain.preAmpGainIfNoInfo);
			}

			{
				auto subsonicTranscodeBitrateRow {_transcodeBitrateModel->getRowFromValue(user->getSubsonicDefaultTranscodeBitrate())};
				if (subsonicTranscodeBitrateRow)
					setValue(SubsonicTranscodeBitrateField, _transcodeBitrateModel->getString(*subsonicTranscodeBitrateRow));

				auto subsonicTranscodeFormatRow {_transcodeFormatModel->getRowFromValue(user->getSubsonicDefaultTranscodeFormat())};
				if (subsonicTranscodeFormatRow)
					setValue(SubsonicTranscodeFormatField, _transcodeFormatModel->getString(*subsonicTranscodeFormatRow));

				auto subsonicArtistListModeRow {_subsonicArtistListModeModel->getRowFromValue(user->getSubsonicArtistListMode())};
				if (subsonicArtistListModeRow)
					setValue(SubsonicArtistListModeField, _subsonicArtistListModeModel->getString(*subsonicArtistListModeRow));
			}

			{
				if (auto scrobblerRow {_scrobblerModel->getRowFromValue(user->getScrobbler())})
					setValue(ScrobblerField, _scrobblerModel->getString(*scrobblerRow));

				if (auto listenBrainzToken {user->getListenBrainzToken()})
				{
					LMS_LOG(UI, DEBUG) << "Read listenBrainzToken! value = " << listenBrainzToken->getAsString();
					setValue(ListenBrainzTokenField, Wt::WString::fromUTF8( std::string {listenBrainzToken->getAsString()}));
				}

				{
					const bool usesListenBrainz {user->getScrobbler() == Scrobbler::ListenBrainz};
					setReadOnly(SettingsModel::ListenBrainzTokenField, !usesListenBrainz);
					validator(SettingsModel::ListenBrainzTokenField)->setMandatory(usesListenBrainz);
				}
			}
			if (_authPasswordService)
			{
				if (_withOldPassword)
					setValue(PasswordOldField, "");

				setValue(PasswordField, "");
				setValue(PasswordConfirmField, "");
			}
		}

	private:
		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == PasswordOldField)
			{
				if (valueText(PasswordOldField).empty() && !valueText(PasswordField).empty())
					error = Wt::WString::tr("Lms.Settings.password-must-fill-old-password");
				else
					return Wt::WFormModel::validateField(field);
			}
			else if (field == PasswordField)
			{
				if (!valueText(PasswordOldField).empty() && valueText(PasswordField).empty())
					error = Wt::WString::tr("Wt.WValidator.Invalid");
				else
					return Wt::WFormModel::validateField(field);
			}
			else if (field == PasswordConfirmField)
			{
				if (validation(PasswordField).state() == Wt::ValidationState::Valid)
				{
					if (valueText(PasswordField) != valueText(PasswordConfirmField))
						error = Wt::WString::tr("Lms.passwords-dont-match");
				}
			}
			else
			{
				return Wt::WFormModel::validateField(field);
			}

			setValidation(field, Wt::WValidator::Result( error.empty() ? Wt::ValidationState::Valid : Wt::ValidationState::Invalid, error));

			return (validation(field).state() == Wt::ValidationState::Valid);
		}

	private:

		void initializeModels()
		{
			_transcodeModeModel = std::make_shared<TranscodeModeModel>();
			_transcodeModeModel->add(Wt::WString::tr("Lms.Settings.transcode-mode.always"), MediaPlayer::Settings::Transcode::Mode::Always);
			_transcodeModeModel->add(Wt::WString::tr("Lms.Settings.transcode-mode.never"), MediaPlayer::Settings::Transcode::Mode::Never);
			_transcodeModeModel->add(Wt::WString::tr("Lms.Settings.transcode-mode.if-format-not-supported"), MediaPlayer::Settings::Transcode::Mode::IfFormatNotSupported);

			_transcodeBitrateModel = std::make_shared<ValueStringModel<Bitrate>>();
			visitAllowedAudioBitrates([&](const Bitrate bitrate)
			{
				_transcodeBitrateModel->add(Wt::WString::fromUTF8(std::to_string(bitrate / 1000)), bitrate);
			});

			_transcodeFormatModel = std::make_shared<ValueStringModel<AudioFormat>>();
			_transcodeFormatModel->add(Wt::WString::tr("Lms.Settings.transcode-format.mp3"), AudioFormat::MP3);
			_transcodeFormatModel->add(Wt::WString::tr("Lms.Settings.transcode-format.ogg_opus"), AudioFormat::OGG_OPUS);
			_transcodeFormatModel->add(Wt::WString::tr("Lms.Settings.transcode-format.matroska_opus"), AudioFormat::MATROSKA_OPUS);
			_transcodeFormatModel->add(Wt::WString::tr("Lms.Settings.transcode-format.ogg_vorbis"), AudioFormat::OGG_VORBIS);
			_transcodeFormatModel->add(Wt::WString::tr("Lms.Settings.transcode-format.webm_vorbis"), AudioFormat::WEBM_VORBIS);

			_replayGainModeModel = std::make_shared<ReplayGainModeModel>();
			_replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.none"), MediaPlayer::Settings::ReplayGain::Mode::None);
			_replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.auto"), MediaPlayer::Settings::ReplayGain::Mode::Auto);
			_replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.track"), MediaPlayer::Settings::ReplayGain::Mode::Track);
			_replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.release"), MediaPlayer::Settings::ReplayGain::Mode::Release);

			_subsonicArtistListModeModel = std::make_shared<ValueStringModel<SubsonicArtistListMode>>();
			_subsonicArtistListModeModel->add(Wt::WString::tr("Lms.Settings.subsonic-artist-list-mode.all-artists"), SubsonicArtistListMode::AllArtists);
			_subsonicArtistListModeModel->add(Wt::WString::tr("Lms.Settings.subsonic-artist-list-mode.release-artists"), SubsonicArtistListMode::ReleaseArtists);
			_subsonicArtistListModeModel->add(Wt::WString::tr("Lms.Settings.subsonic-artist-list-mode.track-artists"), SubsonicArtistListMode::TrackArtists);

			_scrobblerModel = std::make_shared<ValueStringModel<Scrobbler>>();
			_scrobblerModel->add(Wt::WString::tr("Lms.Settings.scrobbling.scrobbler.internal"), Scrobbler::Internal);
			_scrobblerModel->add(Wt::WString::tr("Lms.Settings.scrobbling.scrobbler.listenbrainz"), Scrobbler::ListenBrainz);
		}

		::Auth::IPasswordService* _authPasswordService {};
		bool _withOldPassword {};

		std::shared_ptr<TranscodeModeModel>					_transcodeModeModel;
		std::shared_ptr<ValueStringModel<Bitrate>>			_transcodeBitrateModel;
		std::shared_ptr<ValueStringModel<AudioFormat>>		_transcodeFormatModel;
		std::shared_ptr<ReplayGainModeModel>				_replayGainModeModel;
		std::shared_ptr<ValueStringModel<SubsonicArtistListMode>> _subsonicArtistListModeModel;
		std::shared_ptr<ScrobblerModel> 					_scrobblerModel;
};

SettingsView::SettingsView()
{
	wApp->internalPathChanged().connect(this, [this]
	{
		refreshView();
	});

	LmsApp->getMediaPlayer().settingsLoaded.connect([this]
	{
		refreshView();
	});

	refreshView();
}

void
SettingsView::refreshView()
{
	if (!wApp->internalPathMatches("/settings"))
		return;

	clear();

	// Hack to wait for the audio player know the settings applied
	if (!LmsApp->getMediaPlayer().getSettings())
		return;

	auto t {addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Settings.template"))};

	auto* authPasswordService {Service<::Auth::IPasswordService>::get()};
	if (authPasswordService && !authPasswordService->canSetPasswords())
		authPasswordService = nullptr;

	auto model {std::make_shared<SettingsModel>(authPasswordService, !LmsApp->isUserAuthStrong())};

	if (authPasswordService)
	{
		t->setCondition("if-has-change-password", true);

		// Old password
		if (!LmsApp->isUserAuthStrong())
		{
			t->setCondition("if-has-old-password", true);

			auto oldPassword {std::make_unique<Wt::WLineEdit>()};
			oldPassword->setEchoMode(Wt::EchoMode::Password);
			oldPassword->setAttributeValue("autocomplete", "current-password");
			t->setFormWidget(SettingsModel::PasswordOldField, std::move(oldPassword));
		}

		// Password
		auto password {std::make_unique<Wt::WLineEdit>()};
		password->setEchoMode(Wt::EchoMode::Password);
		password->setAttributeValue("autocomplete", "new-password");
		t->setFormWidget(SettingsModel::PasswordField, std::move(password));

		// Password confirm
		auto passwordConfirm {std::make_unique<Wt::WLineEdit>()};
		passwordConfirm->setEchoMode(Wt::EchoMode::Password);
		passwordConfirm->setAttributeValue("autocomplete", "new-password");
		t->setFormWidget(SettingsModel::PasswordConfirmField, std::move(passwordConfirm));
	}

	// Audio
	{
		// Transcode
		auto transcodeMode {std::make_unique<Wt::WComboBox>()};
		auto* transcodeModeRaw {transcodeMode.get()};
		transcodeMode->setModel(model->getTranscodeModeModel());
		t->setFormWidget(SettingsModel::TranscodeModeField, std::move(transcodeMode));

		// Format
		auto transcodeFormat {std::make_unique<Wt::WComboBox>()};
		transcodeFormat->setModel(model->getTranscodeFormatModel());
		t->setFormWidget(SettingsModel::TranscodeFormatField, std::move(transcodeFormat));

		// Bitrate
		auto transcodeBitrate {std::make_unique<Wt::WComboBox>()};
		transcodeBitrate->setModel(model->getTranscodeBitrateModel());
		t->setFormWidget(SettingsModel::TranscodeBitrateField, std::move(transcodeBitrate));

		transcodeModeRaw->activated().connect([=](int row)
		{
			const bool enable {model->getTranscodeModeModel()->getValue(row) != MediaPlayer::Settings::Transcode::Mode::Never};
			model->setReadOnly(SettingsModel::TranscodeFormatField, !enable);
			model->setReadOnly(SettingsModel::TranscodeBitrateField, !enable);
			t->updateModel(model.get());
			t->updateView(model.get());
		});

		// Replay gain mode
		auto replayGainMode {std::make_unique<Wt::WComboBox>()};
		auto* replayGainModeRaw {replayGainMode.get()};
		replayGainMode->setModel(model->getReplayGainModeModel());
		t->setFormWidget(SettingsModel::ReplayGainModeField, std::move(replayGainMode));

		// Replay gain preampGain
		auto replayGainPreampGain {std::make_unique<Wt::WDoubleSpinBox>()};
		replayGainPreampGain->setRange(MediaPlayer::Settings::ReplayGain::minPreAmpGain, MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
		t->setFormWidget(SettingsModel::ReplayGainPreAmpGainField, std::move(replayGainPreampGain));

		// Replay gain preampGain if no info
		auto replayGainPreampGainIfNoInfo {std::make_unique<Wt::WDoubleSpinBox>()};
		replayGainPreampGainIfNoInfo->setRange(MediaPlayer::Settings::ReplayGain::minPreAmpGain, MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
		t->setFormWidget(SettingsModel::ReplayGainPreAmpGainIfNoInfoField, std::move(replayGainPreampGainIfNoInfo));

		replayGainModeRaw->activated().connect([=](int row)
		{
			const bool enable {model->getReplayGainModeModel()->getValue(row) != MediaPlayer::Settings::ReplayGain::Mode::None};
			model->setReadOnly(SettingsModel::SettingsModel::ReplayGainPreAmpGainField, !enable);
			model->setReadOnly(SettingsModel::SettingsModel::ReplayGainPreAmpGainIfNoInfoField, !enable);
			t->updateModel(model.get());
			t->updateView(model.get());
		});
		if (LmsApp->getMediaPlayer().getSettings()->replayGain.mode == MediaPlayer::Settings::ReplayGain::Mode::None)
		{
			model->setReadOnly(SettingsModel::SettingsModel::ReplayGainPreAmpGainField, true);
			model->setReadOnly(SettingsModel::SettingsModel::ReplayGainPreAmpGainIfNoInfoField, true);
		}
	}

	// Subsonic
	{
		t->setCondition("if-has-subsonic-api", Service<IConfig>::get()->getBool("api-subsonic", true));

		// Format
		auto transcodeFormat {std::make_unique<Wt::WComboBox>()};
		transcodeFormat->setModel(model->getTranscodeFormatModel());
		t->setFormWidget(SettingsModel::SubsonicTranscodeFormatField, std::move(transcodeFormat));

		// Bitrate
		auto transcodeBitrate {std::make_unique<Wt::WComboBox>()};
		transcodeBitrate->setModel(model->getTranscodeBitrateModel());
		t->setFormWidget(SettingsModel::SubsonicTranscodeBitrateField, std::move(transcodeBitrate));

		// Artist list mode
		auto artistListMode {std::make_unique<Wt::WComboBox>()};
		artistListMode->setModel(model->getSubsonicArtistListModeModel());
		t->setFormWidget(SettingsModel::SubsonicArtistListModeField, std::move(artistListMode));
	}

	// Scrobbling
	{
		auto scrobbler {std::make_unique<Wt::WComboBox>()};
		scrobbler->setModel(model->getScrobblerModel());
		auto* scrobblerRaw {scrobbler.get()};
		t->setFormWidget(SettingsModel::ScrobblerField, std::move(scrobbler));

		auto listenbrainzToken {std::make_unique<Wt::WLineEdit>()};
		t->setFormWidget(SettingsModel::ListenBrainzTokenField, std::move(listenbrainzToken));
		scrobblerRaw->activated().connect([=](int row)
		{
			const bool enable {model->getScrobblerModel()->getValue(row) == Scrobbler::ListenBrainz};
			model->setReadOnly(SettingsModel::ListenBrainzTokenField, !enable);
			model->validator(SettingsModel::ListenBrainzTokenField)->setMandatory(enable);
			t->updateModel(model.get());
			t->updateView(model.get());
		});
	}

	// Buttons
	Wt::WPushButton *saveBtn {t->bindWidget("apply-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.apply")))};
	Wt::WPushButton *discardBtn {t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard")))};

	saveBtn->clicked().connect([=]
	{
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			if (LmsApp->getUser()->isDemo())
			{
				LmsApp->notifyMsg(Notification::Type::Warning, Wt::WString::tr("Lms.Settings.settings"), Wt::WString::tr("Lms.Settings.demo-cannot-save"));
				return;
			}
		}

		t->updateModel(model.get());

		if (model->validate())
		{
			model->saveData();
			LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Settings.settings"), Wt::WString::tr("Lms.Settings.settings-saved"));
		}

		// Udate the view: Delete any validation message in the view, etc.
		t->updateView(model.get());
	});

	discardBtn->clicked().connect([=]
	{
		model->loadData();
		model->validate();
		t->updateView(model.get());
	});

	t->updateView(model.get());
}

} // namespace UserInterface


