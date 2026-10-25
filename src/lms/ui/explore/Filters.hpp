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

#pragma once

#include <optional>
#include <vector>

#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>
#include <Wt/WTemplate.h>

#include "core/media/Codec.hpp"

#include "database/objects/ClusterId.hpp"
#include "database/objects/FilterExpr.hpp"
#include "database/objects/GenreId.hpp"
#include "database/objects/GroupingId.hpp"
#include "database/objects/LabelId.hpp"
#include "database/objects/LanguageId.hpp"
#include "database/objects/MediaLibraryId.hpp"
#include "database/objects/MoodId.hpp"
#include "database/objects/ReleaseTypeId.hpp"

namespace lms::ui
{
    class Filters : public Wt::WTemplate
    {
    public:
        Filters();

        const std::optional<db::FilterExpr>& getDbFilter() const { return _dbFilter; }

        void add(db::ClusterId clusterId);
        void set(db::GenreId genreId);
        void set(db::GroupingId groupingId);
        void set(db::LanguageId languageId);
        void set(db::MoodId moodId);

        Wt::Signal<>& updated() { return _sigUpdated; }

    private:
        void showDialog();
        void set(db::LabelId labelId);
        void set(db::MediaLibraryId mediaLibraryId);
        void set(db::ReleaseTypeId releaseTypeId);
        void set(core::media::Codec codec);
        void emitFilterAddedNotification();
        void notifyUpdated();
        std::optional<db::FilterExpr> createDbFilter() const;

        Wt::WContainerWidget* _filters{};
        Wt::WInteractWidget* _mediaLibraryFilter{};
        Wt::WInteractWidget* _labelFilter{};
        Wt::WInteractWidget* _releaseTypeFilter{};
        Wt::WInteractWidget* _codecFilter{};
        Wt::WInteractWidget* _genreFilter{};
        Wt::WInteractWidget* _groupingFilter{};
        Wt::WInteractWidget* _languageFilter{};
        Wt::WInteractWidget* _moodFilter{};

        Wt::Signal<> _sigUpdated;

        struct State
        {
            db::MediaLibraryId mediaLibrary;
            std::vector<db::ClusterId> clusters;
            db::GenreId genre;
            db::GroupingId grouping;
            db::LabelId label;
            db::LanguageId language;
            db::MoodId mood;
            db::ReleaseTypeId releaseType;
            std::optional<core::media::Codec> codec;
        };
        State _state;
        std::optional<db::FilterExpr> _dbFilter; // built from _state on each update, used by many queries
    };
} // namespace lms::ui
