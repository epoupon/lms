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

#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WTemplateFormView.h>
#include <Wt/WTextArea.h>

#include "core/Service.hpp"
#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/ScanSettings.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "services/scanner/IScannerService.hpp"

#include "LmsApplication.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/UppercaseValidator.hpp"
#include "common/ValueStringModel.hpp"

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
                    return Wt::WValidator::validate(input);

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
            static inline constexpr Field SkipSingleReleasePlayListsField{ "skip-single-release-playlists" };
            static inline constexpr Field AllowMBIDArtistMergeField{ "allow-mbid-artist-merge" };
            static inline constexpr Field ArtistImageFallbackToReleaseField{ "artist-image-fallback-to-release" };
            static inline constexpr Field ArtistsToNotSplitField{ "artists-to-not-split" };

            using UpdatePeriodModel = ValueStringModel<ScanSettings::UpdatePeriod>;

            DatabaseSettingsModel()
            {
                initializeModels();

                addField(UpdatePeriodField);
                addField(UpdateStartTimeField);
                addField(SimilarityEngineTypeField);
                addField(SkipSingleReleasePlayListsField);
                addField(AllowMBIDArtistMergeField);
                addField(ArtistImageFallbackToReleaseField);
                addField(ArtistsToNotSplitField);

                setValidator(UpdatePeriodField, createMandatoryValidator());
                setValidator(UpdateStartTimeField, createMandatoryValidator());
                setValidator(SimilarityEngineTypeField, createMandatoryValidator());
                setValidator(SkipSingleReleasePlayListsField, createMandatoryValidator());
                setValidator(AllowMBIDArtistMergeField, createMandatoryValidator());
                setValidator(ArtistImageFallbackToReleaseField, createMandatoryValidator());
            }

            std::shared_ptr<UpdatePeriodModel> updatePeriodModel() { return _updatePeriodModel; }
            std::shared_ptr<Wt::WAbstractItemModel> updateStartTimeModel() { return _updateStartTimeModel; }
            std::shared_ptr<Wt::WAbstractItemModel> similarityEngineTypeModel() { return _similarityEngineTypeModel; }

            void loadData(std::vector<std::string>& extraTagsToScan, std::vector<std::string>& artistDelimiters, std::vector<std::string>& defaultDelimiters)
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };

                const ScanSettings::pointer scanSettings{ ScanSettings::find(LmsApp->getDbSession()) };

                auto periodRow{ _updatePeriodModel->getRowFromValue(scanSettings->getUpdatePeriod()) };
                if (periodRow)
                    setValue(UpdatePeriodField, _updatePeriodModel->getString(*periodRow));

                auto startTimeRow{ _updateStartTimeModel->getRowFromValue(scanSettings->getUpdateStartTime()) };
                if (startTimeRow)
                    setValue(UpdateStartTimeField, _updateStartTimeModel->getString(*startTimeRow));

                if (scanSettings->getUpdatePeriod() == ScanSettings::UpdatePeriod::Hourly
                    || scanSettings->getUpdatePeriod() == ScanSettings::UpdatePeriod::Never)
                {
                    setReadOnly(UpdateStartTimeField, true);
                }

                setValue(SkipSingleReleasePlayListsField, scanSettings->getSkipSingleReleasePlayLists());
                setValue(AllowMBIDArtistMergeField, scanSettings->getAllowMBIDArtistMerge());
                setValue(ArtistImageFallbackToReleaseField, scanSettings->getArtistImageFallbackToReleaseField());

                auto similarityEngineTypeRow{ _similarityEngineTypeModel->getRowFromValue(scanSettings->getSimilarityEngineType()) };
                if (similarityEngineTypeRow)
                    setValue(SimilarityEngineTypeField, _similarityEngineTypeModel->getString(*similarityEngineTypeRow));

                const auto extraTags{ scanSettings->getExtraTagsToScan() };
                extraTagsToScan.clear();
                std::transform(std::cbegin(extraTags), std::cend(extraTags), std::back_inserter(extraTagsToScan), [](std::string_view extraTag) { return std::string{ extraTag }; });
                artistDelimiters = scanSettings->getArtistTagDelimiters();
                defaultDelimiters = scanSettings->getDefaultTagDelimiters();

                {
                    std::string artists{ core::stringUtils::joinStrings(scanSettings->getArtistsToNotSplit(), '\n') };
                    setValue(ArtistsToNotSplitField, Wt::WString::fromUTF8(std::move(artists)));
                    if (artistDelimiters.empty())
                        setReadOnly(ArtistsToNotSplitField, true);
                }
            }

            void saveData(std::span<const std::string_view> extraTagsToScan, std::span<const std::string_view> artistDelimiters, std::span<const std::string_view> defaultDelimiters)
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                ScanSettings::pointer scanSettings{ ScanSettings::find(LmsApp->getDbSession()) };

                {
                    const auto updatePeriodRow{ _updatePeriodModel->getRowFromString(valueText(UpdatePeriodField)) };
                    if (updatePeriodRow)
                        scanSettings.modify()->setUpdatePeriod(_updatePeriodModel->getValue(*updatePeriodRow));
                }

                {
                    const auto startTimeRow{ _updateStartTimeModel->getRowFromString(valueText(UpdateStartTimeField)) };
                    if (startTimeRow)
                        scanSettings.modify()->setUpdateStartTime(_updateStartTimeModel->getValue(*startTimeRow));
                }

                {
                    const bool skipSingleReleasePlayLists{ Wt::asNumber(value(SkipSingleReleasePlayListsField)) != 0 };
                    scanSettings.modify()->setSkipSingleReleasePlayLists(skipSingleReleasePlayLists);
                }

                {
                    const bool allowMBIDArtistMerge{ Wt::asNumber(value(AllowMBIDArtistMergeField)) != 0 };
                    scanSettings.modify()->setAllowMBIDArtistMerge(allowMBIDArtistMerge);
                }

                {
                    const bool artistImageFallbackToRelease{ Wt::asNumber(value(ArtistImageFallbackToReleaseField)) != 0 };
                    scanSettings.modify()->setArtistImageFallbackToReleaseField(artistImageFallbackToRelease);
                }

                {
                    const auto similarityEngineTypeRow{ _similarityEngineTypeModel->getRowFromString(valueText(SimilarityEngineTypeField)) };
                    if (similarityEngineTypeRow)
                        scanSettings.modify()->setSimilarityEngineType(_similarityEngineTypeModel->getValue(*similarityEngineTypeRow));
                }

                scanSettings.modify()->setExtraTagsToScan(extraTagsToScan);
                scanSettings.modify()->setArtistTagDelimiters(artistDelimiters);
                scanSettings.modify()->setDefaultTagDelimiters(defaultDelimiters);

                {
                    const std::string artists{ valueText(ArtistsToNotSplitField).toUTF8() };
                    std::vector<std::string_view> artistsToNotSplit{ core::stringUtils::splitString(artists, '\n') };
                    scanSettings.modify()->setArtistsToNotSplit(artistsToNotSplit);
                }
            }

        private:
            void
            initializeModels()
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

            std::shared_ptr<UpdatePeriodModel> _updatePeriodModel;
            std::shared_ptr<ValueStringModel<Wt::WTime>> _updateStartTimeModel;
            std::shared_ptr<ValueStringModel<ScanSettings::SimilarityEngineType>> _similarityEngineTypeModel;
        };

        class LineEditEntryModel : public Wt::WFormModel
        {
        public:
            static inline constexpr Field ValueField{ "value" };

            LineEditEntryModel(const Wt::WString& initialValue, std::shared_ptr<Wt::WValidator> validator)
                : Wt::WFormModel()
            {
                addField(ValueField);

                setValidator(ValueField, validator);
                setValue(ValueField, initialValue);
            }
        };

        class LineEditEntryWidget : public Wt::WTemplateFormView
        {
        public:
            LineEditEntryWidget(const Wt::WString& initialValue, std::shared_ptr<Wt::WValidator> validator)
                : Wt::WTemplateFormView{ Wt::WString::tr("Lms.Admin.Database.template.line-edit-entry") }
                , _model{ std::make_shared<LineEditEntryModel>(initialValue, validator) }
            {
                setStyleClass("col-sm-4 col-md-3"); // hack

                setFormWidget(LineEditEntryModel::ValueField, std::make_unique<Wt::WLineEdit>());

                auto* delBtn{ bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.template.trash-btn"), Wt::TextFormat::XHTML) };
                delBtn->clicked().connect(this, [this] { deleted.emit(); });
            }

            bool validate() { return _model->validate(); }
            void refreshModel() { Wt::WTemplateFormView::updateModel(_model.get()); }
            void refreshView() { Wt::WTemplateFormView::updateView(_model.get()); }

            Wt::WString getValue() const
            {
                return _model->valueText(LineEditEntryModel::ValueField);
            }

            Wt::Signal<> deleted;
            std::shared_ptr<LineEditEntryModel> _model;
        };

        // Terrible hack to make use of the validation system for each added element
        class LineEditContainerWidget : public Wt::WContainerWidget
        {
        public:
            LineEditContainerWidget(std::shared_ptr<Wt::WValidator> validator)
                : _validator{ validator } {}

            Wt::Signal<std::size_t> sizeChanged;

            void add(const Wt::WString& value = "")
            {
                auto* entry{ addNew<LineEditEntryWidget>(value, _validator) };

                entry->deleted.connect(this, [=, this] {
                    removeWidget(entry);
                    sizeChanged.emit(count());
                });

                sizeChanged.emit(count());
            }

            bool validate()
            {
                bool res{ true };

                for (int i{}; i < count(); ++i)
                {
                    LineEditEntryWidget* entry{ static_cast<LineEditEntryWidget*>(widget(i)) };
                    res &= entry->validate();
                }

                return res;
            }

            void refreshModels()
            {
                for (int i{}; i < count(); ++i)
                {
                    LineEditEntryWidget* entry{ static_cast<LineEditEntryWidget*>(widget(i)) };
                    entry->refreshModel();
                }
            }

            void refreshViews()
            {
                for (int i{}; i < count(); ++i)
                {
                    LineEditEntryWidget* entry{ static_cast<LineEditEntryWidget*>(widget(i)) };
                    entry->refreshView();
                }
            }

            void visitValues(std::function<void(Wt::WString)> visitor) const
            {
                for (int i{}; i < count(); ++i)
                {
                    LineEditEntryWidget* entry{ static_cast<LineEditEntryWidget*>(widget(i)) };
                    visitor(entry->getValue());
                }
            }

            std::vector<std::string> getValues() const
            {
                std::vector<std::string> values;
                visitValues([&](const Wt::WString& value) {
                    values.push_back(value.toUTF8());
                });
                return values;
            }

        private:
            std::shared_ptr<Wt::WValidator> _validator;
        };
    } // namespace

    ScanSettingsView::ScanSettingsView()
    {
        wApp->internalPathChanged().connect(this, [this]() {
            refreshView();
        });

        refreshView();
    }

    void ScanSettingsView::refreshView()
    {
        if (!wApp->internalPathMatches("/admin/scan-settings"))
            return;

        clear();

        auto* t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.Database.template")) };
        auto model{ std::make_shared<DatabaseSettingsModel>() };

        // Update Period
        auto updatePeriod{ std::make_unique<Wt::WComboBox>() };
        updatePeriod->setModel(model->updatePeriodModel());
        updatePeriod->activated().connect([=](int row) {
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

        // Skip playlists
        t->setFormWidget(DatabaseSettingsModel::SkipSingleReleasePlayListsField, std::make_unique<Wt::WCheckBox>());

        // Allow to merge artists without MBID with those with one
        t->setFormWidget(DatabaseSettingsModel::AllowMBIDArtistMergeField, std::make_unique<Wt::WCheckBox>());

        // Allow to fallback on release image if artist image is not available
        t->setFormWidget(DatabaseSettingsModel::ArtistImageFallbackToReleaseField, std::make_unique<Wt::WCheckBox>());

        // Similarity engine type
        auto similarityEngineType{ std::make_unique<Wt::WComboBox>() };
        similarityEngineType->setModel(model->similarityEngineTypeModel());
        t->setFormWidget(DatabaseSettingsModel::SimilarityEngineTypeField, std::move(similarityEngineType));

        // Extra tags
        std::shared_ptr<Wt::WValidator> extraTagValidator{ createUppercaseValidator() };
        extraTagValidator->setMandatory(true);
        auto* extraTagsToScan{ t->bindNew<LineEditContainerWidget>("extra-tags-to-scan-container", extraTagValidator) };
        {
            auto* addExtraScanToScanBtn{ t->bindNew<Wt::WPushButton>("extra-tags-to-scan-add-btn", Wt::WString::tr("Lms.add")) };
            addExtraScanToScanBtn->clicked().connect(this, [=] {
                extraTagsToScan->add();
            });
        }

        // Artist tag delimiter
        std::shared_ptr<Wt::WValidator> tagDelimiterValidator{ std::make_shared<TagDelimitersValidator>() };
        tagDelimiterValidator->setMandatory(true);

        auto* artistTagDelimiters{ t->bindNew<LineEditContainerWidget>("artist-tag-delimiter-container", tagDelimiterValidator) };
        {
            auto* addArtistDelimiterBtn{ t->bindNew<Wt::WPushButton>("artist-tag-delimiter-add-btn", Wt::WString::tr("Lms.add")) };
            addArtistDelimiterBtn->clicked().connect(this, [=] {
                artistTagDelimiters->add();
            });
        }

        t->setFormWidget(DatabaseSettingsModel::ArtistsToNotSplitField, std::make_unique<Wt::WTextArea>());
        artistTagDelimiters->sizeChanged.connect(this, [=](std::size_t newSize) {
            model->setReadOnly(DatabaseSettingsModel::ArtistsToNotSplitField, newSize == 0);
            t->updateView(model.get());
        });

        // Default tag delimiter
        auto* defaultTagDelimiters{ t->bindNew<LineEditContainerWidget>("default-tag-delimiter-container", tagDelimiterValidator) };
        {
            auto* addDefaultDelimiterBtn{ t->bindNew<Wt::WPushButton>("default-tag-delimiter-add-btn", Wt::WString::tr("Lms.add")) };
            addDefaultDelimiterBtn->clicked().connect(this, [=] {
                defaultTagDelimiters->add();
            });
        }

        // Buttons
        Wt::WPushButton* saveBtn{ t->bindWidget("save-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.save"))) };
        Wt::WPushButton* discardBtn{ t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard"))) };

        auto validate{ [=] {
            bool res{ true };

            res &= model->validate();
            res &= extraTagsToScan->validate();
            res &= artistTagDelimiters->validate();
            res &= defaultTagDelimiters->validate();

            return res;
        } };

        auto updateModels{ [=] {
            t->updateModel(model.get());
            extraTagsToScan->refreshModels();
            artistTagDelimiters->refreshModels();
            defaultTagDelimiters->refreshModels();
        } };

        auto updateViews{ [=] {
            t->updateView(model.get());
            extraTagsToScan->refreshViews();
            artistTagDelimiters->refreshViews();
            defaultTagDelimiters->refreshViews();
        } };

        auto loadInitialData{ [=] {
            std::vector<std::string> extraTags;
            std::vector<std::string> artistDelimiters;
            std::vector<std::string> defaultDelimiters;
            model->loadData(extraTags, artistDelimiters, defaultDelimiters);

            extraTagsToScan->clear();
            for (const std::string& extraTag : extraTags)
                extraTagsToScan->add(Wt::WString::fromUTF8(extraTag));

            artistTagDelimiters->clear();
            for (const std::string& artistDelimiter : artistDelimiters)
                artistTagDelimiters->add(Wt::WString::fromUTF8(artistDelimiter));

            defaultTagDelimiters->clear();
            for (const std::string& defaultDelimiter : defaultDelimiters)
                defaultTagDelimiters->add(Wt::WString::fromUTF8(defaultDelimiter));
        } };

        saveBtn->clicked().connect([=] {
            updateModels();
            if (validate())
            {
                const std::vector<std::string> extraTags{ extraTagsToScan->getValues() };
                std::vector<std::string_view> extraTagViews;
                std::transform(std::cbegin(extraTags), std::cend(extraTags), std::back_inserter(extraTagViews), [](const std::string& tag) -> std::string_view { return tag; });

                const std::vector<std::string> artistDelimiters{ artistTagDelimiters->getValues() };
                std::vector<std::string_view> artistDelimiterViews;
                std::transform(std::cbegin(artistDelimiters), std::cend(artistDelimiters), std::back_inserter(artistDelimiterViews), [](const std::string& delimiter) -> std::string_view { return delimiter; });

                const std::vector<std::string> defaultDelimiters{ defaultTagDelimiters->getValues() };
                std::vector<std::string_view> defaultDelimiterViews;
                std::transform(std::cbegin(defaultDelimiters), std::cend(defaultDelimiters), std::back_inserter(defaultDelimiterViews), [](const std::string& delimiter) -> std::string_view { return delimiter; });

                model->saveData(extraTagViews, artistDelimiterViews, defaultDelimiterViews);

                core::Service<recommendation::IRecommendationService>::get()->load();
                // Don't want the scanner to go on with wrong settings
                core::Service<scanner::IScannerService>::get()->requestReload();
                LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Admin.Database.database"), Wt::WString::tr("Lms.settings-saved"));
            }

            // Udate the view: Delete any validation message in the view, etc.
            updateViews();
        });

        discardBtn->clicked().connect([=] {
            loadInitialData();
            validate();
            updateViews();
        });

        loadInitialData();
        updateViews();
    }
} // namespace lms::ui
