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

#include <Wt/WString.h>
#include <Wt/WPushButton.h>
#include <Wt/WComboBox.h>
#include <Wt/WLineEdit.h>

#include <Wt/WFormModel.h>
#include <Wt/WStringListModel.h>

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

		DatabaseSettingsModel()
			: Wt::WFormModel()
		{
			initializeModels();

			addField(MediaDirectoryField);
			addField(UpdatePeriodField);
			addField(UpdateStartTimeField);

			auto dirValidator = std::make_shared<DirectoryValidator>();
			dirValidator->setMandatory(true);
			setValidator(MediaDirectoryField, dirValidator);

			setValidator(UpdatePeriodField, createMandatoryValidator());
			setValidator(UpdateStartTimeField, createMandatoryValidator());

			// populate the model with initial data
			loadData();
		}

		std::shared_ptr<Wt::WAbstractItemModel> updatePeriodModel() { return _updatePeriodModel; }
		std::shared_ptr<Wt::WAbstractItemModel> updateStartTimeModel() { return _updateStartTimeModel; }

		void loadData()
		{
			using namespace Database;

			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			std::vector<MediaDirectory::pointer> mediaDirectories = MediaDirectory::getAll(LmsApp->getDboSession());
			if (!mediaDirectories.empty())
				setValue(MediaDirectoryField, mediaDirectories.front()->getPath().string());

			auto periodRow = getUpdatePeriodModelRow( Scanner::getUpdatePeriod(LmsApp->getDboSession()) );
			if (periodRow)
				setValue(UpdatePeriodField, updatePeriodString(*periodRow));

			auto startTimeRow = getUpdateStartTimeModelRow( Scanner::getUpdateStartTime(LmsApp->getDboSession()) );
			if (startTimeRow)
				setValue(UpdateStartTimeField, updateStartTimeString(*startTimeRow) );
		}

		void saveData()
		{
			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			MediaDirectory::eraseAll(LmsApp->getDboSession());
			MediaDirectory::create(LmsApp->getDboSession(), Wt::cpp17::any_cast<Wt::WString>(value(MediaDirectoryField)).toUTF8());

			auto updatePeriodRow = getUpdatePeriodModelRow( Wt::cpp17::any_cast<Wt::WString>(value(UpdatePeriodField)));
			assert(updatePeriodRow);
			Scanner::setUpdatePeriod(LmsApp->getDboSession(), updatePeriod(*updatePeriodRow));

			auto startTimeRow = getUpdateStartTimeModelRow( Wt::cpp17::any_cast<Wt::WString>(value(UpdateStartTimeField)));
			assert(startTimeRow);
			Scanner::setUpdateStartTime(LmsApp->getDboSession(), updateStartTime(*startTimeRow));
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
			return Wt::cpp17::any_cast<Scanner::UpdatePeriod>
				(_updatePeriodModel->data(_updatePeriodModel->index(row, 0), Wt::ItemDataRole::User));
		}

		Wt::WString updatePeriodString(int row)
		{
			return Wt::cpp17::any_cast<Wt::WString>
				(_updatePeriodModel->data(_updatePeriodModel->index(row, 0), Wt::ItemDataRole::Display));
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

		boost::optional<int> getUpdateStartTimeModelRow(Wt::WTime startTime)
		{
			for (int i = 0; i < _updateStartTimeModel->rowCount(); ++i)
			{
				if (updateStartTime(i) == startTime)
					return i;
			}

			return boost::none;
		}

		Wt::WTime updateStartTime(int row)
		{
			return Wt::cpp17::any_cast<Wt::WTime>
				(_updateStartTimeModel->data(_updateStartTimeModel->index(row, 0), Wt::ItemDataRole::User));
		}

		Wt::WString updateStartTimeString(int row)
		{
			return Wt::cpp17::any_cast<Wt::WString>
				(_updateStartTimeModel->data(_updateStartTimeModel->index(row, 0), Wt::ItemDataRole::Display));
		}


	private:

		void initializeModels()
		{

			_updatePeriodModel = std::make_shared<Wt::WStringListModel>();

			_updatePeriodModel->addString(Wt::WString::tr("Lms.Admin.Database.never"));
			_updatePeriodModel->setData(0, 0, Scanner::UpdatePeriod::Never, Wt::ItemDataRole::User);

			_updatePeriodModel->addString(Wt::WString::tr("Lms.Admin.Database.daily"));
			_updatePeriodModel->setData(1, 0, Scanner::UpdatePeriod::Daily, Wt::ItemDataRole::User);

			_updatePeriodModel->addString(Wt::WString::tr("Lms.Admin.Database.weekly"));
			_updatePeriodModel->setData(2, 0, Scanner::UpdatePeriod::Weekly, Wt::ItemDataRole::User);

			_updatePeriodModel->addString(Wt::WString::tr("Lms.Admin.Database.monthly"));
			_updatePeriodModel->setData(3, 0, Scanner::UpdatePeriod::Monthly, Wt::ItemDataRole::User);

			_updateStartTimeModel = std::make_shared<Wt::WStringListModel>();

			for (std::size_t i = 0; i < 24; ++i)
			{
				Wt::WTime time(i, 0);

				_updateStartTimeModel->addString( time.toString() );
				_updateStartTimeModel->setData(i, 0, time, Wt::ItemDataRole::User);
			}

		}


		std::shared_ptr<Wt::WStringListModel>	_updatePeriodModel;
		std::shared_ptr<Wt::WStringListModel>	_updateStartTimeModel;

};

