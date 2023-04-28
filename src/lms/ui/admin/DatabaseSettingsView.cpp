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

#include <Wt/WComboBox.h>
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WTemplateFormView.h>

#include "services/database/Cluster.hpp"
#include "services/database/ScanSettings.hpp"
#include "services/database/Session.hpp"
#include "services/scanner/IScannerService.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"

#include "common/DirectoryValidator.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/ValueStringModel.hpp"
#include "ScannerController.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

using namespace Database;

class DatabaseSettingsModel : public Wt::WFormModel
{
	public:
		// Associate each field with a unique string literal.
		static inline constexpr Field MediaDirectoryField {"media-directory"};
		static inline constexpr Field UpdatePeriodField {"update-period"};
		static inline constexpr Field UpdateStartTimeField {"update-start-time"};
		static inline constexpr Field RecommendationEngineTypeField {"recommendation-engine-type"};
		static inline constexpr Field ClustersField {"clusters"};

		using UpdatePeriodModel = ValueStringModel<ScanSettings::UpdatePeriod>;

		DatabaseSettingsModel()
		{
			initializeModels();

			addField(MediaDirectoryField);
			addField(UpdatePeriodField);
			addField(UpdateStartTimeField);
			addField(RecommendationEngineTypeField);
			addField(ClustersField);

			auto dirValidator {createDirectoryValidator()};
			dirValidator->setMandatory(true);
			setValidator(MediaDirectoryField, std::move(dirValidator));

			setValidator(UpdatePeriodField, createMandatoryValidator());
			setValidator(UpdateStartTimeField, createMandatoryValidator());
			setValidator(RecommendationEngineTypeField, createMandatoryValidator());

			// populate the model with initial data
			loadData();
		}

		std::shared_ptr<UpdatePeriodModel> updatePeriodModel() { return _updatePeriodModel; }
		std::shared_ptr<Wt::WAbstractItemModel> updateStartTimeModel() { return _updateStartTimeModel; }
		std::shared_ptr<Wt::WAbstractItemModel> recommendationEngineTypeModel() { return _recommendationEngineTypeModel; }

		void loadData()
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const ScanSettings::pointer scanSettings {ScanSettings::get(LmsApp->getDbSession())};

			setValue(MediaDirectoryField, scanSettings->getMediaDirectory().string());

			auto periodRow {_updatePeriodModel->getRowFromValue(scanSettings->getUpdatePeriod())};
			if (periodRow)
				setValue(UpdatePeriodField, _updatePeriodModel->getString(*periodRow));

			auto startTimeRow {_updateStartTimeModel->getRowFromValue(scanSettings->getUpdateStartTime())};
			if (startTimeRow)
				setValue(UpdateStartTimeField, _updateStartTimeModel->getString(*startTimeRow));

			if (scanSettings->getUpdatePeriod() == ScanSettings::UpdatePeriod::Hourly
					|| scanSettings->getUpdatePeriod() == ScanSettings::UpdatePeriod::Never)
			{
				setReadOnly(DatabaseSettingsModel::UpdateStartTimeField, true);
			}

			auto recommendationEngineTypeRow {_recommendationEngineTypeModel->getRowFromValue(scanSettings->getRecommendationEngineType())};
			if (recommendationEngineTypeRow)
				setValue(RecommendationEngineTypeField, _recommendationEngineTypeModel->getString(*recommendationEngineTypeRow));

