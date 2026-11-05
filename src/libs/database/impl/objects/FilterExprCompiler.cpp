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

#include "FilterExprCompiler.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include "core/TaggedType.hpp"
#include "core/Utils.hpp"
#include "database/Types.hpp"

#include "objects/detail/Types.hpp"

namespace lms::db
{
    namespace
    {
        using IsUnderNot = core::TaggedBool<struct IsUnderNotTag>;

        constexpr std::string_view trackAlias{ "t" };

        // A many-to-many relation of a track, or of its release
        struct Relation
        {
            std::string_view trackColumn; // "id", or "release_id" for a relation of the release
            std::string_view table;
            std::string_view foreignKey; // refers to trackColumn
            std::string_view valueColumn;
        };

        template<typename Id>
        struct RelationTraits
        {
        };

        template<>
        struct RelationTraits<ClusterId>
        {
            static constexpr Relation relation{ "id", "track_cluster", "track_id", "cluster_id" };
        };

        template<>
        struct RelationTraits<CountryId>
        {
            static constexpr Relation relation{ "release_id", "release_country", "release_id", "country_id" };
        };

        template<>
        struct RelationTraits<GenreId>
        {
            static constexpr Relation relation{ "id", "track_genre", "track_id", "genre_id" };
        };

        template<>
        struct RelationTraits<GroupingId>
        {
            static constexpr Relation relation{ "id", "track_grouping", "track_id", "grouping_id" };
        };

        template<>
        struct RelationTraits<LanguageId>
        {
            static constexpr Relation relation{ "id", "track_language", "track_id", "language_id" };
        };

        template<>
        struct RelationTraits<MoodId>
        {
            static constexpr Relation relation{ "id", "track_mood", "track_id", "mood_id" };
        };

        template<>
        struct RelationTraits<LabelId>
        {
            static constexpr Relation relation{ "release_id", "release_label", "release_id", "label_id" };
        };

        template<>
        struct RelationTraits<ReleaseTypeId>
        {
            static constexpr Relation relation{ "release_id", "release_release_type", "release_id", "release_type_id" };
        };

        constexpr Relation getRelation(const RelationCondition& condition)
        {
            return std::visit([](const auto& id) { return RelationTraits<std::decay_t<decltype(id)>>::relation; }, condition.id);
        }

        // "t.column"
        std::string createQualifiedTrackColumn(std::string_view column)
        {
            std::string res;
            res.append(trackAlias).append(".").append(column);
            return res;
        }

        // Unknown values are stored as 0 in some columns
        std::string createNullIfZeroTrackColumn(std::string_view column)
        {
            return "NULLIF(" + createQualifiedTrackColumn(column) + ", 0)";
        }

        std::string createIntegerExpr(IntegerField field)
        {
            switch (field)
            {
            case IntegerField::Year:
                return "CAST(SUBSTR(" + createQualifiedTrackColumn("date") + ", 1, 4) AS INTEGER)";
            case IntegerField::OriginalYear:
                return "CAST(SUBSTR(COALESCE(" + createQualifiedTrackColumn("original_date") + ", " + createQualifiedTrackColumn("date") + "), 1, 4) AS INTEGER)";
            case IntegerField::Bitrate:
                return createNullIfZeroTrackColumn("bitrate");
            case IntegerField::SampleRate:
                return createNullIfZeroTrackColumn("sample_rate");
            case IntegerField::ChannelCount:
                return createNullIfZeroTrackColumn("channel_count");
            case IntegerField::BitsPerSample:
                return createQualifiedTrackColumn("bits_per_sample");
            case IntegerField::TrackNumber:
                return createQualifiedTrackColumn("track_number");
            }

            throw Exception{ "Unhandled integer filter field" };
        }

        // Index lookups: SQLite re-evaluated a join on aggregated listens for each row (seconds on 20k tracks)
        std::string createListenCountExpr()
        {
            return "(SELECT COUNT(*) FROM listen f_l WHERE f_l.user_id = ? AND f_l.track_id = " + createQualifiedTrackColumn("id") + ")";
        }

        std::string createLastListenDateTimeExpr()
        {
            return "(SELECT MAX(f_l.date_time) FROM listen f_l WHERE f_l.user_id = ? AND f_l.track_id = " + createQualifiedTrackColumn("id") + ")";
        }

