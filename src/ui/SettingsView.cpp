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

#include <Wt/WString.h>
#include <Wt/WPushButton.h>
#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WLineEdit.h>

#include <Wt/WFormModel.h>
#include <Wt/WStringListModel.h>

#include "common/Validators.hpp"

#include "utils/Logger.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

using namespace Database;

class SettingsModel : public Wt::WFormModel
{
	public:
		// Associate each field with a unique string literal.
		static const Field TranscodeEnableField;
		static const Field TranscodeFormatField;
		static const Field TranscodeBitrateField;
		static const Field PasswordField;
		static const Field PasswordConfirmField;

		SettingsModel()
			: Wt::WFormModel()
		{
			initializeModels();

			addField(TranscodeEnableField);
			addField(TranscodeBitrateField);
			addField(TranscodeFormatField);
			addField(PasswordField);
			addField(PasswordConfirmField);

			setValidator(TranscodeBitrateField, createMandatoryValidator());
			setValidator(TranscodeFormatField, createMandatoryValidator());

			loadData();
		}

		std::shared_ptr<Wt::WAbstractItemModel> transcodeBitrateModel() { return _transcodeBitrateModel; }
		std::shared_ptr<Wt::WAbstractItemModel> transcodeFormatModel() { return _transcodeFormatModel; }

		void loadData()
		{
			Wt::Dbo::Transaction transaction {LmsApp->getDboSession()};

			setValue(TranscodeEnableField, LmsApp->getUser()->getAudioTranscodeEnable());
			if (!LmsApp->getUser()->getAudioTranscodeEnable())
			{
				setReadOnly(TranscodeFormatField, true);
				setReadOnly(TranscodeBitrateField, true);
			}

			auto transcodeBitrateRow {getTranscodeBitrateRow(LmsApp->getUser()->getAudioTranscodeBitrate())};
			if (transcodeBitrateRow)
				setValue(TranscodeBitrateField, transcodeBitrateString(*transcodeBitrateRow));

			auto transcodeFormatRow {getTranscodeFormatRow(LmsApp->getUser()->getAudioTranscodeFormat())};
			if (transcodeFormatRow)
				setValue(TranscodeFormatField, transcodeFormatString(*transcodeFormatRow));
		}

