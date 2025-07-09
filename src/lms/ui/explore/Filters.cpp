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

#include "Filters.hpp"

#include <variant>

#include <Wt/WComboBox.h>
#include <Wt/WDialog.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>

#include "database/Session.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/LabelId.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseTypeId.hpp"

#include "LmsApplication.hpp"
#include "ModalManager.hpp"
#include "State.hpp"
#include "Utils.hpp"
#include "common/ValueStringModel.hpp"

namespace lms::ui
{
    namespace
    {
        struct MediaLibraryTag
        {
        };

        struct LabelTag
        {
        };

        struct ReleaseTypeTag
        {
        };

        using TypeVariant = std::variant<db::ClusterTypeId, MediaLibraryTag, LabelTag, ReleaseTypeTag>;
        using TypeModel = ValueStringModel<TypeVariant>;

        std::unique_ptr<TypeModel> createTypeModel()
        {
            auto typeModel{ std::make_unique<TypeModel>() };

            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                db::ClusterType::find(LmsApp->getDbSession(), [&](const db::ClusterType::pointer& clusterType) {
                    typeModel->add(Wt::WString::fromUTF8(std::string{ clusterType->getName() }), clusterType->getId());
                });
            }

            typeModel->add(Wt::WString::tr("Lms.Explore.media-library"), MediaLibraryTag{});
            typeModel->add(Wt::WString::tr("Lms.Explore.label"), LabelTag{});
            typeModel->add(Wt::WString::tr("Lms.Explore.release-type"), ReleaseTypeTag{});

            return typeModel;
        }

        using ValueVariant = std::variant<db::ClusterId, db::MediaLibraryId, db::LabelId, db::ReleaseTypeId>;
        using ValueModel = ValueStringModel<ValueVariant>;

