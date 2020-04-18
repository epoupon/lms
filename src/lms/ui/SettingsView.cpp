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
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WTemplateFormView.h>

#include "common/Validators.hpp"
#include "common/ValueStringModel.hpp"

#include "auth/IPasswordService.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

#include "LmsApplication.hpp"
#include "LmsTheme.hpp"

namespace UserInterface {

using namespace Database;

class SettingsModel : public Wt::WFormModel
{
	public:
		// Associate each field with a unique string literal.
		static inline const Field DarkModeField {"dark-mode"};
		static inline const Field TranscodeEnableField {"transcoding-enable"};
		static inline const Field TranscodeFormatField {"transcoding-format"};
		static inline const Field TranscodeBitrateField {"transcoding-bitrate"};
		static inline const Field PasswordOldField {"password-old"};
		static inline const Field PasswordField {"password"};
		static inline const Field PasswordConfirmField {"password-confirm"};

		SettingsModel(bool withOldPassword)
			: _withOldPassword {withOldPassword}
		{
			initializeModels();

			addField(DarkModeField);
			addField(TranscodeEnableField);
			addField(TranscodeBitrateField);
			addField(TranscodeFormatField);

			if (_withOldPassword)
				addField(PasswordOldField);

			addField(PasswordField);
			addField(PasswordConfirmField);

			setValidator(TranscodeBitrateField, createMandatoryValidator());
			setValidator(TranscodeFormatField, createMandatoryValidator());

			loadData();
		}

		std::shared_ptr<Wt::WAbstractItemModel> transcodeBitrateModel() { return _transcodeBitrateModel; }
		std::shared_ptr<Wt::WAbstractItemModel> transcodeFormatModel() { return _transcodeFormatModel; }

		void saveData()
		{
			User::PasswordHash passwordHash;

			if (!valueText(PasswordField).empty())
				passwordHash = ServiceProvider<::Auth::IPasswordService>::get()->hashPassword(valueText(PasswordField).toUTF8());

			auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

			User::pointer user {LmsApp->getUser()};

			{
				const User::UITheme newTheme {Wt::asNumber(value(DarkModeField)) ? User::UITheme::Dark : User::UITheme::Light};
				LmsTheme* lmsTheme {static_cast<LmsTheme*>(LmsApp->theme().get())};
				lmsTheme->setTheme(newTheme);

				user.modify()->setUITheme(newTheme);
			}

			user.modify()->setAudioTranscodeEnable(Wt::asNumber(value(TranscodeEnableField)));

			auto transcodeBitrateRow {_transcodeBitrateModel->getRowFromString(valueText(TranscodeBitrateField))};
			if (transcodeBitrateRow)
				user.modify()->setAudioTranscodeBitrate(_transcodeBitrateModel->getValue(*transcodeBitrateRow));

			auto transcodeFormatRow {_transcodeFormatModel->getRowFromString(valueText(TranscodeFormatField))};
			if (transcodeFormatRow)
				user.modify()->setAudioTranscodeFormat(_transcodeFormatModel->getValue(*transcodeFormatRow));

			if (!valueText(PasswordField).empty())
			{
				user.modify()->setPasswordHash(passwordHash);
				user.modify()->clearAuthTokens();
			}
		}

		void loadData()
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			setValue(DarkModeField, LmsApp->getUser()->getUITheme() == User::UITheme::Dark);

			setValue(TranscodeEnableField, LmsApp->getUser()->getAudioTranscodeEnable());
			if (!LmsApp->getUser()->getAudioTranscodeEnable())
			{
				setReadOnly(TranscodeFormatField, true);
				setReadOnly(TranscodeBitrateField, true);
			}

			auto transcodeBitrateRow {_transcodeBitrateModel->getRowFromValue(LmsApp->getUser()->getAudioTranscodeBitrate())};
			if (transcodeBitrateRow)
				setValue(TranscodeBitrateField, _transcodeBitrateModel->getString(*transcodeBitrateRow));

			auto transcodeFormatRow {_transcodeFormatModel->getRowFromValue(LmsApp->getUser()->getAudioTranscodeFormat())};
			if (transcodeFormatRow)
				setValue(TranscodeFormatField, _transcodeFormatModel->getString(*transcodeFormatRow));
		}

