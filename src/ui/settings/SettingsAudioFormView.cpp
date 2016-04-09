/*
 * Copyright (C) 2013 Emeric Poupon
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

#include <vector>

#include <Wt/WStringListModel>
#include <Wt/WFormModel>
#include <Wt/WComboBox>
#include <Wt/WPushButton>

#include "logger/Logger.hpp"

#include "common/Validators.hpp"
#include "LmsApplication.hpp"

#include "SettingsAudioFormView.hpp"

namespace {
std::string encodingToString(Database::AudioEncoding encoding)
{

	switch (encoding)
	{
		case Database::AudioEncoding::AUTO: return "Automatic";
		case Database::AudioEncoding::MP3: return "MP3";
		case Database::AudioEncoding::OGA: return "OGG";
		case Database::AudioEncoding::WEBMA: return "WebM";
	}

	return "?";
}
}

namespace UserInterface {
namespace Settings {

class AudioFormModel : public Wt::WFormModel
{
	public:

		// Associate each field with a unique string literal.
		static const Field BitrateField;
		static const Field EncodingField;

		AudioFormModel(Wt::WObject *parent = 0)
			: Wt::WFormModel(parent)
		{
			initializeModels();

			addField(BitrateField);
			addField(EncodingField);

			setValidator(BitrateField, new Wt::WValidator(true)); // mandatory
			setValidator(EncodingField, new Wt::WValidator(true)); // mandatory

			// populate the model with initial data
			loadData();
		}

		Wt::WAbstractItemModel *bitrateModel() { return _bitrateModel; }
		Wt::WAbstractItemModel *encodingModel() { return _encodingModel; }

		void loadData()
		{
			Wt::Dbo::Transaction transaction(DboSession());

			setValue(BitrateField, std::min(CurrentUser()->getMaxAudioBitrate(), CurrentUser()->getAudioBitrate()) / 1000); // in kps
			int encodingRow = getAudioEncodingRow( CurrentUser()->getAudioEncoding());
			setValue(EncodingField, getAudioEncodingString(encodingRow));
		}

		bool saveData(Wt::WString& error)
		{
			// DBO transaction active here
			try {
				Wt::Dbo::Transaction transaction(DboSession());

				// user may have been deleted by someone else
				CurrentUser().modify()->setAudioBitrate( Wt::asNumber(value(BitrateField)) * 1000); // in kbps

				int encodingRow = getAudioEncodingRow( boost::any_cast<Wt::WString>(value((EncodingField))));
				CurrentUser().modify()->setAudioEncoding( getAudioEncodingValue(encodingRow));
			}
			catch(Wt::Dbo::Exception& exception)
			{
				LMS_LOG(UI, ERROR) << "Dbo exception: " << exception.what();
				return false;
			}

			return true;
		}

		int getAudioEncodingRow( Database::AudioEncoding encoding)
		{
			for (int i = 0; i < _encodingModel->rowCount(); ++i)
			{
				if (getAudioEncodingValue(i) == encoding)
					return i;
			}
			return -1;
		}

		int getAudioEncodingRow( Wt::WString encoding)
		{
			for (int i = 0; i < _encodingModel->rowCount(); ++i)
			{
				if (getAudioEncodingString(i) == encoding)
					return i;
			}
			return -1;
		}

		Database::AudioEncoding getAudioEncodingValue(int row)
		{
			return boost::any_cast<Database::AudioEncoding>
				(_encodingModel->data(_encodingModel->index(row, 0), Wt::UserRole));
		}

		Wt::WString getAudioEncodingString(int row)
		{
			return boost::any_cast<Wt::WString>
				(_encodingModel->data(_encodingModel->index(row, 0), Wt::DisplayRole));
		}


	private:
		void initializeModels()
		{
			Wt::Dbo::Transaction transaction(DboSession());

			_bitrateModel = new Wt::WStringListModel(this);
			BOOST_FOREACH(std::size_t bitrate, Database::User::audioBitrates)
			{
				if (bitrate <= CurrentUser()->getMaxAudioBitrate())
					_bitrateModel->addString( Wt::WString("{1}").arg( bitrate / 1000 ) ); // in kbps
			}

			_encodingModel = new Wt::WStringListModel(this);
			int row = 0;
			BOOST_FOREACH(Database::AudioEncoding encoding, Database::User::audioEncodings)
			{
				_encodingModel->addString(encodingToString(encoding));
				_encodingModel->setData(row, 0, encoding, Wt::UserRole);
				row++;
			}
		}

		std::string		_userId;
		Wt::WStringListModel*	_bitrateModel;
		Wt::WStringListModel*	_encodingModel;
};

const Wt::WFormModel::Field AudioFormModel::BitrateField = "bitrate";
const Wt::WFormModel::Field AudioFormModel::EncodingField = "encoding";

AudioFormView::AudioFormView(Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{

	_model = new AudioFormModel(this);

	setTemplateText(tr("settings-audio"));
	addFunction("id", &WTemplate::Functions::id);
	addFunction("block", &WTemplate::Functions::id);

	_applyInfo = new Wt::WText();
	_applyInfo->setInline(false);
	_applyInfo->hide();
	bindWidget("apply-info", _applyInfo);

	// Bitrate
	Wt::WComboBox *bitrateCB = new Wt::WComboBox();
	setFormWidget(AudioFormModel::BitrateField, bitrateCB);
	bitrateCB->setStyleClass("span2");
	bitrateCB->setModel(_model->bitrateModel());
	bitrateCB->changed().connect(_applyInfo, &Wt::WWidget::hide);

	// Encoding
	Wt::WComboBox *encodingCB = new Wt::WComboBox();
	setFormWidget(AudioFormModel::EncodingField, encodingCB);
	encodingCB->setModel(_model->encodingModel());
	encodingCB->changed().connect(_applyInfo, &Wt::WWidget::hide);

	// Title & Buttons
	bindString("title", "Audio settings");

	Wt::WPushButton *saveButton = new Wt::WPushButton("Apply");
	saveButton->setStyleClass("btn-primary");
	bindWidget("save-button", saveButton);
	saveButton->clicked().connect(this, &AudioFormView::processSave);

	Wt::WPushButton *cancelButton = new Wt::WPushButton("Discard");
	bindWidget("cancel-button", cancelButton);
	cancelButton->clicked().connect(this, &AudioFormView::processCancel);

	updateView(_model);
}

void
AudioFormView::processCancel()
{
	_applyInfo->show();

	_applyInfo->setText( Wt::WString::fromUTF8("Parameters reverted!"));
	_applyInfo->setStyleClass("alert alert-info");
	_model->loadData();

	_model->validate();
	updateView(_model);
}

void
AudioFormView::processSave()
{
	updateModel(_model);

	_applyInfo->show();
	if (_model->validate())
	{
		Wt::WString error;
		// commit model into DB
		if (_model->saveData(error) )
		{
			_applyInfo->setText( Wt::WString::fromUTF8("New parameters successfully applied!"));
			_applyInfo->setStyleClass("alert alert-success");
		}
		else
		{
			_applyInfo->setText( Wt::WString::fromUTF8("Cannot apply new parameters: ") + error);
			_applyInfo->setStyleClass("alert alert-danger");
		}
	}
	else
	{
		_applyInfo->setText( Wt::WString::fromUTF8("Cannot apply new parameters!"));
		_applyInfo->setStyleClass("alert alert-danger");
	}
	updateView(_model);
}

} // namespace Settings
} // namespace UserInterface
