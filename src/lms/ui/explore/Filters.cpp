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

#include "database/Cluster.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Session.hpp"

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

        using TypeVariant = std::variant<db::ClusterTypeId, MediaLibraryTag>;
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

            return typeModel;
        }

        using ValueVariant = std::variant<db::ClusterId, db::MediaLibraryId>;
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
            set(*mediaLibraryId);
    }

    void Filters::add(db::ClusterId clusterId)
    {
        if (std::find(std::cbegin(_clusterIds), std::cend(_clusterIds), clusterId) != std::cend(_clusterIds))
            return;

        Wt::WInteractWidget* filter{};

        {
            auto cluster{ utils::createFilterCluster(clusterId, true) };
            if (!cluster)
                return;

            filter = _filters->addWidget(std::move(cluster));
        }

        _clusterIds.push_back(clusterId);

        filter->clicked().connect([this, filter, clusterId] {
            _filters->removeWidget(filter);
            _clusterIds.erase(std::remove_if(std::begin(_clusterIds), std::end(_clusterIds), [clusterId](db::ClusterId id) { return id == clusterId; }), std::end(_clusterIds));
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
            _mediaLibraryId = db::MediaLibraryId{};
        }

        std::string libraryName;
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const auto library{ db::MediaLibrary::find(LmsApp->getDbSession(), mediaLibraryId) };
            if (!library)
                return;

            libraryName = library->getName();
        }

        _mediaLibraryId = mediaLibraryId;
        _mediaLibraryFilter = _filters->addWidget(utils::createFilter(Wt::WString::fromUTF8(libraryName), Wt::WString::tr("Lms.Explore.media-library"), "bg-primary", true));
        _mediaLibraryFilter->clicked().connect(_mediaLibraryFilter, [this] {
            _filters->removeWidget(_mediaLibraryFilter);
            _mediaLibraryId = db::MediaLibraryId{};
            _mediaLibraryFilter = nullptr;
            _sigUpdated.emit();
            state::writeValue<db::MediaLibraryId::ValueType>("filters_media_library_id", std::nullopt);
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