	private:

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == PasswordOldField)
			{
				if (!valueText(PasswordOldField).empty())
				{
					switch (ServiceProvider<::Auth::IPasswordService>::get()->checkUserPassword(
							LmsApp->getDbSession(),
							boost::asio::ip::address::from_string(LmsApp->environment().clientAddress()),
							LmsApp->getUserLoginName(),
							valueText(PasswordOldField).toUTF8()))
					{
						case ::Auth::IPasswordService::PasswordCheckResult::Match:
							break;
						case ::Auth::IPasswordService::PasswordCheckResult::Mismatch:
							error = Wt::WString::tr("Lms.Settings.password-bad");
							break;
						case ::Auth::IPasswordService::PasswordCheckResult::Throttled:
							error = Wt::WString::tr("Lms.password-client-throttled");
							break;
					}
				}
				else
				{
					if (!valueText(PasswordField).empty())
						error = Wt::WString::tr("Lms.Settings.password-must-fill-old-password");
					else
						return Wt::WFormModel::validateField(field);
				}
			}
			else if (field == PasswordField)
			{
				if (!valueText(PasswordField).empty())
				{
					if (!ServiceProvider<::Auth::IPasswordService>::get()->evaluatePasswordStrength(LmsApp->getUserLoginName(), valueText(PasswordField).toUTF8()))
						error = Wt::WString::tr("Lms.password-too-weak");
				}
				else
				{
					if (!valueText(PasswordOldField).empty())
						error = Wt::WString::tr("Wt.WValidator.Invalid");
					else
						return Wt::WFormModel::validateField(field);
				}
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
			Bitrate maxAudioBitrate;
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};
				maxAudioBitrate = LmsApp->getUser()->getMaxAudioTranscodeBitrate();
			}

			_transcodeBitrateModel = std::make_shared<ValueStringModel<Bitrate>>();
			for (const Bitrate bitrate : User::audioTranscodeAllowedBitrates)
			{
				if (bitrate > maxAudioBitrate)
					break;

				_transcodeBitrateModel->add(Wt::WString::fromUTF8(std::to_string(bitrate / 1000)), bitrate);
			}

			_transcodeFormatModel = std::make_shared<ValueStringModel<AudioFormat>>();
			_transcodeFormatModel->add(Wt::WString::tr("Lms.Settings.transcoding.mp3"), AudioFormat::MP3);
			_transcodeFormatModel->add(Wt::WString::tr("Lms.Settings.transcoding.ogg_opus"), AudioFormat::OGG_OPUS);
			_transcodeFormatModel->add(Wt::WString::tr("Lms.Settings.transcoding.matroska_opus"), AudioFormat::MATROSKA_OPUS);
			_transcodeFormatModel->add(Wt::WString::tr("Lms.Settings.transcoding.ogg_vorbis"), AudioFormat::OGG_VORBIS);
			_transcodeFormatModel->add(Wt::WString::tr("Lms.Settings.transcoding.webm_vorbis"), AudioFormat::WEBM_VORBIS);
		}

		bool _withOldPassword {};

		std::shared_ptr<ValueStringModel<Bitrate>>	_transcodeBitrateModel;
		std::shared_ptr<ValueStringModel<AudioFormat>>	_transcodeFormatModel;
};

SettingsView::SettingsView()
{
	wApp->internalPathChanged().connect(std::bind([=]
	{
		refreshView();
	}));

	refreshView();
}

void
SettingsView::refreshView()
{
	if (!wApp->internalPathMatches("/settings"))
		return;

	clear();

	auto t {addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Settings.template"))};

	auto model {std::make_shared<SettingsModel>(!LmsApp->isUserAuthStrong())};

	// Appearance
	{
		auto darkMode {std::make_unique<Wt::WCheckBox>()};
		t->setFormWidget(SettingsModel::DarkModeField, std::move(darkMode));
	}

	// Old password
	if (!LmsApp->isUserAuthStrong())
	{
		t->setCondition("if-has-old-password", true);
		auto oldPassword {std::make_unique<Wt::WLineEdit>()};
		oldPassword->setEchoMode(Wt::EchoMode::Password);
		t->setFormWidget(SettingsModel::PasswordOldField, std::move(oldPassword));
	}

	// Password
	auto password {std::make_unique<Wt::WLineEdit>()};
	password->setEchoMode(Wt::EchoMode::Password);
	t->setFormWidget(SettingsModel::PasswordField, std::move(password));

	// Password confirm
	auto passwordConfirm {std::make_unique<Wt::WLineEdit>()};
	passwordConfirm->setEchoMode(Wt::EchoMode::Password);
	t->setFormWidget(SettingsModel::PasswordConfirmField, std::move(passwordConfirm));

	// Transcoding
	auto transcode {std::make_unique<Wt::WCheckBox>()};
	auto* transcodeRaw {transcode.get()};
	t->setFormWidget(SettingsModel::TranscodeEnableField, std::move(transcode));

	// Format
	auto transcodeFormat {std::make_unique<Wt::WComboBox>()};
	transcodeFormat->setModel(model->transcodeFormatModel());
	t->setFormWidget(SettingsModel::TranscodeFormatField, std::move(transcodeFormat));

	// Bitrate
	auto transcodeBitrate {std::make_unique<Wt::WComboBox>()};
	transcodeBitrate->setModel(model->transcodeBitrateModel());
	t->setFormWidget(SettingsModel::TranscodeBitrateField, std::move(transcodeBitrate));

	transcodeRaw->changed().connect([=]()
	{
		bool enable {transcodeRaw->checkState() == Wt::CheckState::Checked};
		model->setReadOnly(SettingsModel::TranscodeFormatField, !enable);
		model->setReadOnly(SettingsModel::TranscodeBitrateField, !enable);
		t->updateModel(model.get());
		t->updateView(model.get());
	});

	// Buttons
	Wt::WPushButton *saveBtn {t->bindWidget("apply-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.apply")))};
	Wt::WPushButton *discardBtn {t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard")))};

	saveBtn->clicked().connect(std::bind([=] ()
	{

		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			if (LmsApp->getUser()->isDemo())
			{
				LmsApp->notifyMsg(MsgType::Warning, Wt::WString::tr("Lms.Settings.demo-cannot-save"));
				return;
			}
		}

		t->updateModel(model.get());

		if (model->validate())
		{
			model->saveData();
			LmsApp->notifyMsg(MsgType::Success, Wt::WString::tr("Lms.Settings.settings-saved"));
		}

		// Udate the view: Delete any validation message in the view, etc.
		t->updateView(model.get());
	}));

	discardBtn->clicked().connect(std::bind([=] ()
	{
		model->loadData();
		model->validate();
		t->updateView(model.get());
	}));

	t->updateView(model.get());
}

} // namespace UserInterface


