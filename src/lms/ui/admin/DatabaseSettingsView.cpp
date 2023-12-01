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

#include "database/Cluster.hpp"
#include "database/ScanSettings.hpp"
#include "database/Session.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "services/scanner/IScannerService.hpp"
#include "utils/ILogger.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"

#include "common/DirectoryValidator.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/UppercaseValidator.hpp"
#include "common/ValueStringModel.hpp"
#include "ScannerController.hpp"
#include "LmsApplication.hpp"

namespace UserInterface 
{
    using namespace Database;

    class DatabaseSettingsModel : public Wt::WFormModel
    {
    public:
        // Associate each field with a unique string literal.
        static inline constexpr Field MediaDirectoryField{ "media-directory" };
        static inline constexpr Field UpdatePeriodField{ "update-period" };
        static inline constexpr Field UpdateStartTimeField{ "update-start-time" };
        static inline constexpr Field SimilarityEngineTypeField{ "similarity-engine-type" };
        static inline constexpr Field ExtraTagsField{ "extra-tags-to-scan" };

        using UpdatePeriodModel = ValueStringModel<ScanSettings::UpdatePeriod>;

        static inline constexpr std::string_view extraTagsDelimiter{ ";" };

        DatabaseSettingsModel()
        {
            initializeModels();

            addField(MediaDirectoryField);
            addField(UpdatePeriodField);
            addField(UpdateStartTimeField);
            addField(SimilarityEngineTypeField);
            addField(ExtraTagsField);

            auto dirValidator{ createDirectoryValidator() };
            dirValidator->setMandatory(true);
            setValidator(MediaDirectoryField, std::move(dirValidator));

            setValidator(UpdatePeriodField, createMandatoryValidator());
            setValidator(UpdateStartTimeField, createMandatoryValidator());
            setValidator(SimilarityEngineTypeField, createMandatoryValidator());
            setValidator(ExtraTagsField, createUppercaseValidator());

            // populate the model with initial data
            loadData();
        }

        std::shared_ptr<UpdatePeriodModel> updatePeriodModel() { return _updatePeriodModel; }
        std::shared_ptr<Wt::WAbstractItemModel> updateStartTimeModel() { return _updateStartTimeModel; }
        std::shared_ptr<Wt::WAbstractItemModel> similarityEngineTypeModel() { return _similarityEngineTypeModel; }

        void loadData()
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const ScanSettings::pointer scanSettings{ ScanSettings::get(LmsApp->getDbSession()) };

            setValue(MediaDirectoryField, scanSettings->getMediaDirectory().string());

            auto periodRow{ _updatePeriodModel->getRowFromValue(scanSettings->getUpdatePeriod()) };
            if (periodRow)
                setValue(UpdatePeriodField, _updatePeriodModel->getString(*periodRow));

            auto startTimeRow{ _updateStartTimeModel->getRowFromValue(scanSettings->getUpdateStartTime()) };
            if (startTimeRow)
                setValue(UpdateStartTimeField, _updateStartTimeModel->getString(*startTimeRow));

            if (scanSettings->getUpdatePeriod() == ScanSettings::UpdatePeriod::Hourly
                || scanSettings->getUpdatePeriod() == ScanSettings::UpdatePeriod::Never)
            {
                setReadOnly(DatabaseSettingsModel::UpdateStartTimeField, true);
            }

            auto similarityEngineTypeRow{ _similarityEngineTypeModel->getRowFromValue(scanSettings->getSimilarityEngineType()) };
            if (similarityEngineTypeRow)
                setValue(SimilarityEngineTypeField, _similarityEngineTypeModel->getString(*similarityEngineTypeRow));

            auto extraTags{ scanSettings->getExtraTagsToScan() };
            setValue(ExtraTagsField, StringUtils::joinStrings(scanSettings->getExtraTagsToScan(), extraTagsDelimiter));
        }