const Wt::WFormModel::Field DatabaseSettingsModel::MediaDirectoryField		= "media-directory";
const Wt::WFormModel::Field DatabaseSettingsModel::UpdatePeriodField		= "update-period";
const Wt::WFormModel::Field DatabaseSettingsModel::UpdateStartTimeField		= "update-start-time";


DatabaseSettingsView::DatabaseSettingsView()
: Wt::WTemplateFormView(Wt::WString::tr("Lms.Admin.Database.template"))
{
	auto model = std::make_shared<DatabaseSettingsModel>();

	// Media Directory
	setFormWidget(DatabaseSettingsModel::MediaDirectoryField, std::make_unique<Wt::WLineEdit>());

	// Update Period
	auto updatePeriod = std::make_unique<Wt::WComboBox>();
	updatePeriod->setModel(model->updatePeriodModel());
	setFormWidget(DatabaseSettingsModel::UpdatePeriodField, std::move(updatePeriod));

	// Update Start Time
	auto updateStartTime = std::make_unique<Wt::WComboBox>();
	updateStartTime->setModel(model->updateStartTimeModel());
	setFormWidget(DatabaseSettingsModel::UpdateStartTimeField, std::move(updateStartTime));

	// Buttons

	Wt::WPushButton *saveBtn = bindWidget("apply-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.apply")));
	Wt::WPushButton *discardBtn = bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard")));
	Wt::WPushButton *immScanBtn = bindWidget("immediate-scan-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.Admin.Database.immediate-scan")));

	saveBtn->clicked().connect(std::bind([=] ()
	{
		updateModel(model.get());

		if (model->validate())
		{
			model->saveData();

			LmsApp->getMediaScanner().reschedule();
			LmsApp->notifyMsg(Wt::WString::tr("Lms.Admin.Database.settings-saved"));
		}

		// Udate the view: Delete any validation message in the view, etc.
		updateView(model.get());
	}));

	discardBtn->clicked().connect(std::bind([=] ()
	{
		model->loadData();
		model->validate();
		updateView(model.get());
	}));

	immScanBtn->clicked().connect(std::bind([=] ()
	{
		LmsApp->getMediaScanner().scheduleImmediateScan();
		LmsApp->notifyMsg(Wt::WString::tr("Lms.Admin.Database.scan-launched"));
	}));

	updateView(model.get());
}

} // namespace UserInterface


