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

#include "DatabaseSettingsView.hpp"

#include <Wt/WString.h>
#include <Wt/WPushButton.h>
#include <Wt/WComboBox.h>
#include <Wt/WLineEdit.h>
#include <Wt/WTemplateFormView.h>

#include <Wt/WFormModel.h>
#include <Wt/WStringListModel.h>

#include "common/Validators.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"

namespace UserInterface {

using namespace Database;

class DatabaseSettingsModel : public Wt::WFormModel
{
	public:
		// Associate each field with a unique string literal.
		static const Field MediaDirectoryField;
		static const Field UpdatePeriodField;
		static const Field UpdateStartTimeField;
		static const Field TagsField;

		DatabaseSettingsModel()
			: Wt::WFormModel()
		{
			initializeModels();

			addField(MediaDirectoryField);
			addField(UpdatePeriodField);
			addField(UpdateStartTimeField);
			addField(TagsField);

			auto dirValidator = std::make_shared<DirectoryValidator>();
			dirValidator->setMandatory(true);
			setValidator(MediaDirectoryField, dirValidator);

			setValidator(UpdatePeriodField, createMandatoryValidator());
			setValidator(UpdateStartTimeField, createMandatoryValidator());
			setValidator(TagsField, createTagsValidator());

			// populate the model with initial data
			loadData();
		}

		std::shared_ptr<Wt::WAbstractItemModel> updatePeriodModel() { return _updatePeriodModel; }
		std::shared_ptr<Wt::WAbstractItemModel> updateStartTimeModel() { return _updateStartTimeModel; }

		void loadData()
		{
			using namespace Database;

			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			auto scanSettings = ScanSettings::get(LmsApp->getDboSession());

			setValue(MediaDirectoryField, scanSettings->getMediaDirectory());

			auto periodRow = getUpdatePeriodModelRow( scanSettings->getUpdatePeriod() );
			if (periodRow)
				setValue(UpdatePeriodField, updatePeriodString(*periodRow));

			auto startTimeRow = getUpdateStartTimeModelRow( scanSettings->getUpdateStartTime() );
			if (startTimeRow)
				setValue(UpdateStartTimeField, updateStartTimeString(*startTimeRow) );

			auto clusterTypes = scanSettings->getClusterTypes();
			if (!clusterTypes.empty())
			{
				std::vector<std::string> names;
				std::transform(clusterTypes.begin(), clusterTypes.end(),std::back_inserter(names),  [](auto clusterType) { return clusterType->getName(); });
				setValue(TagsField, joinStrings(names, " "));
			}
		}

		void saveData()
		{
			Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

			auto scanSettings = ScanSettings::get(LmsApp->getDboSession());

			scanSettings.modify()->setMediaDirectory(valueText(MediaDirectoryField).toUTF8());

			auto updatePeriodRow = getUpdatePeriodModelRow( valueText(UpdatePeriodField));
			assert(updatePeriodRow);
			scanSettings.modify()->setUpdatePeriod(updatePeriod(*updatePeriodRow));

			auto startTimeRow = getUpdateStartTimeModelRow( valueText(UpdateStartTimeField));
			assert(startTimeRow);
			scanSettings.modify()->setUpdateStartTime(updateStartTime(*startTimeRow));

			auto clusterTypes = splitString(valueText(TagsField).toUTF8(), " ");
			scanSettings.modify()->setClusterTypes(std::set<std::string>(clusterTypes.begin(), clusterTypes.end()));
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

		boost::optional<int> getUpdatePeriodModelRow(ScanSettings::UpdatePeriod period)
		{
			for (int i = 0; i < _updatePeriodModel->rowCount(); ++i)
			{
				if (updatePeriod(i) == period)
					return i;
			}

			return boost::none;
		}

		ScanSettings::UpdatePeriod updatePeriod(int row)
		{
			return Wt::cpp17::any_cast<ScanSettings::UpdatePeriod>
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

		static std::shared_ptr<Wt::WValidator> createTagsValidator()
		{
			auto v = std::make_shared<Wt::WValidator>();
			return v;
		}

		void initializeModels()
		{

			_updatePeriodModel = std::make_shared<Wt::WStringListModel>();

			_updatePeriodModel->addString(Wt::WString::tr("Lms.Admin.Database.never"));
			_updatePeriodModel->setData(0, 0, ScanSettings::UpdatePeriod::Never, Wt::ItemDataRole::User);

			_updatePeriodModel->addString(Wt::WString::tr("Lms.Admin.Database.daily"));
			_updatePeriodModel->setData(1, 0, ScanSettings::UpdatePeriod::Daily, Wt::ItemDataRole::User);

			_updatePeriodModel->addString(Wt::WString::tr("Lms.Admin.Database.weekly"));
			_updatePeriodModel->setData(2, 0, ScanSettings::UpdatePeriod::Weekly, Wt::ItemDataRole::User);

			_updatePeriodModel->addString(Wt::WString::tr("Lms.Admin.Database.monthly"));
			_updatePeriodModel->setData(3, 0, ScanSettings::UpdatePeriod::Monthly, Wt::ItemDataRole::User);

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
const Wt::WFormModel::Field DatabaseSettingsModel::TagsField			= "tags";

DatabaseSettingsView::DatabaseSettingsView()
{
	wApp->internalPathChanged().connect(std::bind([=]
	{
		refreshView();
	}));

	refreshView();
}

void
DatabaseSettingsView::refreshView()
{
	if (!wApp->internalPathMatches("/admin/database"))
		return;

	clear();

	auto t = addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.Database.template"));
	auto model = std::make_shared<DatabaseSettingsModel>();

	// Media Directory
	t->setFormWidget(DatabaseSettingsModel::MediaDirectoryField, std::make_unique<Wt::WLineEdit>());

	// Update Period
	auto updatePeriod = std::make_unique<Wt::WComboBox>();
	updatePeriod->setModel(model->updatePeriodModel());
	t->setFormWidget(DatabaseSettingsModel::UpdatePeriodField, std::move(updatePeriod));

	// Update Start Time
	auto updateStartTime = std::make_unique<Wt::WComboBox>();
	updateStartTime->setModel(model->updateStartTimeModel());
	t->setFormWidget(DatabaseSettingsModel::UpdateStartTimeField, std::move(updateStartTime));

	// Tags
	t->setFormWidget(DatabaseSettingsModel::TagsField, std::make_unique<Wt::WLineEdit>());

	// Buttons
	Wt::WPushButton *saveBtn = t->bindWidget("apply-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.apply")));
	Wt::WPushButton *discardBtn = t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard")));
	Wt::WPushButton *immScanBtn = t->bindWidget("immediate-scan-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.Admin.Database.immediate-scan")));

	saveBtn->clicked().connect(std::bind([=] ()
	{
		t->updateModel(model.get());

		if (model->validate())
		{
			model->saveData();

			LmsApp->getMediaScanner().reschedule();
			LmsApp->notifyMsg(Wt::WString::tr("Lms.Admin.Database.settings-saved"));
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

	immScanBtn->clicked().connect(std::bind([=] ()
	{
		LmsApp->getMediaScanner().scheduleImmediateScan();
		LmsApp->notifyMsg(Wt::WString::tr("Lms.Admin.Database.scan-launched"));
	}));

	t->updateView(model.get());
}

} // namespace UserInterface