        template<typename T>
        BindValue toBindValue(const T& value)
        {
            if constexpr (std::is_same_v<T, std::int64_t>)
                return BindValue{ std::in_place_type<long long>, value };
            else if constexpr (std::is_same_v<T, std::chrono::milliseconds>)
                return BindValue{ std::in_place_type<long long>, value.count() };
            else if constexpr (std::is_same_v<T, core::media::Codec>)
                return BindValue{ std::in_place_type<int>, static_cast<int>(detail::getDbCodec(value)) };
            else if constexpr (std::is_same_v<T, FeedbackValue> || std::is_same_v<T, Advisory>)
                return BindValue{ std::in_place_type<int>, static_cast<int>(value) };
            else if constexpr (std::is_base_of_v<IdType, T>)
                return BindValue{ std::in_place_type<long long>, value.getValue() };
            else if constexpr (std::is_same_v<T, bool>)
                return BindValue{ std::in_place_type<bool>, value };
            else if constexpr (std::is_same_v<T, Wt::WDateTime>)
                return BindValue{ std::in_place_type<Wt::WDateTime>, value };
            else
                static_assert(!sizeof(T), "Unsupported filter value type");
        }

        BindValue toBindValue(const RelationCondition& condition)
        {
            return std::visit([](const auto& id) { return toBindValue(id); }, condition.id);
        }

        // The values of an operand, after the given ones
        template<typename T>
        std::vector<BindValue> createOperandBinds(const FilterOperand<T>& operand, std::vector<BindValue> binds = {})
        {
            if (const FilterRange<T>* range{ std::get_if<FilterRange<T>>(&operand) })
            {
                binds.push_back(toBindValue(range->min));
                binds.push_back(toBindValue(range->max));
            }
            else
            {
                binds.push_back(toBindValue(std::get<T>(operand)));
            }

            return binds;
        }

        // "= ?", "< ?", ..., "BETWEEN ? AND ?"
        std::string_view getSqlComparison(FilterOperator op)
        {
            switch (op)
            {
            case FilterOperator::Equal:
                return "= ?";
            case FilterOperator::Less:
                return "< ?";
            case FilterOperator::LessOrEqual:
                return "<= ?";
            case FilterOperator::Greater:
                return "> ?";
            case FilterOperator::GreaterOrEqual:
                return ">= ?";
            case FilterOperator::Between:
                return "BETWEEN ? AND ?";
            }

            throw Exception{ "Unhandled filter operator" };
        }

        // Unknown (missing value) already rejects the row except under NOT: the wrapper is added only there to keep indexes usable.
        // Optim: CASE WHEN rather than COALESCE(..., 0) or a bare NOT
        std::string createComparison(std::string_view valueExpr, std::string_view comparison, IsUnderNot isUnderNot)
        {
            std::string res;
            if (isUnderNot.value())
                res += "CASE WHEN ";
            res.append(valueExpr).append(" ").append(comparison);
            if (isUnderNot.value())
                res += " THEN 1 ELSE 0 END";

            return res;
        }

        std::string createComparison(std::string_view valueExpr, FilterOperator op, IsUnderNot isUnderNot)
        {
            return createComparison(valueExpr, getSqlComparison(op), isUnderNot);
        }

        // Each condition is a comparison on the track row or a membership test, so that the expression needs no join
        class Compiler
        {
        public:
            Compiler(UserId user)
                : _user{ user }
            {
            }

            SqlFragment compile(const FilterExpr& expr) const
            {
                assert(_user.isValid() || !expr.hasPerUserCondition());

                SqlFragment where;
                compileExpr(expr, IsUnderNot{ false }, where);
                return where;
            }

        private:
            void compileExpr(const FilterExpr& expr, IsUnderNot isUnderNot, SqlFragment& fragment) const
            {
                switch (expr.getType())
                {
                case FilterExpr::Type::Condition:
                    compileCondition(expr.getCondition(), isUnderNot, fragment);
                    return;

                case FilterExpr::Type::Not:
                    fragment.append("NOT (");
                    compileExpr(expr.getChildren().front(), IsUnderNot{ true }, fragment);
                    fragment.append(")");
                    return;

                case FilterExpr::Type::And:
                case FilterExpr::Type::Or:
                    {
                        const bool isOr{ expr.getType() == FilterExpr::Type::Or };

                        fragment.append("(");
                        if (isOr)
                            compileOrChildren(expr.getChildren(), isUnderNot, fragment);
                        else
                            compileChildren(expr.getChildren(), " AND ", isUnderNot, fragment);
                        fragment.append(")");
                        return;
                    }
                }

                throw Exception{ "Unhandled filter expression type" };
            }

