/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <Wt/WDateTime.h>

#include "core/media/Codec.hpp"

#include "database/objects/ClusterId.hpp"
#include "database/objects/CountryId.hpp"
#include "database/objects/GenreId.hpp"
#include "database/objects/GroupingId.hpp"
#include "database/objects/LabelId.hpp"
#include "database/objects/LanguageId.hpp"
#include "database/objects/MediaLibraryId.hpp"
#include "database/objects/MoodId.hpp"
#include "database/objects/ReleaseTypeId.hpp"
#include "database/objects/Types.hpp"

namespace lms::db
{
    enum class FilterOperator
    {
        Equal,
        Less,
        LessOrEqual,
        Greater,
        GreaterOrEqual,
        Between, // inclusive on both bounds, the operand is a FilterRange
    };

    template<typename T>
    struct FilterRange
    {
        T min;
        T max;
    };

    // A FilterRange if the operator is Between, a single value otherwise
    template<typename T>
    using FilterOperand = std::variant<T, FilterRange<T>>;

    enum class IntegerField
    {
        Year,
        OriginalYear, // falls back on Year if not set
        Bitrate,      // in bps
        SampleRate,   // in Hz
        BitsPerSample,
        ChannelCount,
        TrackNumber,
    };

    struct IntegerCondition
    {
        IntegerField field;
        FilterOperator op;
        FilterOperand<std::int64_t> value;
    };

    struct DurationCondition
    {
        FilterOperator op;
        FilterOperand<std::chrono::milliseconds> value;
    };

    struct AddedDateTimeCondition
    {
        FilterOperator op;
        FilterOperand<Wt::WDateTime> value;
    };

    struct IsCompilationCondition // release condition
    {
        bool value;
    };

    enum class StringField
    {
        TrackName,
        ReleaseName,     // release condition
        ReleaseSortName, // release condition, falls back on ReleaseName if not set
        ArtistName,      // an artist of the track or of its release (any link type), by credited or own name
        ArtistSortName,  // same, each sort name falls back on the matching name if not set
    };

    enum class StringOperator
    {
        Equal,
        Contains,
        StartsWith,
        EndsWith,
    };

    // Case insensitive (ASCII only)
    struct StringCondition
    {
        StringField field;
        StringOperator op;
        std::string value;
    };

    // Per-user conditions: evaluated against the user given alongside the expression
    struct ListenCountCondition // 0 if never listened
    {
        static constexpr bool perUser{ true };

        FilterOperator op;
        FilterOperand<std::int64_t> value;
    };

    struct LastListenDateTimeCondition
    {
        static constexpr bool perUser{ true };

        FilterOperator op;
        FilterOperand<Wt::WDateTime> value;
    };

    struct RatingCondition
    {
        static constexpr bool perUser{ true };

        FilterOperator op;
        FilterOperand<std::int64_t> value;
    };

    struct FeedbackCondition
    {
        static constexpr bool perUser{ true };

        FeedbackValue value;
    };

    // The track is linked to the given entity. Country, Label and ReleaseType are release conditions
    struct RelationCondition
    {
        std::variant<ClusterId, CountryId, GenreId, GroupingId, LabelId, LanguageId, MoodId, ReleaseTypeId> id;

        bool operator==(const RelationCondition&) const = default;
    };

    // A condition on a track. Equal-only conditions have no operator.
    // A related entity and a MediaLibraryId are conditions by themselves.
    using FilterCondition = std::variant<
        RelationCondition,
        MediaLibraryId,
        core::media::Codec,
        Advisory,
        IntegerCondition,
        DurationCondition,
        AddedDateTimeCondition,
        IsCompilationCondition,
        StringCondition,
        ListenCountCondition,
        LastListenDateTimeCondition,
        RatingCondition,
        FeedbackCondition>;

    // Track conditions apply to a single track: releases, artists and tracklists match if one of their tracks satisfies the expression.
    // Release conditions apply to the queried release itself, or to the release of the track otherwise.
    // When querying artists, artist conditions only consider the queried artist.
    // Conditions never evaluate to "unknown": a missing value does not satisfy any comparison, so Not(c) matches it.
    // A 0 Duration, Bitrate, SampleRate or ChannelCount is a missing value
    class FilterExpr
    {
    public:
        enum class Type
        {
            Condition,
            And,
            Or,
            Not,
        };

        using Condition = FilterCondition;

        // Ids must be valid, the operand must fit the operator
        static FilterExpr condition(Condition condition);

        // Same as condition(), for the conditions that do not need any operator
        static FilterExpr equal(ClusterId cluster);
        static FilterExpr equal(CountryId country);
        static FilterExpr equal(GenreId genre);
        static FilterExpr equal(GroupingId grouping);
        static FilterExpr equal(LabelId label);
        static FilterExpr equal(LanguageId language);
        static FilterExpr equal(MediaLibraryId mediaLibrary);
        static FilterExpr equal(MoodId mood);
        static FilterExpr equal(ReleaseTypeId releaseType);
        static FilterExpr equal(core::media::Codec codec);
        static FilterExpr equal(Advisory advisory);
        // children must not be empty
        static FilterExpr And(std::initializer_list<FilterExpr> children);
        static FilterExpr And(std::span<const FilterExpr> children);
        static FilterExpr Or(std::initializer_list<FilterExpr> children);
        static FilterExpr Or(std::span<const FilterExpr> children);
        static FilterExpr Not(const FilterExpr& child);

        Type getType() const { return _type; }
        const Condition& getCondition() const;                                // Type::Condition only
        std::span<const FilterExpr> getChildren() const { return _children; } // And, Or: at least one, Not: exactly one

        bool hasPerUserCondition() const;

    private:
        FilterExpr(Type type, std::optional<Condition> condition, std::vector<FilterExpr> children);

        Type _type;
        std::optional<Condition> _condition;
        std::vector<FilterExpr> _children;
    };
} // namespace lms::db
