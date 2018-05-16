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
		static const Field BitrateField;
		static const Field EncodingField;
		static const Field PasswordField;
		static const Field PasswordConfirmField;

		SettingsModel()
			: Wt::WFormModel()
		{
			initializeModels();

			addField(BitrateField);
			addField(EncodingField);
			addField(PasswordField);
			addField(PasswordConfirmField);

			setValidator(BitrateField, createMandatoryValidator());
			setValidator(EncodingField, createMandatoryValidator());

			loadData();
		}

		std::shared_ptr<Wt::WAbstractItemModel> bitrateModel() { return _bitrateModel; }
		std::shared_ptr<Wt::WAbstractItemModel> encodingModel() { return _encodingModel; }

		void loadData()
		{
			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			auto bitrate = getBitrateRow(LmsApp->getCurrentUser()->getAudioBitrate());
			if (bitrate)
				setValue(BitrateField, bitrateString(*bitrate));

			auto encodingRow = getEncodingRow(LmsApp->getCurrentUser()->getAudioEncoding());
			if (encodingRow)
				setValue(EncodingField, encodingString(*encodingRow));

		}

		void saveData()
		{
			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			auto bitrateRow = getBitrateRow(Wt::asString(value(BitrateField)));
			assert(bitrateRow);
			LmsApp->getCurrentUser().modify()->setAudioBitrate(bitrate(*bitrateRow));

			auto encodingRow = getEncodingRow(Wt::asString(value(EncodingField)));
			LmsApp->getCurrentUser().modify()->setAudioEncoding(encoding(*encodingRow));

			if (!valueText(PasswordField).empty())
			{
				Database::Handler::getPasswordService().updatePassword(LmsApp->getCurrentAuthUser(), valueText(PasswordField));
			}
		}

		bool validateField(Field field)
		{
			Wt::WString error;

			if (field == PasswordField)
			{
				if (!valueText(PasswordField).empty())
				{
					// Evaluate the strength of the password
					auto res = Database::Handler::getPasswordService().strengthValidator()->evaluateStrength(valueText(PasswordField),
						LmsApp->getCurrentAuthUser().identity(Wt::Auth::Identity::LoginName), "");

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

		boost::optional<int> getBitrateRow(Wt::WString value)
		{
			for (int i = 0; i < _bitrateModel->rowCount(); ++i)
			{
				if (bitrateString(i) == value)
					return i;
			}

			return boost::none;
		}

		boost::optional<int> getBitrateRow(std::size_t value)
		{
			for (int i = 0; i < _bitrateModel->rowCount(); ++i)
			{
				if (bitrate(i) == value)
					return i;
			}

			return boost::none;
		}

		std::size_t bitrate(int row)
		{
			return Wt::cpp17::any_cast<std::size_t>
				(_bitrateModel->data(_bitrateModel->index(row, 0), Wt::ItemDataRole::User));
		}

		Wt::WString bitrateString(int row)
		{
			return Wt::cpp17::any_cast<Wt::WString>
				(_bitrateModel->data(_bitrateModel->index(row, 0), Wt::ItemDataRole::Display));
		}

		boost::optional<int> getEncodingRow(Wt::WString value)
		{
			for (int i = 0; i < _encodingModel->rowCount(); ++i)
			{
				if (encodingString(i) == value)
					return i;
			}

			return boost::none;
		}

		boost::optional<int> getEncodingRow(Database::AudioEncoding enc)
		{
			for (int i = 0; i < _encodingModel->rowCount(); ++i)
			{
				if (encoding(i) == enc)
					return i;
			}

			return boost::none;
		}

		Database::AudioEncoding encoding(int row)
		{
			return Wt::cpp17::any_cast<Database::AudioEncoding>
				(_encodingModel->data(_encodingModel->index(row, 0), Wt::ItemDataRole::User));
		}

		Wt::WString encodingString(int row)
		{
			return Wt::cpp17::any_cast<Wt::WString>
				(_encodingModel->data(_encodingModel->index(row, 0), Wt::ItemDataRole::Display));
		}


	private:

		void initializeModels()
		{
			_bitrateModel = std::make_shared<Wt::WStringListModel>();

			std::size_t maxAudioBitrate;
			{
				Wt::Dbo::Transaction transaction(LmsApp->getDboSession());
				maxAudioBitrate = LmsApp->getCurrentUser()->getMaxAudioBitrate();
			}

			std::size_t id = 0;
			for (std::size_t bitrate : Database::User::audioBitrates)
			{
				if (bitrate > maxAudioBitrate)
					break;

				_bitrateModel->addString( Wt::WString::fromUTF8(std::to_string(bitrate / 1000)) );
				_bitrateModel->setData( id, 0, bitrate, Wt::ItemDataRole::User);
				id++;
			}

			_encodingModel = std::make_shared<Wt::WStringListModel>();

			_encodingModel->addString(Wt::WString::tr("Lms.Settings.auto"));
			_encodingModel->setData(0, 0, Database::AudioEncoding::AUTO, Wt::ItemDataRole::User);

			_encodingModel->addString(Wt::WString::tr("Lms.Settings.mp3"));
			_encodingModel->setData(1, 0, Database::AudioEncoding::MP3, Wt::ItemDataRole::User);

			_encodingModel->addString(Wt::WString::tr("Lms.Settings.oga"));
			_encodingModel->setData(2, 0, Database::AudioEncoding::OGA, Wt::ItemDataRole::User);

			_encodingModel->addString(Wt::WString::tr("Lms.Settings.webma"));
			_encodingModel->setData(3, 0, Database::AudioEncoding::WEBMA, Wt::ItemDataRole::User);
		}

		std::shared_ptr<Wt::WStringListModel>	_bitrateModel;
		std::shared_ptr<Wt::WStringListModel>	_encodingModel;

};

const Wt::WFormModel::Field SettingsModel::BitrateField		= "bitrate";
const Wt::WFormModel::Field SettingsModel::EncodingField	= "encoding";
const Wt::WFormModel::Field SettingsModel::PasswordField	= "password";
const Wt::WFormModel::Field SettingsModel::PasswordConfirmField	= "password-confirm";

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

	auto t = addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Settings.template"));

	auto model = std::make_shared<SettingsModel>();

	// Password
	auto password = std::make_unique<Wt::WLineEdit>();
	password->setEchoMode(Wt::EchoMode::Password);
	t->setFormWidget(SettingsModel::PasswordField, std::move(password));

	// Password confirm
	auto passwordConfirm = std::make_unique<Wt::WLineEdit>();
	passwordConfirm->setEchoMode(Wt::EchoMode::Password);
	t->setFormWidget(SettingsModel::PasswordConfirmField, std::move(passwordConfirm));

	// Bitrate
	auto bitrate = std::make_unique<Wt::WComboBox>();
	bitrate->setModel(model->bitrateModel());
	t->setFormWidget(SettingsModel::BitrateField, std::move(bitrate));

	// Encoding
	auto encoding = std::make_unique<Wt::WComboBox>();
	encoding->setModel(model->encodingModel());
	t->setFormWidget(SettingsModel::EncodingField, std::move(encoding));

	// Buttons
	Wt::WPushButton *saveBtn = t->bindWidget("apply-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.apply")));
	Wt::WPushButton *discardBtn = t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard")));

	saveBtn->clicked().connect(std::bind([=] ()
	{
		t->updateModel(model.get());

		if (model->validate())
		{
			model->saveData();
			LmsApp->notifyMsg(Wt::WString::tr("Lms.Settings.settings-saved"));
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