			auto clusterTypes {scanSettings->getClusterTypes()};
			if (!clusterTypes.empty())
			{
				std::vector<std::string> names;
				std::transform(clusterTypes.begin(), clusterTypes.end(), std::back_inserter(names),  [](auto clusterType) { return clusterType->getName(); });
				setValue(ClustersField, StringUtils::joinStrings(names, " "));
			}
		}

		void saveData()
		{
			auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

			ScanSettings::pointer scanSettings {ScanSettings::get(LmsApp->getDbSession())};

			scanSettings.modify()->setMediaDirectory(valueText(MediaDirectoryField).toUTF8());

			auto updatePeriodRow {_updatePeriodModel->getRowFromString(valueText(UpdatePeriodField))};
			if (updatePeriodRow)
				scanSettings.modify()->setUpdatePeriod(_updatePeriodModel->getValue(*updatePeriodRow));

			auto startTimeRow {_updateStartTimeModel->getRowFromString(valueText(UpdateStartTimeField))};
			if (startTimeRow)
				scanSettings.modify()->setUpdateStartTime(_updateStartTimeModel->getValue(*startTimeRow));

			auto recommendationEngineTypeRow {_recommendationEngineTypeModel->getRowFromString(valueText(RecommendationEngineTypeField))};
			if (recommendationEngineTypeRow)
				scanSettings.modify()->setRecommendationEngineType(_recommendationEngineTypeModel->getValue(*recommendationEngineTypeRow));

			auto clusterTypes {StringUtils::splitStringCopy(valueText(ClustersField).toUTF8(), " ")};
			scanSettings.modify()->setClusterTypes(LmsApp->getDbSession(), std::set<std::string>(clusterTypes.begin(), clusterTypes.end()));
		}

		static ScanSettings::RecommendationEngineType getCurrentRecommendationEngine()
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};
			const ScanSettings::pointer scanSettings {ScanSettings::get(LmsApp->getDbSession())};
			return scanSettings->getRecommendationEngineType();
		}

	private:
		void initializeModels()
		{
			_updatePeriodModel = std::make_shared<ValueStringModel<ScanSettings::UpdatePeriod>>();
			_updatePeriodModel->add(Wt::WString::tr("Lms.Admin.Database.never"), ScanSettings::UpdatePeriod::Never);
			_updatePeriodModel->add(Wt::WString::tr("Lms.Admin.Database.hourly"), ScanSettings::UpdatePeriod::Hourly);
			_updatePeriodModel->add(Wt::WString::tr("Lms.Admin.Database.daily"), ScanSettings::UpdatePeriod::Daily);
			_updatePeriodModel->add(Wt::WString::tr("Lms.Admin.Database.weekly"), ScanSettings::UpdatePeriod::Weekly);
			_updatePeriodModel->add(Wt::WString::tr("Lms.Admin.Database.monthly"), ScanSettings::UpdatePeriod::Monthly);

			_updateStartTimeModel = std::make_shared<ValueStringModel<Wt::WTime>>();
			for (std::size_t i = 0; i < 24; ++i)
			{
				Wt::WTime time {static_cast<int>(i), 0};
				_updateStartTimeModel->add(time.toString(), time);
			}

			_recommendationEngineTypeModel = std::make_shared<ValueStringModel<ScanSettings::RecommendationEngineType>>();
			_recommendationEngineTypeModel->add(Wt::WString::tr("Lms.Admin.Database.recommendation-engine-type.clusters"), ScanSettings::RecommendationEngineType::Clusters);
			_recommendationEngineTypeModel->add(Wt::WString::tr("Lms.Admin.Database.recommendation-engine-type.features"), ScanSettings::RecommendationEngineType::Features);
		}

		std::shared_ptr<UpdatePeriodModel>											_updatePeriodModel;
		std::shared_ptr<ValueStringModel<Wt::WTime>>								_updateStartTimeModel;
		std::shared_ptr<ValueStringModel<ScanSettings::RecommendationEngineType>>	_recommendationEngineTypeModel;
};

DatabaseSettingsView::DatabaseSettingsView()
{
	wApp->internalPathChanged().connect(this, [this]
	{
		refreshView();
	});

	refreshView();
}

void
DatabaseSettingsView::refreshView()
{
	if (!wApp->internalPathMatches("/admin/database"))
		return;

	clear();

	auto t {addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.Database.template"))};
	auto model {std::make_shared<DatabaseSettingsModel>()};

	// Media Directory
	t->setFormWidget(DatabaseSettingsModel::MediaDirectoryField, std::make_unique<Wt::WLineEdit>());

	// Update Period
	auto updatePeriod {std::make_unique<Wt::WComboBox>()};
	updatePeriod->setModel(model->updatePeriodModel());
	updatePeriod->activated().connect([=](int row)
	{
		const ScanSettings::UpdatePeriod period {model->updatePeriodModel()->getValue(row)};
		model->setReadOnly(DatabaseSettingsModel::UpdateStartTimeField, period == ScanSettings::UpdatePeriod::Hourly || period == ScanSettings::UpdatePeriod::Never);
		t->updateModel(model.get());
		t->updateView(model.get());
	});
	t->setFormWidget(DatabaseSettingsModel::UpdatePeriodField, std::move(updatePeriod));

	// Update Start Time
	auto updateStartTime {std::make_unique<Wt::WComboBox>()};
	updateStartTime->setModel(model->updateStartTimeModel());
	t->setFormWidget(DatabaseSettingsModel::UpdateStartTimeField, std::move(updateStartTime));

	// recommendation engine type
	// Hide the settings if the engine is set to clusters, as we don't want users to switch to acoustic features (currently broken)
	// Otherwise, give the user a way to switch back to clusters
	t->setCondition("if-has-recommendation-engine", model->getCurrentRecommendationEngine() == ScanSettings::RecommendationEngineType::Features);
	auto recommendationEngineType {std::make_unique<Wt::WComboBox>()};
	recommendationEngineType->setModel(model->recommendationEngineTypeModel());
	t->setFormWidget(DatabaseSettingsModel::RecommendationEngineTypeField, std::move(recommendationEngineType));

	// Clusters
	t->setFormWidget(DatabaseSettingsModel::ClustersField, std::make_unique<Wt::WLineEdit>());

	// Buttons
	Wt::WPushButton *saveBtn = t->bindWidget("apply-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.apply")));
	Wt::WPushButton *discardBtn = t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard")));
	Wt::WPushButton *immScanBtn = t->bindWidget("immediate-scan-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.Admin.Database.immediate-scan")));

	t->bindNew<ScannerController>("scanner-controller");

	saveBtn->clicked().connect([=]
	{
		t->updateModel(model.get());

		if (model->validate())
		{
			model->saveData();

			Service<Scanner::IScannerService>::get()->requestImmediateScan(false);
			LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Admin.Database.database"), Wt::WString::tr("Lms.Admin.Database.settings-saved"));
		}

		// Udate the view: Delete any validation message in the view, etc.
		t->updateView(model.get());
	});

	discardBtn->clicked().connect([=]
	{
		model->loadData();
		model->validate();
		t->updateView(model.get());
	});

	immScanBtn->clicked().connect([=]
	{
		Service<Scanner::IScannerService>::get()->requestImmediateScan(false);
	});

	t->updateView(model.get());
}

} // namespace UserInterface