            void compileChildren(std::span<const FilterExpr> children, std::string_view separator, IsUnderNot isUnderNot, SqlFragment& fragment) const
            {
                bool first{ true };
                for (const FilterExpr& child : children)
                {
                    if (!first)
                        fragment.append(separator);
                    compileExpr(child, isUnderNot, fragment);
                    first = false;
                }
            }

            void compileOrChildren(std::span<const FilterExpr> children, IsUnderNot isUnderNot, SqlFragment& fragment) const
            {
                std::vector<bool> merged(children.size(), false);

                bool first{ true };
                for (std::size_t i{}; i < children.size(); ++i)
                {
                    if (merged[i])
                        continue;

                    if (!first)
                        fragment.append(" OR ");
                    first = false;

                    const RelationCondition* relationCondition{ getMergeableRelation(children[i]) };
                    if (!relationCondition)
                    {
                        compileExpr(children[i], isUnderNot, fragment);
                        continue;
                    }

                    std::vector<const RelationCondition*> sameRelations{ relationCondition };
                    for (std::size_t j{ i + 1 }; j < children.size(); ++j)
                    {
                        const RelationCondition* sibling{ getMergeableRelation(children[j]) };
                        if (sibling && sibling->id.index() == relationCondition->id.index())
                        {
                            sameRelations.push_back(sibling);
                            merged[j] = true;
                        }
                    }

                    compileRelationMembership(sameRelations, isUnderNot, fragment);
                }
            }

            static const RelationCondition* getMergeableRelation(const FilterExpr& expr)
            {
                if (expr.getType() != FilterExpr::Type::Condition)
                    return nullptr;

                return std::get_if<RelationCondition>(&expr.getCondition());
            }

            void compileRelationMembership(const RelationCondition& relationCondition, IsUnderNot isUnderNot, SqlFragment& fragment) const
            {
                const RelationCondition* const relationConditionPtr{ &relationCondition };
                compileRelationMembership(std::span{ &relationConditionPtr, 1 }, isUnderNot, fragment);
            }

            // True if the entity is linked to any of the given items, which must all be of the same type.
            // Not an EXISTS correlated with the row: SQLite drives from the relation index instead of probing it for every row
            void compileRelationMembership(std::span<const RelationCondition* const> relationConditions, IsUnderNot isUnderNot, SqlFragment& fragment) const
            {
                assert(!relationConditions.empty());

                const Relation relation{ getRelation(*relationConditions.front()) };

                // Under NOT, the release of a track may be missing, and NULL IN (...) is unknown (which NOT keeps out) instead of false.
                // Optim: CASE WHEN rather than COALESCE(..., 0) or a bare NOT, even where the id cannot be NULL
                const bool needsCase{ isUnderNot.value() };

                std::string sql;
                if (needsCase)
                    sql += "CASE WHEN ";
                sql.append(createQualifiedTrackColumn(relation.trackColumn)).append(" IN (SELECT f_e.").append(relation.foreignKey).append(" FROM ").append(relation.table).append(" f_e WHERE f_e.").append(relation.valueColumn);
                if (relationConditions.size() == 1)
                {
                    sql.append(" = ?");
                }
                else
                {
                    sql.append(" IN (?");
                    for (std::size_t i{ 1 }; i < relationConditions.size(); ++i)
                        sql.append(",?");
                    sql.append(")");
                }
                sql.append(")");
                if (needsCase)
                    sql += " THEN 1 ELSE 0 END";

                std::vector<BindValue> binds;
                binds.reserve(relationConditions.size());
                for (const RelationCondition* relationCondition : relationConditions)
                {
                    assert(relationCondition->id.index() == relationConditions.front()->id.index());
                    binds.push_back(toBindValue(*relationCondition));
                }

                fragment.append(sql, std::move(binds));
            }

