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

#include <Wt/WString>
#include <Wt/WPushButton>
#include <Wt/WCheckBox>
#include <Wt/WComboBox>
#include <Wt/WMessageBox>

#include <Wt/WFormModel>
#include <Wt/WStringListModel>

#include "logger/Logger.hpp"
#include "database/MediaDirectory.hpp"

#include "common/DirectoryValidator.hpp"

#include "SettingsDatabaseFormView.hpp"

namespace UserInterface {
namespace Settings {

class DatabaseFormModel : public Wt::WFormModel
{
	public:
		// Associate each field with a unique string literal.
		static const Field UpdatePeriodField;
		static const Field UpdateStartTimeField;

		DatabaseFormModel(SessionData& sessionData, Wt::WObject *parent = 0)
			: Wt::WFormModel(parent),
			_sessionData(sessionData)
		{
			initializeModels();

			addField(UpdatePeriodField);
			addField(UpdateStartTimeField);

			setValidator(UpdatePeriodField, 		createUpdatePeriodValidator());
			setValidator(UpdateStartTimeField, 		createStartTimeValidator());

			// populate the model with initial data
			loadData();
		}

		Wt::WAbstractItemModel *updatePeriodModel() { return _updatePeriodModel; }
		Wt::WAbstractItemModel *updateStartTimeModel() { return _updateStartTimeModel; }

		void loadData()
		{
			Wt::Dbo::Transaction transaction(_sessionData.getDatabaseHandler().getSession());

			// Get refresh settings
			::Database::MediaDirectorySettings::pointer settings = ::Database::MediaDirectorySettings::get(_sessionData.getDatabaseHandler().getSession());

			int periodRow = getUpdatePeriodModelRow( settings->getUpdatePeriod() );
			if (periodRow != -1)
				setValue(UpdatePeriodField, updatePeriod(periodRow));

			int startTimeRow = getUpdateStartTimeModelRow( settings->getUpdateStartTime() );
			if (startTimeRow != -1)
				setValue(UpdateStartTimeField, updateStartTime( startTimeRow ) );

		}

		void saveData()
		{
			Wt::Dbo::Session& session( _sessionData.getDatabaseHandler().getSession());
			Wt::Dbo::Transaction transaction(session);

			::Database::MediaDirectorySettings::pointer settings = ::Database::MediaDirectorySettings::get(_sessionData.getDatabaseHandler().getSession() );

			int periodRow = getUpdatePeriodModelRow( boost::any_cast<Wt::WString>(value(UpdatePeriodField)));
			assert(periodRow != -1);
			settings.modify()->setUpdatePeriod( updatePeriodDuration( periodRow ) );

			int startTimeRow = getUpdateStartTimeModelRow( boost::any_cast<Wt::WString>(value(UpdateStartTimeField)));
			assert(startTimeRow != -1);
			settings.modify()->setUpdateStartTime( updateStartTimeDuration( startTimeRow ) );

		}

		bool setImmediateScan(Wt::WString& error)
		{
			try {
				Wt::Dbo::Session& session( _sessionData.getDatabaseHandler().getSession());
				Wt::Dbo::Transaction transaction(session);

				::Database::MediaDirectorySettings::pointer settings = ::Database::MediaDirectorySettings::get(_sessionData.getDatabaseHandler().getSession() );

				settings.modify()->setManualScanRequested( true );
			}
			catch(Wt::Dbo::Exception& exception)
			{
				LMS_LOG(MOD_UI, SEV_ERROR) << "Dbo exception: " << exception.what();
				return false;
			}

			return true;
		}

		int getUpdatePeriodModelRow(Wt::WString value)
		{
			for (int i = 0; i < _updatePeriodModel->rowCount(); ++i)
			{
				if (updatePeriod(i) == value)
					return i;
			}
			return -1;
		}

		int getUpdatePeriodModelRow(Database::MediaDirectorySettings::UpdatePeriod duration)
		{
			for (int i = 0; i < _updatePeriodModel->rowCount(); ++i)
			{
				if (updatePeriodDuration(i) == duration)
					return i;
			}

			return -1;
		}

		Database::MediaDirectorySettings::UpdatePeriod updatePeriodDuration(int row) {
			return boost::any_cast<Database::MediaDirectorySettings::UpdatePeriod>
				(_updatePeriodModel->data(_updatePeriodModel->index(row, 0), Wt::UserRole));
		}

		Wt::WString updatePeriod(int row) {
			return boost::any_cast<Wt::WString>
				(_updatePeriodModel->data(_updatePeriodModel->index(row, 0), Wt::DisplayRole));
		}


		int getUpdateStartTimeModelRow(Wt::WString value)
		{
			for (int i = 0; i < _updateStartTimeModel->rowCount(); ++i)
			{
				if (updateStartTime(i) == value)
					return i;
			}

			return -1;
		}

		int getUpdateStartTimeModelRow(boost::posix_time::time_duration duration)
		{
			for (int i = 0; i < _updateStartTimeModel->rowCount(); ++i)
			{
				if (updateStartTimeDuration(i) == duration)
					return i;
			}

			return -1;
		}

		boost::posix_time::time_duration updateStartTimeDuration(int row) {
			return boost::any_cast<boost::posix_time::time_duration>
				(_updateStartTimeModel->data(_updateStartTimeModel->index(row, 0), Wt::UserRole));
		}

		Wt::WString updateStartTime(int row) {
			return boost::any_cast<Wt::WString>
				(_updateStartTimeModel->data(_updateStartTimeModel->index(row, 0), Wt::DisplayRole));
		}


	private:

