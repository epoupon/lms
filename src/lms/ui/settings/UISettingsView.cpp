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

#include "UISettingsView.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WFormModel.h>
#include <Wt/WString.h>
#include <Wt/WTemplateFormView.h>

#include "core/EnumSet.hpp"

#include "database/Session.hpp"
#include "database/objects/User.hpp"

#include "LmsApplication.hpp"
#include "common/CheckList.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/ValueStringModel.hpp"

#include "SettingsViewUtils.hpp"

namespace lms::ui
{
    namespace
    {
        class UISettingsModel : public Wt::WFormModel
        {
        public:
            static inline const Field ArtistReleaseSortMethodField{ "artist-release-sort-method" };
            static inline const Field EnableInlineArtistRelationships{ "enable-inline-artist-relationships" };
            static inline const Field InlineArtistRelationships{ "inline-artist-relationships" };

            using ArtistReleaseSortMethodModel = ValueStringModel<db::ReleaseSortMethod>;
            using ArtistRelationshipsModel = ValueStringModel<db::TrackArtistLinkType>;
            using ArtistRelationships = core::EnumSet<db::TrackArtistLinkType>;

            UISettingsModel()
            {
                _artistReleaseSortMethodModel = std::make_shared<ArtistReleaseSortMethodModel>();
                _artistReleaseSortMethodModel->add(Wt::WString::tr("Lms.Settings.date-asc"), db::ReleaseSortMethod::DateAsc);
                _artistReleaseSortMethodModel->add(Wt::WString::tr("Lms.Settings.date-desc"), db::ReleaseSortMethod::DateDesc);
                _artistReleaseSortMethodModel->add(Wt::WString::tr("Lms.Settings.original-date-asc"), db::ReleaseSortMethod::OriginalDate);
                _artistReleaseSortMethodModel->add(Wt::WString::tr("Lms.Settings.original-date-desc"), db::ReleaseSortMethod::OriginalDateDesc);
                _artistReleaseSortMethodModel->add(Wt::WString::tr("Lms.Settings.name"), db::ReleaseSortMethod::Name);

                _artistRelationshipsModel = std::make_shared<ArtistRelationshipsModel>();
                _artistRelationshipsModel->add(Wt::WString::trn("Lms.Explore.composer", 2), db::TrackArtistLinkType::Composer);
                _artistRelationshipsModel->add(Wt::WString::trn("Lms.Explore.conductor", 2), db::TrackArtistLinkType::Conductor);
                _artistRelationshipsModel->add(Wt::WString::trn("Lms.Explore.lyricist", 2), db::TrackArtistLinkType::Lyricist);
                _artistRelationshipsModel->add(Wt::WString::trn("Lms.Explore.mixer", 2), db::TrackArtistLinkType::Mixer);
                _artistRelationshipsModel->add(Wt::WString::trn("Lms.Explore.performer", 2), db::TrackArtistLinkType::Performer);
                _artistRelationshipsModel->add(Wt::WString::trn("Lms.Explore.producer", 2), db::TrackArtistLinkType::Producer);
                _artistRelationshipsModel->add(Wt::WString::trn("Lms.Explore.remixer", 2), db::TrackArtistLinkType::Remixer);

                addField(ArtistReleaseSortMethodField);
                addField(EnableInlineArtistRelationships);
                addField(InlineArtistRelationships);

                setValidator(ArtistReleaseSortMethodField, createMandatoryValidator());

                loadData();
            }

            std::shared_ptr<ArtistReleaseSortMethodModel> getArtistReleaseSortMethodModel() { return _artistReleaseSortMethodModel; }
            std::shared_ptr<ArtistRelationshipsModel> getArtistRelationshipsModel() { return _artistRelationshipsModel; }

            void saveData()
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };
                db::User::pointer user{ LmsApp->getUser() };

                const auto artistReleaseSortMethodRow{ _artistReleaseSortMethodModel->getRowFromString(valueText(ArtistReleaseSortMethodField)) };
                if (artistReleaseSortMethodRow)
                    user.modify()->setUIArtistReleaseSortMethod(_artistReleaseSortMethodModel->getValue(*artistReleaseSortMethodRow));

