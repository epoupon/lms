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
#include <string>
#include <variant>
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
    // The filter bar is an AND of filters. A filter matches the tracks that have one of its values, or none of them
    class Filters : public Wt::WTemplate
    {
    public:
        using Value = std::variant<db::ClusterId, db::MediaLibraryId, db::LabelId, db::ReleaseTypeId, core::media::Codec, db::GenreId, db::GroupingId, db::LanguageId, db::MoodId>;
        enum class Match
        {
            OneOf,
            NoneOf,
        };

        Filters();

        const std::optional<db::FilterExpr>& getDbFilter() const { return _dbFilter; }

        void add(db::ClusterId clusterId);
        // Replace the "one of" filters of the same type, "none of" filters stay
        void set(db::GenreId genreId);
        void set(db::GroupingId groupingId);
        void set(db::LanguageId languageId);
        void set(db::MoodId moodId);

        Wt::Signal<>& updated() { return _sigUpdated; }

    private:
        struct Filter
        {
            Match match{};
            std::vector<Value> values; // not empty, all of the same type (and of the same cluster type)
            Wt::WInteractWidget* widget{};
        };

        void showDialog();
        void restore(const std::string& storedFilters);
        void store() const;
        // false if the same filter already exists, or if one of its values no longer exists
        bool addFilter(Match match, std::vector<Value> values);
        void replaceOneOfFilter(Value value);
        void emitFilterAddedNotification();
        void notifyUpdated();
        std::optional<db::FilterExpr> createDbFilter() const;

        Wt::WContainerWidget* _filtersContainer{};
        std::vector<Filter> _filters;

        Wt::Signal<> _sigUpdated;
        std::optional<db::FilterExpr> _dbFilter; // built from _filters on each update, used by many queries
    };
} // namespace lms::ui