		void initializeModels() {

			_updatePeriodModel = new Wt::WStringListModel(this);

			_updatePeriodModel->addString("Never");
			_updatePeriodModel->setData(0, 0, Database::MediaDirectorySettings::Never, Wt::UserRole);

			_updatePeriodModel->addString("Daily");
			_updatePeriodModel->setData(1, 0, Database::MediaDirectorySettings::Daily, Wt::UserRole);

			_updatePeriodModel->addString("Weekly");
			_updatePeriodModel->setData(2, 0, Database::MediaDirectorySettings::Weekly, Wt::UserRole);

			_updatePeriodModel->addString("Monthly");
			_updatePeriodModel->setData(3, 0, Database::MediaDirectorySettings::Monthly, Wt::UserRole);

			_updateStartTimeModel = new Wt::WStringListModel(this);

			for (std::size_t i = 0; i < 24; ++i)
			{
				boost::posix_time::time_duration dur = boost::posix_time::hours(i);

				boost::posix_time::time_facet* facet = new boost::posix_time::time_facet("%H:%M");
				facet->time_duration_format("%H:%M");

				std::ostringstream oss;
				oss.imbue(std::locale(oss.getloc(), facet));

				oss << dur;

				_updateStartTimeModel->addString( oss.str() );
				_updateStartTimeModel->setData(i, 0, dur, Wt::UserRole);
			}

		}

		Wt::WValidator *createUpdatePeriodValidator() {
			Wt::WValidator* v = new Wt::WValidator();
			v->setMandatory(true);
			return v;
		}

		Wt::WValidator *createStartTimeValidator() {
			Wt::WValidator* v = new Wt::WValidator();
			v->setMandatory(true);
			return v;
		}


		SessionData&		_sessionData;
		Wt::WStringListModel*	_updatePeriodModel;
		Wt::WStringListModel*	_updateStartTimeModel;

};

const Wt::WFormModel::Field DatabaseFormModel::UpdatePeriodField		= "update-period";
const Wt::WFormModel::Field DatabaseFormModel::UpdateStartTimeField		= "update-start-time";


DatabaseFormView::DatabaseFormView(SessionData& sessionData, Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{
	_model = new DatabaseFormModel(sessionData, this);

	setTemplateText(tr("databaseForm-template"));
	addFunction("id", &WTemplate::Functions::id);
	addFunction("block", &WTemplate::Functions::id);

	_applyInfo = new Wt::WText();
	_applyInfo->setInline(false);
	_applyInfo->hide();
	bindWidget("apply-info", _applyInfo);

	// Update Period
	Wt::WComboBox *updatePeriodCB = new Wt::WComboBox();
	setFormWidget(DatabaseFormModel::UpdatePeriodField, updatePeriodCB);
	updatePeriodCB->setModel(_model->updatePeriodModel());
	updatePeriodCB->changed().connect(_applyInfo, &Wt::WWidget::hide);

	// Update Start Time
	Wt::WComboBox *updateStartTimeCB = new Wt::WComboBox();
	setFormWidget(DatabaseFormModel::UpdateStartTimeField, updateStartTimeCB);
	updateStartTimeCB->setModel(_model->updateStartTimeModel());
	updateStartTimeCB->changed().connect(_applyInfo, &Wt::WWidget::hide);

	// Title & Buttons
	bindString("title", "Media folder settings");

	Wt::WPushButton *saveButton = new Wt::WPushButton("Apply");
	bindWidget("apply-button", saveButton);
	saveButton->setStyleClass("btn-primary");

	Wt::WPushButton *discardButton = new Wt::WPushButton("Discard");
	bindWidget("discard-button", discardButton);

	saveButton->clicked().connect(this, &DatabaseFormView::processSave);
	discardButton->clicked().connect(this, &DatabaseFormView::processDiscard);

	Wt::WPushButton *immediateScanButton = new Wt::WPushButton("Immediate scan");
	immediateScanButton->setStyleClass("btn-warning");
	bindWidget("immediate-scan-button", immediateScanButton);
	immediateScanButton->clicked().connect(this, &DatabaseFormView::processImmediateScan);

	updateView(_model);

}

void
DatabaseFormView::processImmediateScan()
{
	Wt::WString error;
	_applyInfo->show();

	if (_model->setImmediateScan(error))
	{
		_applyInfo->setText( Wt::WString::fromUTF8("Media folder scan has been started!" ) );
		_applyInfo->setStyleClass("alert alert-warning");

		_sigChanged.emit();
	}
	else
	{
		_applyInfo->setText( error );
		_applyInfo->setStyleClass("alert alert-danger");
	}
}

void
DatabaseFormView::processDiscard()
{
	_applyInfo->show();

	_applyInfo->setText( Wt::WString::fromUTF8("Parameters reverted!"));
	_applyInfo->setStyleClass("alert alert-info");

	_model->loadData();

	_model->validate();
	updateView(_model);
}

void
DatabaseFormView::processSave()
{
	updateModel(_model);

	_applyInfo->show();

	if (_model->validate()) {
		// Make the model to commit data into DB
		_model->saveData();

		_sigChanged.emit();

		_applyInfo->setText( Wt::WString::fromUTF8("New parameters successfully applied!"));
		_applyInfo->setStyleClass("alert alert-success");
	}
	else {
		_applyInfo->setText( Wt::WString::fromUTF8("Cannot apply new parameters!"));
		_applyInfo->setStyleClass("alert alert-danger");
	}
	// Udate the view: Delete any validation message in the view, etc.
	updateView(_model);
}


} // namespace Settings
} // namespace UserInterface