        void saveData()
        {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

            ScanSettings::pointer scanSettings{ ScanSettings::get(LmsApp->getDbSession()) };

            scanSettings.modify()->setMediaDirectory(valueText(MediaDirectoryField).toUTF8());

            auto updatePeriodRow{ _updatePeriodModel->getRowFromString(valueText(UpdatePeriodField)) };
            if (updatePeriodRow)
                scanSettings.modify()->setUpdatePeriod(_updatePeriodModel->getValue(*updatePeriodRow));

            auto startTimeRow{ _updateStartTimeModel->getRowFromString(valueText(UpdateStartTimeField)) };
            if (startTimeRow)
                scanSettings.modify()->setUpdateStartTime(_updateStartTimeModel->getValue(*startTimeRow));

            auto similarityEngineTypeRow{ _similarityEngineTypeModel->getRowFromString(valueText(SimilarityEngineTypeField)) };
            if (similarityEngineTypeRow)
                scanSettings.modify()->setSimilarityEngineType(_similarityEngineTypeModel->getValue(*similarityEngineTypeRow));

            scanSettings.modify()->setExtraTagsToScan(StringUtils::splitString(valueText(ExtraTagsField).toUTF8(), extraTagsDelimiter));
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
                Wt::WTime time{ static_cast<int>(i), 0 };
                _updateStartTimeModel->add(time.toString(), time);
            }

            _similarityEngineTypeModel = std::make_shared<ValueStringModel<ScanSettings::SimilarityEngineType>>();
            _similarityEngineTypeModel->add(Wt::WString::tr("Lms.Admin.Database.similarity-engine-type.clusters"), ScanSettings::SimilarityEngineType::Clusters);
            _similarityEngineTypeModel->add(Wt::WString::tr("Lms.Admin.Database.similarity-engine-type.none"), ScanSettings::SimilarityEngineType::None);
        }

        std::shared_ptr<UpdatePeriodModel>											_updatePeriodModel;
        std::shared_ptr<ValueStringModel<Wt::WTime>>								_updateStartTimeModel;
        std::shared_ptr<ValueStringModel<ScanSettings::SimilarityEngineType>>	_similarityEngineTypeModel;
    };

    DatabaseSettingsView::DatabaseSettingsView()
    {
        wApp->internalPathChanged().connect(this, [this]
            {
                refreshView();
            });

        refreshView();
    }

    void DatabaseSettingsView::refreshView()
    {
        if (!wApp->internalPathMatches("/admin/database"))
            return;

        clear();

        auto t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.Database.template")) };
        auto model{ std::make_shared<DatabaseSettingsModel>() };

        // Media Directory
        t->setFormWidget(DatabaseSettingsModel::MediaDirectoryField, std::make_unique<Wt::WLineEdit>());

        // Update Period
        auto updatePeriod{ std::make_unique<Wt::WComboBox>() };
        updatePeriod->setModel(model->updatePeriodModel());
        updatePeriod->activated().connect([=](int row)
            {
                const ScanSettings::UpdatePeriod period{ model->updatePeriodModel()->getValue(row) };
                model->setReadOnly(DatabaseSettingsModel::UpdateStartTimeField, period == ScanSettings::UpdatePeriod::Hourly || period == ScanSettings::UpdatePeriod::Never);
                t->updateModel(model.get());
                t->updateView(model.get());
            });
        t->setFormWidget(DatabaseSettingsModel::UpdatePeriodField, std::move(updatePeriod));

        // Update Start Time
        auto updateStartTime{ std::make_unique<Wt::WComboBox>() };
        updateStartTime->setModel(model->updateStartTimeModel());
        t->setFormWidget(DatabaseSettingsModel::UpdateStartTimeField, std::move(updateStartTime));

        // Similarity engine type
        auto similarityEngineType{ std::make_unique<Wt::WComboBox>() };
        similarityEngineType->setModel(model->similarityEngineTypeModel());
        t->setFormWidget(DatabaseSettingsModel::SimilarityEngineTypeField, std::move(similarityEngineType));

        // Clusters
        t->setFormWidget(DatabaseSettingsModel::ExtraTagsField, std::make_unique<Wt::WLineEdit>());

        // Buttons
        Wt::WPushButton* saveBtn = t->bindWidget("apply-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.apply")));
        Wt::WPushButton* discardBtn = t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard")));
        Wt::WPushButton* immScanBtn = t->bindWidget("immediate-scan-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.Admin.Database.immediate-scan")));

        t->bindNew<ScannerController>("scanner-controller");

        saveBtn->clicked().connect([=]
            {
                t->updateModel(model.get());

                if (model->validate())
                {
                    model->saveData();

                    Service<Recommendation::IRecommendationService>::get()->load();
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
