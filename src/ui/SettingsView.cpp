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

#include <Wt/WString>
#include <Wt/WPushButton>
#include <Wt/WComboBox>
#include <Wt/WLineEdit>

#include <Wt/WFormModel>
#include <Wt/WStringListModel>

#include "common/Validators.hpp"

#include "utils/Logger.hpp"
#include "LmsApplication.hpp"

#include "SettingsView.hpp"

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

		SettingsModel(Wt::WObject *parent = 0)
			: Wt::WFormModel(parent)
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

		Wt::WAbstractItemModel *bitrateModel() { return _bitrateModel; }
		Wt::WAbstractItemModel *encodingModel() { return _encodingModel; }

		void loadData()
		{
			Wt::Dbo::Transaction transaction(DboSession());

			auto bitrate = getBitrateRow(CurrentUser()->getAudioBitrate());
			if (bitrate)
				setValue(BitrateField, bitrateString(*bitrate));

			auto encodingRow = getEncodingRow(CurrentUser()->getAudioEncoding());
			if (encodingRow)
				setValue(EncodingField, encodingString(*encodingRow));

		}

		void saveData()
		{
			Wt::Dbo::Transaction transaction(DboSession());

			auto bitrateRow = getBitrateRow(Wt::asString(value(BitrateField)));
			assert(bitrateRow);
			CurrentUser().modify()->setAudioBitrate(bitrate(*bitrateRow));

			auto encodingRow = getEncodingRow(Wt::asString(value(EncodingField)));
			CurrentUser().modify()->setAudioEncoding(encoding(*encodingRow));

			if (!valueText(PasswordField).empty())
			{
				Database::Handler::getPasswordService().updatePassword(CurrentAuthUser(), valueText(PasswordField));
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
						CurrentAuthUser().identity(Wt::Auth::Identity::LoginName), "");

					if (!res.isValid())
						error = res.message();
				}
				else
					return Wt::WFormModel::validateField(field);
			}
			else if (field == PasswordConfirmField)
			{
				if (validation(PasswordField).state() == Wt::WValidator::Valid)
				{
					if (valueText(PasswordField) != valueText(PasswordConfirmField))
						error = Wt::WString::tr("Lms.passwords-dont-match");
				}
			}
			else
			{
				return Wt::WFormModel::validateField(field);
			}

			setValidation(field, Wt::WValidator::Result( error.empty() ? Wt::WValidator::Valid : Wt::WValidator::Invalid, error));

			return (validation(field).state() == Wt::WValidator::Valid);
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
			return boost::any_cast<std::size_t>
				(_bitrateModel->data(_encodingModel->index(row, 0), Wt::UserRole));
		}

		Wt::WString bitrateString(int row)
		{
			return boost::any_cast<Wt::WString>
				(_bitrateModel->data(_encodingModel->index(row, 0), Wt::DisplayRole));
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
			return boost::any_cast<Database::AudioEncoding>
				(_encodingModel->data(_encodingModel->index(row, 0), Wt::UserRole));
		}

		Wt::WString encodingString(int row)
		{
			return boost::any_cast<Wt::WString>
				(_encodingModel->data(_encodingModel->index(row, 0), Wt::DisplayRole));
		}


	private:

		void initializeModels()
		{

			_bitrateModel = new Wt::WStringListModel(this);

			std::size_t id = 0;
			for (auto bitrate : Database::User::audioBitrates)
			{
				_bitrateModel->addString( Wt::WString::fromUTF8(std::to_string(bitrate / 1000)) );
				_bitrateModel->setData( id++, 0, bitrate, Wt::UserRole);
			}

			_encodingModel = new Wt::WStringListModel(this);

			_encodingModel->addString(Wt::WString::tr("Lms.Settings.auto"));
			_encodingModel->setData(0, 0, Database::AudioEncoding::AUTO, Wt::UserRole);

			_encodingModel->addString(Wt::WString::tr("Lms.Settings.mp3"));
			_encodingModel->setData(1, 0, Database::AudioEncoding::MP3, Wt::UserRole);

			_encodingModel->addString(Wt::WString::tr("Lms.Settings.oga"));
			_encodingModel->setData(2, 0, Database::AudioEncoding::OGA, Wt::UserRole);

			_encodingModel->addString(Wt::WString::tr("Lms.Settings.webma"));
			_encodingModel->setData(3, 0, Database::AudioEncoding::WEBMA, Wt::UserRole);
		}

		Wt::WStringListModel*	_bitrateModel;
		Wt::WStringListModel*	_encodingModel;

};

const Wt::WFormModel::Field SettingsModel::BitrateField		= "bitrate";
const Wt::WFormModel::Field SettingsModel::EncodingField	= "encoding";
const Wt::WFormModel::Field SettingsModel::PasswordField	= "password";
const Wt::WFormModel::Field SettingsModel::PasswordConfirmField	= "password-confirm";

SettingsView::SettingsView(Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{
	auto model = new SettingsModel(this);

	setTemplateText(tr("Lms.Settings.template"));
	addFunction("tr", &WTemplate::Functions::tr);
	addFunction("id", &WTemplate::Functions::id);

	// Password
	Wt::WLineEdit *password = new Wt::WLineEdit();
	setFormWidget(SettingsModel::PasswordField, password);
	password->setEchoMode(Wt::WLineEdit::Password);

	// Password confirm
	Wt::WLineEdit *passwordConfirm = new Wt::WLineEdit();
	setFormWidget(SettingsModel::PasswordConfirmField, passwordConfirm);
	passwordConfirm->setEchoMode(Wt::WLineEdit::Password);

	// Bitrate
	Wt::WComboBox *bitrate = new Wt::WComboBox();
	setFormWidget(SettingsModel::BitrateField, bitrate);
	bitrate->setModel(model->bitrateModel());

	// Encoding
	Wt::WComboBox *encoding = new Wt::WComboBox();
	setFormWidget(SettingsModel::EncodingField, encoding);
	encoding->setModel(model->encodingModel());

	// Buttons
	Wt::WPushButton *saveBtn = new Wt::WPushButton(Wt::WString::tr("Lms.apply"));
	bindWidget("apply-btn", saveBtn);

	Wt::WPushButton *discardBtn = new Wt::WPushButton(Wt::WString::tr("Lms.discard"));
	bindWidget("discard-btn", discardBtn);

	saveBtn->clicked().connect(std::bind([=] ()
	{
		updateModel(model);

		if (model->validate())
		{
			model->saveData();
			LmsApp->notify(Wt::WString::tr("Lms.Settings.settings-saved"));
		}

		// Udate the view: Delete any validation message in the view, etc.
		updateView(model);
	}));

	discardBtn->clicked().connect(std::bind([=] ()
	{
		model->loadData();
		model->validate();
		updateView(model);
	}));

	updateView(model);
}

} // namespace UserInterface


