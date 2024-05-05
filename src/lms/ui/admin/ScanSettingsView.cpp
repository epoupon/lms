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

#include "ScanSettingsView.hpp"

#include <Wt/WComboBox.h>
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WTemplateFormView.h>

#include "database/ScanSettings.hpp"
#include "database/Session.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "services/scanner/IScannerService.hpp"
#include "core/ILogger.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"

#include "common/MandatoryValidator.hpp"
#include "common/UppercaseValidator.hpp"
#include "common/ValueStringModel.hpp"
#include "LmsApplication.hpp"

namespace lms::ui
{
    using namespace db;

    namespace
    {
        class TagDelimitersValidator : public Wt::WValidator
        {
        private:
            Wt::WValidator::Result validate(const Wt::WString& input) const override
            {
                if (input.empty())
                    return Wt::WValidator::Result{ Wt::ValidationState::Valid };

                std::string inputStr{ input.toUTF8() };
                if (std::all_of(std::cbegin(inputStr), std::cend(inputStr), [](char c) { return std::isspace(c); }))
                    return Wt::WValidator::Result{ Wt::ValidationState::Invalid, Wt::WString::tr("Lms.Admin.Database.tag-delimiter-must-not-contain-only-spaces") };
                    
                return Wt::WValidator::Result{ Wt::ValidationState::Valid };
            }

            std::string javaScriptValidate() const override { return {}; }
        };

        class DatabaseSettingsModel : public Wt::WFormModel
        {
        public:
            static inline constexpr Field UpdatePeriodField{ "update-period" };
            static inline constexpr Field UpdateStartTimeField{ "update-start-time" };
            static inline constexpr Field SimilarityEngineTypeField{ "similarity-engine-type" };
            static inline constexpr Field ExtraTagsField{ "extra-tags-to-scan" };
            static inline constexpr Field ArtistTagDelimiterField{ "artist-tag-delimiter" };
            static inline constexpr Field DefaultTagDelimiterField{ "default-tag-delimiter" };

            using UpdatePeriodModel = ValueStringModel<ScanSettings::UpdatePeriod>;

            static inline constexpr char extraTagsDelimiter{ ';' };

            DatabaseSettingsModel()
            {
                initializeModels();

                addField(UpdatePeriodField);
                addField(UpdateStartTimeField);
                addField(SimilarityEngineTypeField);
                addField(ExtraTagsField);
                addField(ArtistTagDelimiterField);
                addField(DefaultTagDelimiterField);

                setValidator(UpdatePeriodField, createMandatoryValidator());
                setValidator(UpdateStartTimeField, createMandatoryValidator());
                setValidator(SimilarityEngineTypeField, createMandatoryValidator());
                setValidator(ExtraTagsField, createUppercaseValidator());
                setValidator(ArtistTagDelimiterField, std::make_unique<TagDelimitersValidator>());
                setValidator(DefaultTagDelimiterField, std::make_unique<TagDelimitersValidator>());

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

                const auto extraTags{ scanSettings->getExtraTagsToScan() };
                setValue(ExtraTagsField, core::stringUtils::joinStrings(extraTags, extraTagsDelimiter));

                {
                    std::vector<std::string> delimiters{ scanSettings->getArtistTagDelimiters() };
                    setValue(ArtistTagDelimiterField, delimiters.empty() ? "" : delimiters.front());
                }

                {
                    std::vector<std::string> delimiters{ scanSettings->getDefaultTagDelimiters() };
                    setValue(DefaultTagDelimiterField, delimiters.empty() ? "" : delimiters.front());
                }
            }

            void saveData()
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                ScanSettings::pointer scanSettings{ ScanSettings::get(LmsApp->getDbSession()) };

                auto updatePeriodRow{ _updatePeriodModel->getRowFromString(valueText(UpdatePeriodField)) };
                if (updatePeriodRow)
                    scanSettings.modify()->setUpdatePeriod(_updatePeriodModel->getValue(*updatePeriodRow));

                auto startTimeRow{ _updateStartTimeModel->getRowFromString(valueText(UpdateStartTimeField)) };
                if (startTimeRow)
                    scanSettings.modify()->setUpdateStartTime(_updateStartTimeModel->getValue(*startTimeRow));

                auto similarityEngineTypeRow{ _similarityEngineTypeModel->getRowFromString(valueText(SimilarityEngineTypeField)) };
                if (similarityEngineTypeRow)
                    scanSettings.modify()->setSimilarityEngineType(_similarityEngineTypeModel->getValue(*similarityEngineTypeRow));

                scanSettings.modify()->setExtraTagsToScan(core::stringUtils::splitString(valueText(ExtraTagsField).toUTF8(), extraTagsDelimiter));
                
                {
                    std::vector<std::string_view> artistDelimiters;
                    if (std::string artistDelimiter{ valueText(ArtistTagDelimiterField).toUTF8() }; !artistDelimiter.empty())
                        artistDelimiters.push_back(std::move(artistDelimiter));
                    scanSettings.modify()->setArtistTagDelimiters(artistDelimiters);
                }

                 {
                    std::vector<std::string_view> defaultDelimiters;
                    if (std::string defaultDelimiter{ valueText(DefaultTagDelimiterField).toUTF8() }; !defaultDelimiter.empty())
                        defaultDelimiters.push_back(std::move(defaultDelimiter));
                    scanSettings.modify()->setDefaultTagDelimiters(defaultDelimiters);
                }
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
    }

    ScanSettingsView::ScanSettingsView()
    {
        wApp->internalPathChanged().connect(this, [this]()
            {
                refreshView();
            });

        refreshView();
    }

    void ScanSettingsView::refreshView()
    {
        if (!wApp->internalPathMatches("/admin/scan-settings"))
            return;

        clear();

        auto t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.Database.template")) };
        auto model{ std::make_shared<DatabaseSettingsModel>() };

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

        // Extra tags
        t->setFormWidget(DatabaseSettingsModel::ExtraTagsField, std::make_unique<Wt::WLineEdit>());

        // Artist tag delimiter
        t->setFormWidget(DatabaseSettingsModel::ArtistTagDelimiterField, std::make_unique<Wt::WLineEdit>());

        // Default tag delimiter
        t->setFormWidget(DatabaseSettingsModel::DefaultTagDelimiterField, std::make_unique<Wt::WLineEdit>());

        // Buttons
        Wt::WPushButton* saveBtn = t->bindWidget("save-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.save")));
        Wt::WPushButton* discardBtn = t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard")));

        saveBtn->clicked().connect([=]
            {
                t->updateModel(model.get());

                if (model->validate())
                {
                    model->saveData();

                    core::Service<recommendation::IRecommendationService>::get()->load();
                    // Don't want the scanner to go on with wrong settings
                    core::Service<scanner::IScannerService>::get()->requestReload();
                    LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Admin.Database.database"), Wt::WString::tr("Lms.settings-saved"));
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

        t->updateView(model.get());
    }

} // namespace lms::ui
