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
#include <Wt/WMessageBox>
#include <Wt/WLineEdit>

#include <Wt/WFormModel>
#include <Wt/WStringListModel>

#include "common/Validators.hpp"
#include "database/MediaDirectory.hpp"
#include "scanner/MediaScanner.hpp"
#include "utils/Logger.hpp"

#include "LmsApplication.hpp"

#include "DatabaseSettingsView.hpp"

namespace UserInterface {

using namespace Database;

class DatabaseSettingsModel : public Wt::WFormModel
{
	public:
		// Associate each field with a unique string literal.
		static const Field MediaDirectoryField;
		static const Field UpdatePeriodField;
		static const Field UpdateStartTimeField;

		DatabaseSettingsModel(Wt::WObject *parent = 0)
			: Wt::WFormModel(parent)
		{
			initializeModels();

			addField(MediaDirectoryField);
			addField(UpdatePeriodField);
			addField(UpdateStartTimeField);

			DirectoryValidator* dirValidator = new DirectoryValidator();
			dirValidator->setMandatory(true);
			setValidator(MediaDirectoryField, dirValidator);

			setValidator(UpdatePeriodField, createMandatoryValidator());
			setValidator(UpdateStartTimeField, createMandatoryValidator());

			// populate the model with initial data
			loadData();
		}

		Wt::WAbstractItemModel *updatePeriodModel() { return _updatePeriodModel; }
		Wt::WAbstractItemModel *updateStartTimeModel() { return _updateStartTimeModel; }

		void loadData()
		{
			using namespace Database;

			Wt::Dbo::Transaction transaction(DboSession());

			std::vector<MediaDirectory::pointer> mediaDirectories = MediaDirectory::getAll(DboSession());
			if (!mediaDirectories.empty())
				setValue(MediaDirectoryField, mediaDirectories.front()->getPath().string());

			auto periodRow = getUpdatePeriodModelRow( Scanner::getUpdatePeriod(DboSession()) );
			if (periodRow)
				setValue(UpdatePeriodField, updatePeriodString(*periodRow));

			auto startTimeRow = getUpdateStartTimeModelRow( Scanner::getUpdateStartTime(DboSession()) );
			if (startTimeRow)
				setValue(UpdateStartTimeField, updateStartTimeString(*startTimeRow) );
		}

		void saveData()
		{
			Wt::Dbo::Transaction transaction(DboSession());

			MediaDirectory::eraseAll(DboSession());
			MediaDirectory::create(DboSession(), boost::any_cast<Wt::WString>(value(MediaDirectoryField)).toUTF8());

			auto updatePeriodRow = getUpdatePeriodModelRow( boost::any_cast<Wt::WString>(value(UpdatePeriodField)));
			assert(updatePeriodRow);
			Scanner::setUpdatePeriod(DboSession(), updatePeriod(*updatePeriodRow));

			auto startTimeRow = getUpdateStartTimeModelRow( boost::any_cast<Wt::WString>(value(UpdateStartTimeField)));
			assert(startTimeRow);
			Scanner::setUpdateStartTime(DboSession(), updateStartTime(*startTimeRow));
		}

		boost::optional<int> getUpdatePeriodModelRow(Wt::WString value)
		{
			for (int i = 0; i < _updatePeriodModel->rowCount(); ++i)
			{
				if (updatePeriodString(i) == value)
					return i;
			}

			return boost::none;
		}

		boost::optional<int> getUpdatePeriodModelRow(Scanner::UpdatePeriod period)
		{
			for (int i = 0; i < _updatePeriodModel->rowCount(); ++i)
			{
				if (updatePeriod(i) == period)
					return i;
			}

			return boost::none;
		}

		Scanner::UpdatePeriod updatePeriod(int row)
		{
			return boost::any_cast<Scanner::UpdatePeriod>
				(_updatePeriodModel->data(_updatePeriodModel->index(row, 0), Wt::UserRole));
		}

		Wt::WString updatePeriodString(int row)
		{
			return boost::any_cast<Wt::WString>
				(_updatePeriodModel->data(_updatePeriodModel->index(row, 0), Wt::DisplayRole));
		}


		boost::optional<int> getUpdateStartTimeModelRow(Wt::WString value)
		{
			for (int i = 0; i < _updateStartTimeModel->rowCount(); ++i)
			{
				if (updateStartTimeString(i) == value)
					return i;
			}

			return boost::none;
		}