		void saveData()
		{
			Wt::Dbo::Transaction transaction {LmsApp->getDboSession()};

			LmsApp->getUser().modify()->setAudioTranscodeEnable(Wt::asNumber(value(TranscodeEnableField)));

			auto transcodeBitrateRow {getTranscodeBitrateRow(Wt::asString(value(TranscodeBitrateField)))};
			if (transcodeBitrateRow)
				LmsApp->getUser().modify()->setAudioTranscodeBitrate(transcodeBitrate(*transcodeBitrateRow));

			auto transcodeFormatRow {getTranscodeFormatRow(Wt::asString(value(TranscodeFormatField)))};
			if (transcodeFormatRow)
				LmsApp->getUser().modify()->setAudioTranscodeFormat(transcodeFormat(*transcodeFormatRow));

			if (!valueText(PasswordField).empty())
				Database::Handler::getPasswordService().updatePassword(LmsApp->getAuthUser(), valueText(PasswordField));
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == PasswordField)
			{
				if (!valueText(PasswordField).empty())
				{
					// Evaluate the strength of the password
					auto res = Database::Handler::getPasswordService().strengthValidator()->evaluateStrength(valueText(PasswordField), LmsApp->getUserIdentity(), "");

					if (!res.isValid())
						error = res.message();
				}
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

		boost::optional<int> getTranscodeBitrateRow(Wt::WString value)
		{
			for (int i = 0; i < _transcodeBitrateModel->rowCount(); ++i)
			{
				if (transcodeBitrateString(i) == value)
					return i;
			}

			return boost::none;
		}

		boost::optional<int> getTranscodeBitrateRow(std::size_t value)
		{
			for (int i = 0; i < _transcodeBitrateModel->rowCount(); ++i)
			{
				if (transcodeBitrate(i) == value)
					return i;
			}

			return boost::none;
		}

		std::size_t transcodeBitrate(int row)
		{
			return Wt::cpp17::any_cast<std::size_t>
				(_transcodeBitrateModel->data(_transcodeBitrateModel->index(row, 0), Wt::ItemDataRole::User));
		}

		Wt::WString transcodeBitrateString(int row)
		{
			return Wt::cpp17::any_cast<Wt::WString>
				(_transcodeBitrateModel->data(_transcodeBitrateModel->index(row, 0), Wt::ItemDataRole::Display));
		}

		boost::optional<int> getTranscodeFormatRow(Wt::WString value)
		{
			for (int i = 0; i < _transcodeFormatModel->rowCount(); ++i)
			{
				if (transcodeFormatString(i) == value)
					return i;
			}

			return boost::none;
		}

		boost::optional<int> getTranscodeFormatRow(Database::AudioFormat format)
		{
			for (int i = 0; i < _transcodeFormatModel->rowCount(); ++i)
			{
				if (transcodeFormat(i) == format)
					return i;
			}

			return boost::none;
		}

		Database::AudioFormat transcodeFormat(int row)
		{
			return Wt::cpp17::any_cast<Database::AudioFormat>
				(_transcodeFormatModel->data(_transcodeFormatModel->index(row, 0), Wt::ItemDataRole::User));
		}

		Wt::WString transcodeFormatString(int row)
		{
			return Wt::cpp17::any_cast<Wt::WString>
				(_transcodeFormatModel->data(_transcodeFormatModel->index(row, 0), Wt::ItemDataRole::Display));
		}


	private:

		void initializeModels()
		{
			_transcodeBitrateModel = std::make_shared<Wt::WStringListModel>();

			Database::Bitrate maxAudioBitrate;
			{
				Wt::Dbo::Transaction transaction {LmsApp->getDboSession()};
				maxAudioBitrate = LmsApp->getUser()->getMaxAudioTranscodeBitrate();
			}

			std::size_t id = 0;
			for (Database::Bitrate bitrate : Database::User::audioTranscodeAllowedBitrates)
			{
				if (bitrate > maxAudioBitrate)
					break;

				_transcodeBitrateModel->addString( Wt::WString::fromUTF8(std::to_string(bitrate / 1000)) );
				_transcodeBitrateModel->setData( id, 0, bitrate, Wt::ItemDataRole::User);
				id++;
			}

			_transcodeFormatModel = std::make_shared<Wt::WStringListModel>();

			_transcodeFormatModel->addString(Wt::WString::tr("Lms.Settings.transcoding.mp3"));
			_transcodeFormatModel->setData(0, 0, Database::AudioFormat::MP3, Wt::ItemDataRole::User);

			_transcodeFormatModel->addString(Wt::WString::tr("Lms.Settings.transcoding.ogg_opus"));
			_transcodeFormatModel->setData(1, 0, Database::AudioFormat::OGG_OPUS, Wt::ItemDataRole::User);

			_transcodeFormatModel->addString(Wt::WString::tr("Lms.Settings.transcoding.ogg_vorbis"));
			_transcodeFormatModel->setData(2, 0, Database::AudioFormat::OGG_VORBIS, Wt::ItemDataRole::User);

			_transcodeFormatModel->addString(Wt::WString::tr("Lms.Settings.transcoding.webm_vorbis"));
			_transcodeFormatModel->setData(3, 0, Database::AudioFormat::WEBM_VORBIS, Wt::ItemDataRole::User);
		}

		std::shared_ptr<Wt::WStringListModel>	_transcodeBitrateModel;
		std::shared_ptr<Wt::WStringListModel>	_transcodeFormatModel;

};

const Wt::WFormModel::Field SettingsModel::TranscodeEnableField		= "transcoding-enable";
const Wt::WFormModel::Field SettingsModel::TranscodeBitrateField	= "transcoding-bitrate";
const Wt::WFormModel::Field SettingsModel::TranscodeFormatField		= "transcoding-format";
const Wt::WFormModel::Field SettingsModel::PasswordField		= "password";
const Wt::WFormModel::Field SettingsModel::PasswordConfirmField		= "password-confirm";

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

	auto model {std::make_shared<SettingsModel>()};

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
			Wt::Dbo::Transaction transaction {LmsApp->getDboSession()};

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