        std::unique_ptr<ValueModel> createValueModel(TypeVariant type)
        {
            db::Session& session{ LmsApp->getDbSession() };

            auto valueModel{ std::make_unique<ValueModel>() };

            auto transaction{ session.createReadTransaction() };

            if (std::holds_alternative<MediaLibraryTag>(type))
            {
                db::MediaLibrary::find(session, [&](const db::MediaLibrary::pointer& library) {
                    valueModel->add(Wt::WString::fromUTF8(std::string{ library->getName() }), library->getId());
                });
            }
            else if (std::holds_alternative<LabelTag>(type))
            {
                db::Label::find(session, db::LabelSortMethod::Name, [&](const db::Label::pointer& label) {
                    valueModel->add(Wt::WString::fromUTF8(std::string{ label->getName() }), label->getId());
                });
            }
            else if (std::holds_alternative<ReleaseTypeTag>(type))
            {
                db::ReleaseType::find(session, db::ReleaseTypeSortMethod::Name, [&](const db::ReleaseType::pointer& releaseType) {
                    valueModel->add(Wt::WString::fromUTF8(std::string{ releaseType->getName() }), releaseType->getId());
                });
            }
            else if (const db::ClusterTypeId * clusterTypeId{ std::get_if<db::ClusterTypeId>(&type) })
            {
                db::Cluster::FindParameters params;
                params.setClusterType(*clusterTypeId);
                params.setSortMethod(db::ClusterSortMethod::Name);

                db::Cluster::find(session, params, [&](const db::Cluster::pointer& cluster) {
                    valueModel->add(Wt::WString::fromUTF8(std::string{ cluster->getName() }), cluster->getId());
                });
            }

            return valueModel;
        }
    } // namespace

    void Filters::showDialog()
    {
        auto dialog{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.template.add-filter")) };
        Wt::WWidget* dialogPtr{ dialog.get() };
        dialog->addFunction("tr", &Wt::WTemplate::Functions::tr);
        dialog->addFunction("id", &Wt::WTemplate::Functions::id);

        Wt::WComboBox* typeCombo{ dialog->bindNew<Wt::WComboBox>("type") };
        const std::shared_ptr<TypeModel> typeModel{ createTypeModel() };
        typeCombo->setModel(typeModel);

        Wt::WComboBox* valueCombo{ dialog->bindNew<Wt::WComboBox>("value") };

        Wt::WPushButton* addBtn{ dialog->bindNew<Wt::WPushButton>("add-btn", Wt::WString::tr("Lms.Explore.add-filter")) };
        addBtn->clicked().connect([this, valueCombo, dialogPtr] {
            const auto valueModel{ std::static_pointer_cast<ValueModel>(valueCombo->model()) };
            const ValueVariant value{ valueModel->getValue(valueCombo->currentIndex()) };

            if (const db::MediaLibraryId * mediaLibraryId{ std::get_if<db::MediaLibraryId>(&value) })
            {
                set(*mediaLibraryId);
                state::writeValue<db::MediaLibraryId::ValueType>("filters_media_library_id", mediaLibraryId->getValue());
            }
            else if (const db::LabelId * labelId{ std::get_if<db::LabelId>(&value) })
            {
                set(*labelId);
                state::writeValue<db::LabelId::ValueType>("filters_label_id", labelId->getValue());
            }
            else if (const db::ReleaseTypeId * releaseTypeId{ std::get_if<db::ReleaseTypeId>(&value) })
            {
                set(*releaseTypeId);
                state::writeValue<db::ReleaseTypeId::ValueType>("filters_release_type_id", releaseTypeId->getValue());
            }
            else if (const db::ClusterId * clusterId{ std::get_if<db::ClusterId>(&value) })
            {
                add(*clusterId);
            }

            // TODO
            LmsApp->getModalManager().dispose(dialogPtr);
        });

        Wt::WPushButton* cancelBtn{ dialog->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel")) };
        cancelBtn->clicked().connect([=] {
            LmsApp->getModalManager().dispose(dialogPtr);
        });

        typeCombo->activated().connect([valueCombo, typeModel](int row) {
            const TypeVariant type{ typeModel->getValue(row) };

            const std::shared_ptr<ValueModel> valueModel{ createValueModel(type) };
            valueCombo->clear();
            valueCombo->setModel(valueModel);
        });

        typeCombo->activated().emit(0); // force emit to refresh the type combo model

        LmsApp->getModalManager().show(std::move(dialog));
    }

    Filters::Filters()
        : Wt::WTemplate{ Wt::WString::tr("Lms.Explore.template.filters") }
    {
        addFunction("tr", &Functions::tr);

        // Filters
        Wt::WPushButton* addFilterBtn = bindNew<Wt::WPushButton>("add-filter", Wt::WText::tr("Lms.Explore.add-filter"));
        addFilterBtn->clicked().connect(this, &Filters::showDialog);

        _filters = bindNew<Wt::WContainerWidget>("clusters");

        if (const std::optional<db::MediaLibraryId::ValueType> mediaLibraryId{ state::readValue<db::MediaLibraryId::ValueType>("filters_media_library_id") })
            set(db::MediaLibraryId{ *mediaLibraryId });
        if (const std::optional<db::LabelId::ValueType> labelId{ state::readValue<db::LabelId::ValueType>("filters_label_id") })
            set(db::LabelId{ *labelId });
        if (const std::optional<db::ReleaseTypeId::ValueType> releaseTypeId{ state::readValue<db::ReleaseTypeId::ValueType>("filters_release_type_id") })
            set(db::ReleaseTypeId{ *releaseTypeId });
    }

    void Filters::add(db::ClusterId clusterId)
    {
        if (std::find(std::cbegin(_dbFilters.clusters), std::cend(_dbFilters.clusters), clusterId) != std::cend(_dbFilters.clusters))
            return;

        Wt::WInteractWidget* filter{};

        {
            auto cluster{ utils::createFilterCluster(clusterId, true) };
            if (!cluster)
                return;

            filter = _filters->addWidget(std::move(cluster));
        }

        _dbFilters.clusters.push_back(clusterId);

        filter->clicked().connect([this, filter, clusterId] {
            _filters->removeWidget(filter);
            _dbFilters.clusters.erase(std::remove_if(std::begin(_dbFilters.clusters), std::end(_dbFilters.clusters), [clusterId](db::ClusterId id) { return id == clusterId; }), std::end(_dbFilters.clusters));
            _sigUpdated.emit();
        });

        emitFilterAddedNotification();
    }

    void Filters::set(db::MediaLibraryId mediaLibraryId)
    {
        if (_mediaLibraryFilter)
        {
            _filters->removeWidget(_mediaLibraryFilter);
            _mediaLibraryFilter = nullptr;
            _dbFilters.mediaLibrary = db::MediaLibraryId{};
        }

        std::string libraryName;
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const auto library{ db::MediaLibrary::find(LmsApp->getDbSession(), mediaLibraryId) };
            if (!library)
                return;

            libraryName = library->getName();
        }

        _dbFilters.mediaLibrary = mediaLibraryId;
        _mediaLibraryFilter = _filters->addWidget(utils::createFilter(Wt::WString::fromUTF8(libraryName), Wt::WString::tr("Lms.Explore.media-library"), "bg-primary", true));
        _mediaLibraryFilter->clicked().connect(_mediaLibraryFilter, [this] {
            _filters->removeWidget(_mediaLibraryFilter);
            _dbFilters.mediaLibrary = db::MediaLibraryId{};
            _mediaLibraryFilter = nullptr;
            _sigUpdated.emit();
            state::writeValue<db::MediaLibraryId::ValueType>("filters_media_library_id", std::nullopt);
        });

        emitFilterAddedNotification();
    }

    void Filters::set(db::LabelId labelId)
    {
        if (_labelFilter)
        {
            _filters->removeWidget(_labelFilter);
            _labelFilter = nullptr;
            _dbFilters.label = db::LabelId{};
        }

        std::string name;
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const auto label{ db::Label::find(LmsApp->getDbSession(), labelId) };
            if (!label)
                return;

            name = label->getName();
        }

        _dbFilters.label = labelId;
        _labelFilter = _filters->addWidget(utils::createFilter(Wt::WString::fromUTF8(name), Wt::WString::tr("Lms.Explore.label"), "bg-secondary", true));
        _labelFilter->clicked().connect(_labelFilter, [this] {
            _filters->removeWidget(_labelFilter);
            _dbFilters.label = db::LabelId{};
            _labelFilter = nullptr;
            _sigUpdated.emit();
            state::writeValue<db::LabelId::ValueType>("filters_label_id", std::nullopt);
        });

        emitFilterAddedNotification();
    }

    void Filters::set(db::ReleaseTypeId releaseTypeId)
    {
        if (_releaseTypeFilter)
        {
            _filters->removeWidget(_releaseTypeFilter);
            _releaseTypeFilter = nullptr;
            _dbFilters.releaseType = db::ReleaseTypeId{};
        }

        std::string name;
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const auto releaseType{ db::ReleaseType::find(LmsApp->getDbSession(), releaseTypeId) };
            if (!releaseType)
                return;

            name = releaseType->getName();
        }

        _dbFilters.releaseType = releaseTypeId;
        _releaseTypeFilter = _filters->addWidget(utils::createFilter(Wt::WString::fromUTF8(name), Wt::WString::tr("Lms.Explore.release-type"), "bg-dark", true));
        _releaseTypeFilter->clicked().connect(_releaseTypeFilter, [this] {
            _filters->removeWidget(_releaseTypeFilter);
            _dbFilters.releaseType = db::ReleaseTypeId{};
            _releaseTypeFilter = nullptr;
            _sigUpdated.emit();
            state::writeValue<db::LabelId::ValueType>("filters_release_type_id", std::nullopt);
        });

        emitFilterAddedNotification();
    }

    void Filters::emitFilterAddedNotification()
    {
        LmsApp->notifyMsg(Notification::Type::Info,
            Wt::WString::tr("Lms.Explore.filters"),
            Wt::WString::tr("Lms.Explore.filter-added"), std::chrono::seconds{ 2 });

        _sigUpdated.emit();
    }
} // namespace lms::ui
