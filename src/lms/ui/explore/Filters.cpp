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

#include "core/Utils.hpp"

#include "core/media/Codec.hpp"
#include "database/Session.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/LabelId.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Mood.hpp"
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
                db::ClusterType::find(LmsApp->getDbSession(), [&](const db::ClusterType::pointer& clusterType) {
                    typeModel->add(Wt::WString::fromUTF8(std::string{ clusterType->getName() }), clusterType->getId());
                });
            }

            typeModel->add(Wt::WString::tr("Lms.Explore.media-library"), MediaLibraryTag{});
            typeModel->add(Wt::WString::tr("Lms.Explore.label"), LabelTag{});
            typeModel->add(Wt::WString::tr("Lms.Explore.release-type"), ReleaseTypeTag{});
            typeModel->add(Wt::WString::tr("Lms.Explore.codec"), CodecTag{});

            return typeModel;
        }

        using ValueVariant = std::variant<db::ClusterId, db::MediaLibraryId, db::LabelId, db::ReleaseTypeId, core::media::Codec, db::GenreId, db::GroupingId, db::LanguageId, db::MoodId>;
        using ValueModel = ValueStringModel<ValueVariant>;

        std::unique_ptr<ValueModel> createValueModel(TypeVariant type)
        {
            db::Session& session{ LmsApp->getDbSession() };

            auto valueModel{ std::make_unique<ValueModel>() };

            auto transaction{ session.createReadTransaction() };

            std::visit(core::utils::overloads{
                           [&](const MediaLibraryTag&) {
                               db::MediaLibrary::find(session, [&](const db::MediaLibrary::pointer& library) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ library->getName() }), library->getId());
                               });
                           },
                           [&](const LabelTag&) {
                               db::Label::find(session, db::LabelSortMethod::Name, [&](const db::Label::pointer& label) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ label->getName() }), label->getId());
                               });
                           },
                           [&](const ReleaseTypeTag&) {
                               db::ReleaseType::find(session, db::ReleaseTypeSortMethod::Name, [&](const db::ReleaseType::pointer& releaseType) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ releaseType->getName() }), releaseType->getId());
                               });
                           },
                           [&](const CodecTag&) {
                               core::media::visitCodecs([&](const core::media::CodecDesc& desc) {
                                   valueModel->add(Wt::WString::fromUTF8(desc.longName.c_str()), desc.type);
                               });
                           },
                           [&](const GenreTag&) {
                               db::Genre::find(session, db::Genre::FindParameters{}.setSortMethod(db::GenreSortMethod::Name), [&](const db::Genre::pointer& genre) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ genre->getName() }), genre->getId());
                               });
                           },
                           [&](const GroupingTag&) {
                               db::Grouping::find(session, db::Grouping::FindParameters{}.setSortMethod(db::GroupingSortMethod::Name), [&](const db::Grouping::pointer& grouping) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ grouping->getName() }), grouping->getId());
                               });
                           },
                           [&](const LanguageTag&) {
                               db::Language::find(session, db::Language::FindParameters{}.setSortMethod(db::LanguageSortMethod::Name), [&](const db::Language::pointer& language) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ language->getName() }), language->getId());
                               });
                           },
                           [&](const MoodTag&) {
                               db::Mood::find(session, db::Mood::FindParameters{}.setSortMethod(db::MoodSortMethod::Name), [&](const db::Mood::pointer& mood) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ mood->getName() }), mood->getId());
                               });
                           },
                           [&](db::ClusterTypeId clusterTypeId) {
                               db::Cluster::FindParameters params;
                               params.setClusterType(clusterTypeId);
                               params.setSortMethod(db::ClusterSortMethod::Name);

                               db::Cluster::find(session, params, [&](const db::Cluster::pointer& cluster) {
                                   valueModel->add(Wt::WString::fromUTF8(std::string{ cluster->getName() }), cluster->getId());
                               });
                           } },
                       type);
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

            std::visit(core::utils::overloads{
                           [&](db::MediaLibraryId mediaLibraryId) {
                               set(mediaLibraryId);
                           },
                           [&](db::LabelId labelId) {
                               set(labelId);
                           },
                           [&](db::ReleaseTypeId releaseTypeId) {
                               set(releaseTypeId);
                           },
                           [&](db::ClusterId clusterId) {
                               add(clusterId);
                           },
                           [&](core::media::Codec codec) {
                               set(codec);
                           },
                           [&](db::GenreId genreId) {
                               set(genreId);
                           },
                           [&](db::GroupingId groupingId) {
                               set(groupingId);
                           },
                           [&](db::LanguageId languageId) {
                               set(languageId);
                           },
                           [&](db::MoodId moodId) {
                               set(moodId);
                           },
                       },
                       value);

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
        if (const std::optional<core::media::Codec> codec{ state::readValue<core::media::Codec>("filters_codec") })
            set(*codec);
        if (const std::optional<db::GenreId::ValueType> genreId{ state::readValue<db::GenreId::ValueType>("filters_genre_id") })
            set(db::GenreId{ *genreId });
        if (const std::optional<db::GroupingId::ValueType> groupingId{ state::readValue<db::GroupingId::ValueType>("filters_grouping_id") })
            set(db::GroupingId{ *groupingId });
        if (const std::optional<db::LanguageId::ValueType> languageId{ state::readValue<db::LanguageId::ValueType>("filters_language_id") })
            set(db::LanguageId{ *languageId });
        if (const std::optional<db::MoodId::ValueType> moodId{ state::readValue<db::MoodId::ValueType>("filters_mood_id") })
            set(db::MoodId{ *moodId });
    }

    std::optional<db::FilterExpr> Filters::createDbFilter() const
    {
        std::vector<db::FilterExpr> conditions;

        if (_state.mediaLibrary.isValid())
            conditions.push_back(db::FilterExpr::equal(_state.mediaLibrary));
        for (const db::ClusterId clusterId : _state.clusters)
            conditions.push_back(db::FilterExpr::equal(clusterId));
        if (_state.genre.isValid())
            conditions.push_back(db::FilterExpr::equal(_state.genre));
        if (_state.grouping.isValid())
            conditions.push_back(db::FilterExpr::equal(_state.grouping));
        if (_state.label.isValid())
            conditions.push_back(db::FilterExpr::equal(_state.label));
        if (_state.language.isValid())
            conditions.push_back(db::FilterExpr::equal(_state.language));
        if (_state.mood.isValid())
            conditions.push_back(db::FilterExpr::equal(_state.mood));
        if (_state.releaseType.isValid())
            conditions.push_back(db::FilterExpr::equal(_state.releaseType));
        if (_state.codec)
            conditions.push_back(db::FilterExpr::equal(*_state.codec));

        if (conditions.empty())
            return std::nullopt;
        if (conditions.size() == 1)
            return std::move(conditions.front());
        return db::FilterExpr::And(conditions);
    }

    void Filters::add(db::ClusterId clusterId)
    {
        if (std::find(std::cbegin(_state.clusters), std::cend(_state.clusters), clusterId) != std::cend(_state.clusters))
            return;

        Wt::WInteractWidget* filter{};

        {
            auto cluster{ utils::createFilterCluster(clusterId, true) };
            if (!cluster)
                return;

            filter = _filters->addWidget(std::move(cluster));
        }

        _state.clusters.push_back(clusterId);

        filter->clicked().connect([this, filter, clusterId] {
            _filters->removeWidget(filter);
            _state.clusters.erase(std::remove_if(std::begin(_state.clusters), std::end(_state.clusters), [clusterId](db::ClusterId id) { return id == clusterId; }), std::end(_state.clusters));
            notifyUpdated();
        });

        emitFilterAddedNotification();
    }

    void Filters::set(db::GenreId genreId)
    {
        if (_state.genre == genreId)
            return;

        if (_genreFilter)
        {
            _filters->removeWidget(_genreFilter);
            _genreFilter = nullptr;
            _state.genre = db::GenreId{};
        }

        auto genre{ utils::createFilterGenre(genreId, true) };
        if (!genre)
        {
            notifyUpdated();
            return;
        }

        _state.genre = genreId;
        state::writeValue<db::GenreId::ValueType>("filters_genre_id", genreId.getValue());

        _genreFilter = _filters->addWidget(std::move(genre));
        _genreFilter->clicked().connect([this] {
            _filters->removeWidget(_genreFilter);
            _genreFilter = nullptr;
            _state.genre = db::GenreId{};
            state::writeValue<db::GenreId::ValueType>("filters_genre_id", std::nullopt);
            notifyUpdated();
        });

        emitFilterAddedNotification();
    }

    void Filters::set(db::GroupingId groupingId)
    {
        if (_state.grouping == groupingId)
            return;

        if (_groupingFilter)
        {
            _filters->removeWidget(_groupingFilter);
            _groupingFilter = nullptr;
            _state.grouping = db::GroupingId{};
        }

        auto grouping{ utils::createFilterGrouping(groupingId, true) };
        if (!grouping)
        {
            notifyUpdated();
            return;
        }

        _state.grouping = groupingId;
        state::writeValue<db::GroupingId::ValueType>("filters_grouping_id", groupingId.getValue());

        _groupingFilter = _filters->addWidget(std::move(grouping));
        _groupingFilter->clicked().connect([this] {
            _filters->removeWidget(_groupingFilter);
            _groupingFilter = nullptr;
            _state.grouping = db::GroupingId{};
            state::writeValue<db::GroupingId::ValueType>("filters_grouping_id", std::nullopt);
            notifyUpdated();
        });

        emitFilterAddedNotification();
    }

    void Filters::set(db::LanguageId languageId)
    {
        if (_state.language == languageId)
            return;

        if (_languageFilter)
        {
            _filters->removeWidget(_languageFilter);
            _languageFilter = nullptr;
            _state.language = db::LanguageId{};
        }

        auto language{ utils::createFilterLanguage(languageId, true) };
        if (!language)
        {
            notifyUpdated();
            return;
        }

        _state.language = languageId;
        state::writeValue<db::LanguageId::ValueType>("filters_language_id", languageId.getValue());

        _languageFilter = _filters->addWidget(std::move(language));
        _languageFilter->clicked().connect([this] {
            _filters->removeWidget(_languageFilter);
            _languageFilter = nullptr;
            _state.language = db::LanguageId{};
            state::writeValue<db::LanguageId::ValueType>("filters_language_id", std::nullopt);
            notifyUpdated();
        });

        emitFilterAddedNotification();
    }

    void Filters::set(db::MoodId moodId)
    {
        if (_state.mood == moodId)
            return;

        if (_moodFilter)
        {
            _filters->removeWidget(_moodFilter);
            _moodFilter = nullptr;
            _state.mood = db::MoodId{};
        }

        auto mood{ utils::createFilterMood(moodId, true) };
        if (!mood)
        {
            notifyUpdated();
            return;
        }

        _state.mood = moodId;
        state::writeValue<db::MoodId::ValueType>("filters_mood_id", moodId.getValue());

        _moodFilter = _filters->addWidget(std::move(mood));
        _moodFilter->clicked().connect([this] {
            _filters->removeWidget(_moodFilter);
            _moodFilter = nullptr;
            _state.mood = db::MoodId{};
            state::writeValue<db::MoodId::ValueType>("filters_mood_id", std::nullopt);
            notifyUpdated();
        });

        emitFilterAddedNotification();
    }

    void Filters::set(db::MediaLibraryId mediaLibraryId)
    {
        if (_mediaLibraryFilter)
        {
            _filters->removeWidget(_mediaLibraryFilter);
            _mediaLibraryFilter = nullptr;
            _state.mediaLibrary = db::MediaLibraryId{};
        }

        std::string libraryName;
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const auto library{ db::MediaLibrary::find(LmsApp->getDbSession(), mediaLibraryId) };
            if (!library)
            {
                notifyUpdated();
                return;
            }

            libraryName = library->getName();
        }

        _state.mediaLibrary = mediaLibraryId;
        state::writeValue<db::MediaLibraryId::ValueType>("filters_media_library_id", mediaLibraryId.getValue());

        _mediaLibraryFilter = _filters->addWidget(utils::createFilter(Wt::WString::fromUTF8(libraryName), Wt::WString::tr("Lms.Explore.media-library"), "bg-primary", true));
        _mediaLibraryFilter->clicked().connect(_mediaLibraryFilter, [this] {
            _filters->removeWidget(_mediaLibraryFilter);
            _state.mediaLibrary = db::MediaLibraryId{};
            _mediaLibraryFilter = nullptr;
            notifyUpdated();
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
            _state.label = db::LabelId{};
        }

        std::string name;
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const auto label{ db::Label::find(LmsApp->getDbSession(), labelId) };
            if (!label)
            {
                notifyUpdated();
                return;
            }

            name = label->getName();
        }

        _state.label = labelId;
        state::writeValue<db::LabelId::ValueType>("filters_label_id", labelId.getValue());

        _labelFilter = _filters->addWidget(utils::createFilter(Wt::WString::fromUTF8(name), Wt::WString::tr("Lms.Explore.label"), "bg-secondary", true));
        _labelFilter->clicked().connect(_labelFilter, [this] {
            _filters->removeWidget(_labelFilter);
            _state.label = db::LabelId{};
            _labelFilter = nullptr;
            notifyUpdated();
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
            _state.releaseType = db::ReleaseTypeId{};
        }

        std::string name;
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const auto releaseType{ db::ReleaseType::find(LmsApp->getDbSession(), releaseTypeId) };
            if (!releaseType)
            {
                notifyUpdated();
                return;
            }

            name = releaseType->getName();
        }

        _state.releaseType = releaseTypeId;
        state::writeValue<db::ReleaseTypeId::ValueType>("filters_release_type_id", releaseTypeId.getValue());

        _releaseTypeFilter = _filters->addWidget(utils::createFilter(Wt::WString::fromUTF8(name), Wt::WString::tr("Lms.Explore.release-type"), "bg-dark", true));
        _releaseTypeFilter->clicked().connect(_releaseTypeFilter, [this] {
            _filters->removeWidget(_releaseTypeFilter);
            _state.releaseType = db::ReleaseTypeId{};
            _releaseTypeFilter = nullptr;
            notifyUpdated();
            state::writeValue<db::LabelId::ValueType>("filters_release_type_id", std::nullopt);
        });

        emitFilterAddedNotification();
    }

    void Filters::set(core::media::Codec codec)
    {
        if (_codecFilter)
        {
            _filters->removeWidget(_codecFilter);
            _codecFilter = nullptr;
            _state.codec.reset();
        }

        _state.codec = codec;
        state::writeValue<core::media::Codec>("filters_codec", codec);

        _codecFilter = _filters->addWidget(utils::createFilter(Wt::WString::fromUTF8(core::media::getCodecDesc(codec).longName.c_str()), Wt::WString::tr("Lms.Explore.codec"), "bg-success", true));
        _codecFilter->clicked().connect(_codecFilter, [this] {
            _filters->removeWidget(_codecFilter);
            _state.codec.reset();
            _codecFilter = nullptr;
            notifyUpdated();
            state::writeValue<db::LabelId::ValueType>("filters_codec", std::nullopt);
        });

        emitFilterAddedNotification();
    }

    void Filters::emitFilterAddedNotification()
    {
        LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Explore.filter-added"), std::chrono::seconds{ 2 });

        notifyUpdated();
    }

    void Filters::notifyUpdated()
    {
        _dbFilter = createDbFilter();
        _sigUpdated.emit();
    }
} // namespace lms::ui
