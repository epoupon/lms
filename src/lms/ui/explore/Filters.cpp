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

#include <algorithm>
#include <cassert>
#include <span>
#include <variant>

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Serializer.h>
#include <Wt/Json/Value.h>
#include <Wt/WComboBox.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>

#include "core/ILogger.hpp"
#include "core/Utils.hpp"

#include "core/media/Codec.hpp"
#include "database/ISession.hpp"
#include "database/Types.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Release.hpp"

#include "LmsApplication.hpp"
#include "ModalManager.hpp"
#include "Utils.hpp"
#include "WebStorage.hpp"
#include "common/CheckList.hpp"
#include "common/ValueStringModel.hpp"

namespace lms::ui
{
    namespace
    {
        constexpr std::string_view storageKey{ "filters" };

        struct MediaLibraryTag
        {
        };

        struct LabelTag
        {
        };

        struct ReleaseTypeTag
        {
        };

        struct CodecTag
        {
        };

        struct GenreTag
        {
        };

        struct GroupingTag
        {
        };

        struct LanguageTag
        {
        };

        struct MoodTag
        {
        };

        using TypeVariant = std::variant<db::ClusterTypeId, MediaLibraryTag, LabelTag, ReleaseTypeTag, CodecTag, GenreTag, GroupingTag, LanguageTag, MoodTag>;
        using TypeModel = ValueStringModel<TypeVariant>;

        std::unique_ptr<TypeModel> createTypeModel()
        {
            auto typeModel{ std::make_unique<TypeModel>() };

            typeModel->add(Wt::WString::trn("Lms.Explore.genre", 1), GenreTag{});
            typeModel->add(Wt::WString::trn("Lms.Explore.grouping", 1), GroupingTag{});
            typeModel->add(Wt::WString::trn("Lms.Explore.language", 1), LanguageTag{});
            typeModel->add(Wt::WString::trn("Lms.Explore.mood", 1), MoodTag{});

            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                db::ClusterType::find(LmsApp->getDbSession(), [&](const db::ClusterType& clusterType) {
                    typeModel->add(Wt::WString::fromUTF8(std::string{ clusterType.getName() }), clusterType.getId());
                });
            }

            typeModel->add(Wt::WString::tr("Lms.Explore.media-library"), MediaLibraryTag{});
            typeModel->add(Wt::WString::tr("Lms.Explore.label"), LabelTag{});
            typeModel->add(Wt::WString::tr("Lms.Explore.release-type"), ReleaseTypeTag{});
            typeModel->add(Wt::WString::tr("Lms.Explore.codec"), CodecTag{});

            return typeModel;
        }

        using ValueModel = ValueStringModel<Filters::Value>;