		boost::optional<int> getUpdateStartTimeModelRow(boost::posix_time::time_duration startTime)
		{
			for (int i = 0; i < _updateStartTimeModel->rowCount(); ++i)
			{
				if (updateStartTime(i) == startTime)
					return i;
			}

			return boost::none;
		}

		boost::posix_time::time_duration updateStartTime(int row)
		{
			return boost::any_cast<boost::posix_time::time_duration>
				(_updateStartTimeModel->data(_updateStartTimeModel->index(row, 0), Wt::UserRole));
		}

		Wt::WString updateStartTimeString(int row)
		{
			return boost::any_cast<Wt::WString>
				(_updateStartTimeModel->data(_updateStartTimeModel->index(row, 0), Wt::DisplayRole));
		}


	private:

		void initializeModels() {

			_updatePeriodModel = new Wt::WStringListModel(this);

			_updatePeriodModel->addString(Wt::WString::tr("msg-update-period-never"));
			_updatePeriodModel->setData(0, 0, Scanner::UpdatePeriod::Never, Wt::UserRole);

			_updatePeriodModel->addString(Wt::WString::tr("msg-update-period-daily"));
			_updatePeriodModel->setData(1, 0, Scanner::UpdatePeriod::Daily, Wt::UserRole);

			_updatePeriodModel->addString(Wt::WString::tr("msg-update-period-weekly"));
			_updatePeriodModel->setData(2, 0, Scanner::UpdatePeriod::Weekly, Wt::UserRole);

			_updatePeriodModel->addString(Wt::WString::tr("msg-update-period-monthly"));
			_updatePeriodModel->setData(3, 0, Scanner::UpdatePeriod::Monthly, Wt::UserRole);

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


		Wt::WStringListModel*	_updatePeriodModel;
		Wt::WStringListModel*	_updateStartTimeModel;

};

const Wt::WFormModel::Field DatabaseSettingsModel::MediaDirectoryField		= "media-directory";
const Wt::WFormModel::Field DatabaseSettingsModel::UpdatePeriodField		= "update-period";
const Wt::WFormModel::Field DatabaseSettingsModel::UpdateStartTimeField		= "update-start-time";


DatabaseSettingsView::DatabaseSettingsView(Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{
	auto model = new DatabaseSettingsModel(this);

	setTemplateText(tr("template-settings-database"));
	addFunction("tr", &WTemplate::Functions::tr);
	addFunction("id", &WTemplate::Functions::id);

	// Media Directory
	Wt::WLineEdit *mediaDirectoryEdit = new Wt::WLineEdit();
	setFormWidget(DatabaseSettingsModel::MediaDirectoryField, mediaDirectoryEdit);

	// Update Period
	Wt::WComboBox *updatePeriodCB = new Wt::WComboBox();
	setFormWidget(DatabaseSettingsModel::UpdatePeriodField, updatePeriodCB);
	updatePeriodCB->setModel(model->updatePeriodModel());

	// Update Start Time
	Wt::WComboBox *updateStartTimeCB = new Wt::WComboBox();
	setFormWidget(DatabaseSettingsModel::UpdateStartTimeField, updateStartTimeCB);
	updateStartTimeCB->setModel(model->updateStartTimeModel());

	// Buttons

	Wt::WPushButton *saveBtn = new Wt::WPushButton(Wt::WString::tr("msg-btn-apply"));
	bindWidget("apply-btn", saveBtn);

	Wt::WPushButton *discardBtn = new Wt::WPushButton(Wt::WString::tr("msg-btn-discard"));
	bindWidget("discard-btn", discardBtn);

	Wt::WPushButton *immScanBtn = new Wt::WPushButton(Wt::WString::tr("msg-btn-immediate-scan"));
	bindWidget("immediate-scan-btn", immScanBtn);

	saveBtn->clicked().connect(std::bind([=] ()
	{
		updateModel(model);

		if (model->validate())
		{
			model->saveData();

			LmsApp->getMediaScanner().reschedule();
			LmsApp->notify(Wt::WString::tr("msg-notify-settings-saved"));
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

	immScanBtn->clicked().connect(std::bind([=] ()
	{
		LmsApp->getMediaScanner().scheduleImmediateScan();
		LmsApp->notify(Wt::WString::tr("msg-notify-scan-launched"));
	}));

	updateView(model);
}

} // namespace UserInterface