                const bool enableInlineArtistRelationships{ Wt::asNumber(value(EnableInlineArtistRelationships)) != 0 };
                user.modify()->setUIEnableInlineArtistRelationships(enableInlineArtistRelationships);

                user.modify()->setUIInlineArtistRelationships(Wt::cpp17::any_cast<ArtistRelationships>(value(InlineArtistRelationships)));
            }

            void loadData()
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                const db::User::pointer user{ LmsApp->getUser() };

                auto artistReleaseSortMethodRow{ _artistReleaseSortMethodModel->getRowFromValue(user->getUIArtistReleaseSortMethod()) };
                if (artistReleaseSortMethodRow)
                    setValue(ArtistReleaseSortMethodField, _artistReleaseSortMethodModel->getString(*artistReleaseSortMethodRow));

                setValue(EnableInlineArtistRelationships, user->getUIEnableInlineArtistRelationships());
                setReadOnly(InlineArtistRelationships, !user->getUIEnableInlineArtistRelationships());
                setValue(InlineArtistRelationships, user->getUIInlineArtistRelationships());
            }

        private:
            std::shared_ptr<ArtistReleaseSortMethodModel> _artistReleaseSortMethodModel;
            std::shared_ptr<ArtistRelationshipsModel> _artistRelationshipsModel;
        };
    } // namespace

    UISettingsView::UISettingsView()
    {
        wApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        refreshView();
    }

    void UISettingsView::refreshView()
    {
        if (!wApp->internalPathMatches("/settings/ui"))
            return;

        clear();

        auto* t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Settings.ui.template")) };
        auto model{ std::make_shared<UISettingsModel>() };

        auto enableInlineArtistRelationships{ std::make_unique<Wt::WCheckBox>() };
        auto inlineArtistRelationships{ std::make_unique<CheckList>() };
        const auto artistRelationshipsModel{ model->getArtistRelationshipsModel() };
        inlineArtistRelationships->setModel(artistRelationshipsModel);
        CheckList* inlineArtistRelationshipsList{ inlineArtistRelationships.get() };

        auto updateInlineArtistRelationships{ [=](bool readOnly) {
            model->setReadOnly(UISettingsModel::InlineArtistRelationships, readOnly);
            t->updateModel(model.get());
            t->updateView(model.get());
        } };
        enableInlineArtistRelationships->checked().connect([=] { updateInlineArtistRelationships(false); });
        enableInlineArtistRelationships->unChecked().connect([=] { updateInlineArtistRelationships(true); });

        auto artistReleaseSortMethod{ std::make_unique<Wt::WComboBox>() };
        artistReleaseSortMethod->setModel(model->getArtistReleaseSortMethodModel());
        t->setFormWidget(UISettingsModel::ArtistReleaseSortMethodField, std::move(artistReleaseSortMethod));
        t->setFormWidget(UISettingsModel::EnableInlineArtistRelationships, std::move(enableInlineArtistRelationships));
        t->setFormWidget(
            UISettingsModel::InlineArtistRelationships, std::move(inlineArtistRelationships),
            [=] {
                const auto artistRelationships{ Wt::cpp17::any_cast<UISettingsModel::ArtistRelationships>(model->value(UISettingsModel::InlineArtistRelationships)) };
                for (std::size_t row{}; row < static_cast<std::size_t>(artistRelationshipsModel->rowCount()); ++row)
                    inlineArtistRelationshipsList->setChecked(row, artistRelationships.contains(artistRelationshipsModel->getValue(row)));
            },
            [=] {
                UISettingsModel::ArtistRelationships artistRelationships;
                for (const std::size_t row : inlineArtistRelationshipsList->getCheckedRows())
                    artistRelationships.insert(artistRelationshipsModel->getValue(row));
                model->setValue(UISettingsModel::InlineArtistRelationships, artistRelationships);
            });

        utils::bindSaveDiscardButtons(t, model.get(), [model] { model->saveData(); }, [model] { model->loadData(); });
        t->updateView(model.get());
    }
} // namespace lms::ui