        std::unique_ptr<ValueModel> createValueModel(TypeVariant type)
        {
            db::ISession& session{ LmsApp->getDbSession() };

            auto valueModel{ std::make_unique<ValueModel>() };

            auto transaction{ session.createReadTransaction() };

            std::visit(core::utils::overloads{
                           [&](const MediaLibraryTag&) {
                               db::MediaLibrary::find(session, [&](const db::MediaLibrary& library) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ library.getName() }), library.getId());
                               });
                           },
                           [&](const LabelTag&) {
                               db::Label::find(session, db::LabelSortMethod::Name, [&](const db::Label& label) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ label.getName() }), label.getId());
                               });
                           },
                           [&](const ReleaseTypeTag&) {
                               db::ReleaseType::find(session, db::ReleaseTypeSortMethod::Name, [&](const db::ReleaseType& releaseType) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ releaseType.getName() }), releaseType.getId());
                               });
                           },
                           [&](const CodecTag&) {
                               core::media::visitCodecs([&](const core::media::CodecDesc& desc) {
                                   valueModel->add(Wt::WString::fromUTF8(desc.longName.c_str()), desc.type);
                               });
                           },
                           [&](const GenreTag&) {
                               db::Genre::find(session, db::Genre::FindParameters{}.setSortMethod(db::GenreSortMethod::Name), [&](const db::Genre& genre) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ genre.getName() }), genre.getId());
                               });
                           },
                           [&](const GroupingTag&) {
                               db::Grouping::find(session, db::Grouping::FindParameters{}.setSortMethod(db::GroupingSortMethod::Name), [&](const db::Grouping& grouping) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ grouping.getName() }), grouping.getId());
                               });
                           },
                           [&](const LanguageTag&) {
                               db::Language::find(session, db::Language::FindParameters{}.setSortMethod(db::LanguageSortMethod::Name), [&](const db::Language& language) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ language.getName() }), language.getId());
                               });
                           },
                           [&](const MoodTag&) {
                               db::Mood::find(session, db::Mood::FindParameters{}.setSortMethod(db::MoodSortMethod::Name), [&](const db::Mood& mood) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ mood.getName() }), mood.getId());
                               });
                           },
                           [&](db::ClusterTypeId clusterTypeId) {
                               db::Cluster::FindParameters params;
                               params.setClusterType(clusterTypeId);
                               params.setSortMethod(db::ClusterSortMethod::Name);

                               db::Cluster::find(session, params, [&](const db::Cluster& cluster) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ cluster.getName() }), cluster.getId());
                               });
                           } },
                       type);
            return valueModel;
        }

        using MatchModel = ValueStringModel<Filters::Match>;

        std::unique_ptr<MatchModel> createMatchModel()
        {
            auto matchModel{ std::make_unique<MatchModel>() };

            matchModel->add(Wt::WString::tr("Lms.Explore.filter-one-of"), Filters::Match::OneOf);
            matchModel->add(Wt::WString::tr("Lms.Explore.filter-none-of"), Filters::Match::NoneOf);

            return matchModel;
        }

        // Values are stored by name: ids change when a tag disappears from the library and comes back
        struct StoredValue
        {
            std::string type;
            std::string clusterType; // cluster values only
            std::string name;
        };

        template<typename Object, typename Id>
        std::optional<StoredValue> findStoredValue(db::ISession& session, std::string_view type, Id id)
        {
            const auto object{ Object::find(session, id) };
            if (!object)
                return std::nullopt;

            return StoredValue{ std::string{ type }, {}, std::string{ object->getName() } };
        }

        // nullopt if the value no longer exists
        std::optional<StoredValue> toStoredValue(db::ISession& session, const Filters::Value& value)
        {
            return std::visit(core::utils::overloads{
                                  [&](db::ClusterId id) -> std::optional<StoredValue> {
                                      const std::optional<db::Cluster> cluster{ db::Cluster::find(session, id) };
                                      if (!cluster)
                                          return std::nullopt;

                                      return StoredValue{ "cluster", std::string{ db::ClusterType::find(session, cluster->getTypeId())->getName() }, std::string{ cluster->getName() } };
                                  },
                                  [&](db::MediaLibraryId id) { return findStoredValue<db::MediaLibrary>(session, "mediaLibrary", id); },
                                  [&](db::LabelId id) { return findStoredValue<db::Label>(session, "label", id); },
                                  [&](db::ReleaseTypeId id) { return findStoredValue<db::ReleaseType>(session, "releaseType", id); },
                                  [](core::media::Codec codec) -> std::optional<StoredValue> {
                                      return StoredValue{ "codec", {}, std::string{ core::media::getCodecDesc(codec).name.str() } };
                                  },
                                  [&](db::GenreId id) { return findStoredValue<db::Genre>(session, "genre", id); },
                                  [&](db::GroupingId id) { return findStoredValue<db::Grouping>(session, "grouping", id); },
                                  [&](db::LanguageId id) { return findStoredValue<db::Language>(session, "language", id); },
                                  [&](db::MoodId id) { return findStoredValue<db::Mood>(session, "mood", id); },
                              },
                              value);
        }

        // A filter is stored as a whole: nullopt if one of its values no longer exists
        std::optional<Wt::Json::Object> toStoredFilter(db::ISession& session, std::span<const Filters::Value> values, Filters::Match match)
        {
            Wt::Json::Object storedFilter;
            Wt::Json::Array names;
            for (const Filters::Value& value : values)
            {
                const std::optional<StoredValue> storedValue{ toStoredValue(session, value) };
                if (!storedValue)
                    return std::nullopt;

                // All the values of a filter have the same type
                storedFilter["type"] = Wt::Json::Value{ storedValue->type };
                if (!storedValue->clusterType.empty())
                    storedFilter["clusterType"] = Wt::Json::Value{ storedValue->clusterType };
                names.push_back(Wt::Json::Value{ storedValue->name });
            }

            storedFilter["noneOf"] = Wt::Json::Value{ match == Filters::Match::NoneOf };
            storedFilter["values"] = std::move(names);
            return storedFilter;
        }

        template<typename Object>
        std::optional<Filters::Value> findValue(db::ISession& session, std::string_view name)
        {
            const auto object{ Object::find(session, name) };
            if (!object)
                return std::nullopt;

            return object->getId();
        }

        // nullopt if the value no longer exists
        std::optional<Filters::Value> fromStoredValue(db::ISession& session, const StoredValue& storedValue)
        {
            if (storedValue.type == "cluster")
            {
                const std::optional<db::ClusterType> clusterType{ db::ClusterType::find(session, storedValue.clusterType) };
                if (!clusterType)
                    return std::nullopt;

                const std::optional<db::Cluster> cluster{ db::Cluster::find(session, clusterType->getId(), storedValue.name) };
                if (!cluster)
                    return std::nullopt;

                return cluster->getId();
            }
            if (storedValue.type == "mediaLibrary")
                return findValue<db::MediaLibrary>(session, storedValue.name);
            if (storedValue.type == "label")
                return findValue<db::Label>(session, storedValue.name);
            if (storedValue.type == "releaseType")
                return findValue<db::ReleaseType>(session, storedValue.name);
            if (storedValue.type == "codec")
                return core::media::findCodec(storedValue.name);
            if (storedValue.type == "genre")
                return findValue<db::Genre>(session, storedValue.name);
            if (storedValue.type == "grouping")
                return findValue<db::Grouping>(session, storedValue.name);
            if (storedValue.type == "language")
                return findValue<db::Language>(session, storedValue.name);
            if (storedValue.type == "mood")
                return findValue<db::Mood>(session, storedValue.name);

            return std::nullopt;
        }

        // A filter is restored as a whole: nullopt if one of its values cannot be
        std::optional<std::vector<Filters::Value>> fromStoredFilter(db::ISession& session, const Wt::Json::Object& filter)
        {
            StoredValue storedValue{ .type = filter.get("type").toString().orIfNull(""), .clusterType = filter.get("clusterType").toString().orIfNull(""), .name = {} };

            std::vector<Filters::Value> values;
            const Wt::Json::Array& names = filter.get("values");
            for (const Wt::Json::Value& name : names)
            {
                storedValue.name = name.toString().orIfNull("");
                const std::optional<Filters::Value> value{ fromStoredValue(session, storedValue) };
                if (!value)
                    return std::nullopt;

                values.push_back(*value);
            }

            return values;
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

        Wt::WComboBox* matchCombo{ dialog->bindNew<Wt::WComboBox>("match") };
        const std::shared_ptr<MatchModel> matchModel{ createMatchModel() };
        matchCombo->setModel(matchModel);

        CheckList* valueList{ dialog->bindNew<CheckList>("value") };

        Wt::WPushButton* addBtn{ dialog->bindNew<Wt::WPushButton>("add-btn", Wt::WString::tr("Lms.Explore.add-filter")) };
        addBtn->clicked().connect([this, matchCombo, matchModel, valueList, dialogPtr] {
            const auto valueModel{ std::static_pointer_cast<ValueModel>(valueList->getModel()) };

            std::vector<Value> values;
            for (const std::size_t row : valueList->getCheckedRows())
                values.push_back(valueModel->getValue(row));

            if (addFilter(matchModel->getValue(matchCombo->currentIndex()), std::move(values)))
                emitFilterAddedNotification();

            // TODO
            LmsApp->getModalManager().dispose(dialogPtr);
        });

        Wt::WPushButton* cancelBtn{ dialog->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel")) };
        cancelBtn->clicked().connect([=] {
            LmsApp->getModalManager().dispose(dialogPtr);
        });

        valueList->checkedChanged().connect([valueList, addBtn] {
            addBtn->setDisabled(valueList->getCheckedRows().empty());
        });

        typeCombo->activated().connect([valueList, addBtn, typeModel](int row) {
            const TypeVariant type{ typeModel->getValue(row) };

            valueList->setModel(createValueModel(type));
            addBtn->setDisabled(true);
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

        _filtersContainer = bindNew<Wt::WContainerWidget>("filters");

        LmsApp->getWebStorage().getItem(*this, storageKey, [this](std::optional<std::string> storedFilters) {
            if (storedFilters)
                restore(*storedFilters);
        });
    }

    void Filters::restore(const std::string& storedFilters)
    {
        Wt::Json::Array filters;
        Wt::Json::ParseError error;
        if (!Wt::Json::parse(storedFilters, filters, error))
        {
            LMS_LOG(UI, WARNING, "Cannot parse stored filters: " << error.what());
            return;
        }

        bool added{};
        {
            db::ISession& session{ LmsApp->getDbSession() };
            auto transaction{ session.createReadTransaction() };

            for (const Wt::Json::Value& filterValue : filters)
            {
                if (filterValue.type() != Wt::Json::Type::Object)
                    continue;

                const Wt::Json::Object& filter = filterValue;
                if (filter.type("values") != Wt::Json::Type::Array)
                    continue;

                std::optional<std::vector<Value>> values;
                try
                {
                    values = fromStoredFilter(session, filter);
                }
                catch (const db::Exception& e)
                {
                    // The stored filters come from the browser: an edited name may be too long to be looked up
                    LMS_LOG(UI, WARNING, "Cannot restore stored filter: " << e.what());
                }

                const Match match{ filter.get("noneOf").toBool().orIfNull(false) ? Match::NoneOf : Match::OneOf };
                if (values && !values->empty() && addFilter(match, std::move(*values)))
                    added = true;
            }
        }

        if (added)
            notifyUpdated();
    }

    void Filters::store() const
    {
        if (_filters.empty())
        {
            LmsApp->getWebStorage().removeItem(storageKey);
            return;
        }

        Wt::Json::Array storedFilters;
        {
            db::ISession& session{ LmsApp->getDbSession() };
            auto transaction{ session.createReadTransaction() };

            for (const Filter& filter : _filters)
            {
                if (std::optional<Wt::Json::Object> storedFilter{ toStoredFilter(session, filter.values, filter.match) })
                    storedFilters.push_back(std::move(*storedFilter));
            }
        }

        LmsApp->getWebStorage().setItem(storageKey, Wt::Json::serialize(storedFilters));
    }

    std::optional<db::FilterExpr> Filters::createDbFilter() const
    {
        std::vector<db::FilterExpr> conditions;

        for (const Filter& filter : _filters)
        {
            std::vector<db::FilterExpr> anyOf;
            for (const Value& value : filter.values)
                anyOf.push_back(std::visit([](auto id) { return db::FilterExpr::equal(id); }, value));

            db::FilterExpr condition{ anyOf.size() == 1 ? std::move(anyOf.front()) : db::FilterExpr::Or(anyOf) };
            conditions.push_back(filter.match == Match::NoneOf ? db::FilterExpr::Not(condition) : std::move(condition));
        }

        if (conditions.empty())
            return std::nullopt;
        if (conditions.size() == 1)
            return std::move(conditions.front());
        return db::FilterExpr::And(conditions);
    }

    void Filters::add(db::ClusterId clusterId)
    {
        if (addFilter(Match::OneOf, { clusterId }))
            emitFilterAddedNotification();
    }

    void Filters::set(db::GenreId genreId)
    {
        replaceOneOfFilter(genreId);
    }

    void Filters::set(db::GroupingId groupingId)
    {
        replaceOneOfFilter(groupingId);
    }

    void Filters::set(db::LanguageId languageId)
    {
        replaceOneOfFilter(languageId);
    }

    void Filters::set(db::MoodId moodId)
    {
        replaceOneOfFilter(moodId);
    }

    bool Filters::addFilter(Match match, std::vector<Value> values)
    {
        if (std::any_of(std::cbegin(_filters), std::cend(_filters), [&](const Filter& filter) { return filter.match == match && filter.values == values; }))
            return false;

        auto filterWidget{ utils::createFilter(values, match) };
        if (!filterWidget)
            return false;

        Wt::WInteractWidget* widget{ _filtersContainer->addWidget(std::move(filterWidget)) };
        widget->clicked().connect([this, widget] {
            _filtersContainer->removeWidget(widget);
            std::erase_if(_filters, [widget](const Filter& filter) { return filter.widget == widget; });
            notifyUpdated();
        });

        _filters.push_back(Filter{ match, std::move(values), widget });
        return true;
    }

    void Filters::replaceOneOfFilter(Value value)
    {
        // The type of a cluster also depends on its cluster type
        assert(!std::holds_alternative<db::ClusterId>(value));

        const std::vector<Value> values{ value };
        const auto isReplaced{ [&](const Filter& filter) { return filter.match == Match::OneOf && filter.values.front().index() == value.index() && filter.values != values; } };

        for (const Filter& filter : _filters)
        {
            if (isReplaced(filter))
                _filtersContainer->removeWidget(filter.widget);
        }
        const bool removed{ std::erase_if(_filters, isReplaced) > 0 };

        if (addFilter(Match::OneOf, values))
            emitFilterAddedNotification();
        else if (removed)
            notifyUpdated();
    }

    void Filters::emitFilterAddedNotification()
    {
        LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Explore.filter-added"), std::chrono::seconds{ 2 });

        notifyUpdated();
    }

    void Filters::notifyUpdated()
    {
        _dbFilter = createDbFilter();
        store();
        _sigUpdated.emit();
    }
} // namespace lms::ui