            // One lambda per alternative of FilterExpr::Condition: the compiler rejects a forgotten one
            void compileCondition(const FilterExpr::Condition& condition, IsUnderNot isUnderNot, SqlFragment& fragment) const
            {
                std::visit(core::utils::overloads{
                               [&](const RelationCondition& relationCondition) {
                                   compileRelationMembership(relationCondition, isUnderNot, fragment);
                               },
                               [&](const MediaLibraryId& mediaLibrary) {
                                   fragment.append(createComparison(createQualifiedTrackColumn("media_library_id"), FilterOperator::Equal, isUnderNot), { toBindValue(mediaLibrary) });
                               },
                               [&](const core::media::Codec& codec) {
                                   fragment.append(createComparison(createQualifiedTrackColumn("codec"), FilterOperator::Equal, isUnderNot), { toBindValue(codec) });
                               },
                               [&](const Advisory& advisory) {
                                   fragment.append(createComparison(createQualifiedTrackColumn("advisory"), FilterOperator::Equal, isUnderNot), { toBindValue(advisory) });
                               },
                               [&](const IntegerCondition& integerCondition) {
                                   fragment.append(createComparison(createIntegerExpr(integerCondition.field), integerCondition.op, isUnderNot), createOperandBinds(integerCondition.value));
                               },
                               [&](const DurationCondition& durationCondition) {
                                   fragment.append(createComparison(createNullIfZeroTrackColumn("duration"), durationCondition.op, isUnderNot), createOperandBinds(durationCondition.value));
                               },
                               [&](const AddedDateTimeCondition& addedDateTimeCondition) {
                                   fragment.append(createComparison(createQualifiedTrackColumn("file_added"), addedDateTimeCondition.op, isUnderNot), createOperandBinds(addedDateTimeCondition.value));
                               },
                               [&](const IsCompilationCondition& isCompilationCondition) {
                                   // a membership like the other release conditions, under NOT the release of a track may be missing as well
                                   fragment.append(createComparison(createQualifiedTrackColumn("release_id"), "IN (SELECT f_r.id FROM release f_r WHERE f_r.is_compilation = ?)", isUnderNot), { toBindValue(isCompilationCondition.value) });
                               },
                               [&](const ListenCountCondition& listenCountCondition) {
                                   fragment.append(createComparison(createListenCountExpr(), listenCountCondition.op, isUnderNot), createOperandBinds(listenCountCondition.value, { toBindValue(_user) }));
                               },
                               [&](const LastListenDateTimeCondition& lastListenDateTimeCondition) {
                                   fragment.append(createComparison(createLastListenDateTimeExpr(), lastListenDateTimeCondition.op, isUnderNot), createOperandBinds(lastListenDateTimeCondition.value, { toBindValue(_user) }));
                               },
                               [&](const RatingCondition& ratingCondition) {
                                   const std::string membership{ "IN (SELECT f_rt.track_id FROM rated_track f_rt WHERE f_rt.user_id = ? AND f_rt.rating " + std::string{ getSqlComparison(ratingCondition.op) } + ")" };
                                   fragment.append(createComparison(createQualifiedTrackColumn("id"), membership, isUnderNot), createOperandBinds(ratingCondition.value, { toBindValue(_user) }));
                               },
                               [&](const FeedbackCondition& feedbackCondition) {
                                   fragment.append(createComparison(createQualifiedTrackColumn("id"), "IN (SELECT f_tf.track_id FROM track_feedback f_tf WHERE f_tf.user_id = ? AND f_tf.value = ?)", isUnderNot), { toBindValue(_user), toBindValue(feedbackCondition.value) });
                               } },
                           condition);
            }

            UserId _user;
        };
    } // namespace

    SqlFragment compileFilterExpr(const FilterExpr& expr, UserId user)
    {
        return Compiler{ user }.compile(expr);
    }

    SqlFragment createMatchingTracksCondition(std::string_view column, const FilterExpr& expr, UserId user, std::string_view trackColumn)
    {
        SqlFragment res{ std::string{ column } + " IN (SELECT " + std::string{ trackColumn } + " FROM track t WHERE " };
        res.append(compileFilterExpr(expr, user));
        res.append(")");

        return res;
    }
} // namespace lms::db
