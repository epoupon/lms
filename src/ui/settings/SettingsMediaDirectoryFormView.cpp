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
#include <Wt/WLineEdit>
#include <Wt/WFormModel>
#include <Wt/WComboBox>
#include <Wt/WPushButton>

#include "logger/Logger.hpp"
#include "database/MediaDirectory.hpp"

#include "common/DirectoryValidator.hpp"
#include "LmsApplication.hpp"

#include "SettingsMediaDirectoryFormView.hpp"

namespace UserInterface {
namespace Settings {

// TODO validate if directory already exists
class MediaDirectoryFormModel : public Wt::WFormModel
{
	public:

		// Associate each field with a unique string literal.
		static const Field PathField;
		static const Field TypeField;

		MediaDirectoryFormModel(Wt::WObject *parent = 0)
			: Wt::WFormModel(parent)
		{
			initializeModels();

			addField(PathField);
			addField(TypeField);

			DirectoryValidator* dirValidator = new DirectoryValidator();
			dirValidator->setMandatory(true);
			setValidator(PathField, dirValidator);
			setValidator(TypeField, new Wt::WValidator(true)); // mandatory

		}

		Wt::WAbstractItemModel *typeModel() { return _typeModel; }

		bool saveData(Wt::WString& error)
		{
			try {
				Wt::Dbo::Transaction transaction(DboSession());

				Database::MediaDirectory::Type type
					= (valueText(TypeField) == "Audio") ? Database::MediaDirectory::Audio : Database::MediaDirectory::Video;

				if (Database::MediaDirectory::get(DboSession(), valueText(PathField).toUTF8(), type))
				{
					error = "This Path/Type already exists!";
					return false;
				}

				Database::MediaDirectory::create(DboSession(), valueText(PathField).toUTF8(), type);

			}
			catch(Wt::Dbo::Exception& exception)
			{
				LMS_LOG(MOD_UI, SEV_ERROR) << "Dbo exception: " << exception.what();
				return false;
			}

			return true;
		}

	private:
		void initializeModels()
		{
			_typeModel = new Wt::WStringListModel(this);
			_typeModel->addString("Audio");
			_typeModel->addString("Video");
		}

		std::string		_userId;
		Wt::WStringListModel*	_typeModel;
};

const Wt::WFormModel::Field MediaDirectoryFormModel::PathField = "path";
const Wt::WFormModel::Field MediaDirectoryFormModel::TypeField = "type";

MediaDirectoryFormView::MediaDirectoryFormView(Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{

	_model = new MediaDirectoryFormModel(this);

	setTemplateText(tr("mediaDirectoryForm-template"));
	addFunction("id", &WTemplate::Functions::id);
	addFunction("block", &WTemplate::Functions::id);

	_applyInfo = new Wt::WText();
	_applyInfo->setInline(false);
	_applyInfo->hide();
	bindWidget("apply-info", _applyInfo);

	// Path
	Wt::WLineEdit* pathEdit = new Wt::WLineEdit();
	setFormWidget(MediaDirectoryFormModel::PathField, pathEdit);
	pathEdit->changed().connect(_applyInfo, &Wt::WWidget::hide);

	// Type
	Wt::WComboBox *typeCB = new Wt::WComboBox();
	setFormWidget(MediaDirectoryFormModel::TypeField, typeCB);
	typeCB->setModel(_model->typeModel());
	typeCB->changed().connect(_applyInfo, &Wt::WWidget::hide);

	// Title & Buttons
	bindString("title", "Add media folder");

	Wt::WPushButton *saveButton = new Wt::WPushButton("Add");
	saveButton->setStyleClass("btn-primary");
	bindWidget("save-button", saveButton);
	saveButton->clicked().connect(this, &MediaDirectoryFormView::processSave);

	Wt::WPushButton *cancelButton = new Wt::WPushButton("Cancel");
	bindWidget("cancel-button", cancelButton);
	cancelButton->clicked().connect(this, &MediaDirectoryFormView::processCancel);

	updateView(_model);
}

void
MediaDirectoryFormView::processCancel()
{
	// parent widget will delete this widget
	completed().emit(false);
}

void
MediaDirectoryFormView::processSave()
{
	updateModel(_model);

	_applyInfo->show();
	if (_model->validate())
	{
		Wt::WString error;
		// commit model into DB
		if (_model->saveData(error) ) {
			completed().emit(true);
			return;
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
