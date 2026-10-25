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

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include "core/TaggedType.hpp"
#include "core/Utils.hpp"

#include "Utils.hpp"
#include "objects/detail/Types.hpp"

namespace lms::db
{
    namespace
    {
        using IsConjunct = core::TaggedBool<struct IsConjunctTag>;
        using IsUnderNot = core::TaggedBool<struct IsUnderNotTag>;

        // Aliases of the queried entity (and of its tracks) given by the queries
        constexpr std::string_view trackAlias{ "t" };
        constexpr std::string_view releaseAlias{ "r" };
        constexpr std::string_view artistAlias{ "a" };

        // Fixed aliases of the joins registered for some conditions
        constexpr std::string_view releaseJoinAlias{ "f_rel" };
        constexpr std::string_view ratedTrackJoinAlias{ "f_rt" };
        constexpr std::string_view trackFeedbackJoinAlias{ "f_tf" };

        // A many-to-many relation of an entity
        struct Relation
        {
            FilterEntity entity;
            std::string_view table;
            std::string_view foreignKey; // refers to the id of the entity
            std::string_view valueColumn;
        };

        template<typename Id>
        struct RelationTraits
        {
        };

        template<>
        struct RelationTraits<ClusterId>
        {
            static constexpr Relation relation{ FilterEntity::Track, "track_cluster", "track_id", "cluster_id" };
        };

        template<>
        struct RelationTraits<CountryId>
        {
            static constexpr Relation relation{ FilterEntity::Release, "release_country", "release_id", "country_id" };
        };

        template<>
        struct RelationTraits<GenreId>
        {
            static constexpr Relation relation{ FilterEntity::Track, "track_genre", "track_id", "genre_id" };
        };

        template<>
        struct RelationTraits<GroupingId>
        {
            static constexpr Relation relation{ FilterEntity::Track, "track_grouping", "track_id", "grouping_id" };
        };

        template<>
        struct RelationTraits<LanguageId>
        {
            static constexpr Relation relation{ FilterEntity::Track, "track_language", "track_id", "language_id" };
        };

        template<>
        struct RelationTraits<MoodId>
        {
            static constexpr Relation relation{ FilterEntity::Track, "track_mood", "track_id", "mood_id" };
        };

        template<>
        struct RelationTraits<LabelId>
        {
            static constexpr Relation relation{ FilterEntity::Release, "release_label", "release_id", "label_id" };
        };

        template<>
        struct RelationTraits<ReleaseTypeId>
        {
            static constexpr Relation relation{ FilterEntity::Release, "release_release_type", "release_id", "release_type_id" };
        };

        constexpr Relation getRelation(const RelationCondition& condition)
        {
            return std::visit([](const auto& id) { return RelationTraits<std::decay_t<decltype(id)>>::relation; }, condition.id);
        }

        // "alias.column"
        std::string createQualifiedColumn(std::string_view alias, std::string_view column)
        {
            std::string res;
            res.append(alias).append(".").append(column);
            return res;
        }

        std::string createQualifiedTrackColumn(std::string_view column)
        {
            return createQualifiedColumn(trackAlias, column);
        }

        // Unknown values are stored as 0 in some columns
        std::string createNullIfZeroTrackColumn(std::string_view column)
        {
            return "NULLIF(" + createQualifiedTrackColumn(column) + ", 0)";
        }

        // Same sort name fallback as the artist sort orders
        std::string createSortNameExpr(std::string_view sortName, std::string_view name)
        {
            return "COALESCE(NULLIF(" + std::string{ sortName } + ", ''), " + std::string{ name } + ")";
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
            else if constexpr (std::is_same_v<T, std::string>)
                return BindValue{ std::in_place_type<std::string>, value };
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

        FilterEntity getStringFieldEntity(StringField field)
        {
            switch (field)
            {
            case StringField::TrackName:
            case StringField::ArtistName:
            case StringField::ArtistSortName:
                return FilterEntity::Track;
            case StringField::ReleaseName:
            case StringField::ReleaseSortName:
                return FilterEntity::Release;
            }

            throw Exception{ "Unhandled string filter field" };
        }

        // The entity the condition is about
        FilterEntity getConditionEntity(const FilterCondition& condition)
        {
            return std::visit(core::utils::overloads{
                                  [](const RelationCondition& relationCondition) { return getRelation(relationCondition).entity; },
                                  [](const IsCompilationCondition&) { return FilterEntity::Release; },
                                  [](const StringCondition& stringCondition) { return getStringFieldEntity(stringCondition.field); },
                                  [](const MediaLibraryId&) { return FilterEntity::Track; },
                                  [](const core::media::Codec&) { return FilterEntity::Track; },
                                  [](const Advisory&) { return FilterEntity::Track; },
                                  [](const IntegerCondition&) { return FilterEntity::Track; },
                                  [](const DurationCondition&) { return FilterEntity::Track; },
                                  [](const AddedDateTimeCondition&) { return FilterEntity::Track; },
                                  [](const ListenCountCondition&) { return FilterEntity::Track; },
                                  [](const LastListenDateTimeCondition&) { return FilterEntity::Track; },
                                  [](const RatingCondition&) { return FilterEntity::Track; },
                                  [](const FeedbackCondition&) { return FilterEntity::Track; } },
                              condition);
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

        // To be bound with createLikePattern()
        constexpr std::string_view sqlLikeComparison{ "LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'" };

        std::string createLikePattern(StringOperator op, std::string_view value)
        {
            std::string escapedValue{ utils::escapeForLikeKeyword(value) };

            switch (op)
            {
            case StringOperator::Equal:
                return escapedValue;
            case StringOperator::Contains:
                return "%" + escapedValue + "%";
            case StringOperator::StartsWith:
                return escapedValue + "%";
            case StringOperator::EndsWith:
                return "%" + escapedValue;
            }

            throw Exception{ "Unhandled string filter operator" };
        }

        // Unknown (missing value) already rejects the row except under NOT: COALESCE is added only there to keep indexes usable
        std::string createComparison(std::string_view valueExpr, std::string_view comparison, IsUnderNot isUnderNot)
        {
            std::string res;
            if (isUnderNot.value())
                res += "COALESCE(";
            res.append(valueExpr).append(" ").append(comparison);
            if (isUnderNot.value())
                res += ", 0)";

            return res;
        }

        std::string createComparison(std::string_view valueExpr, FilterOperator op, IsUnderNot isUnderNot)
        {
            return createComparison(valueExpr, getSqlComparison(op), isUnderNot);
        }

        // Registers the joins needed by the expression, then compiles it. Only lives during the compilation.
        // Conditions that are positive conjuncts on multi-valued relations are compiled as inner joins, others as IN (subquery)
        class Compiler
        {
        public:
            Compiler(FilterEntity queriedEntity, UserId user, JoinSet& joins)
                : _queriedEntity{ queriedEntity }
                , _user{ user }
                , _joins{ joins }
            {
            }
            ~Compiler() = default;

            Compiler(const Compiler&) = delete;
            Compiler& operator=(const Compiler&) = delete;

            SqlFragment compile(const FilterExpr& expr)
            {
                assert(_user.isValid() || !expr.hasPerUserCondition());

                registerJoins(expr, IsConjunct{ true });

                SqlFragment where;
                compileExpr(expr, IsUnderNot{ false }, where);
                return where;
            }

        private:
            struct JoinedCondition
            {
                RelationCondition condition;
                std::string alias;
            };

            void registerJoins(const FilterExpr& expr, IsConjunct isConjunct)
            {
                switch (expr.getType())
                {
                case FilterExpr::Type::Condition:
                    registerConditionJoins(expr.getCondition(), isConjunct);
                    break;

                case FilterExpr::Type::And:
                    for (const FilterExpr& child : expr.getChildren())
                        registerJoins(child, isConjunct);
                    break;

                case FilterExpr::Type::Or:
                case FilterExpr::Type::Not:
                    // an inner join would drop the rows without any related item, or match any other related item
                    for (const FilterExpr& child : expr.getChildren())
                        registerJoins(child, IsConjunct{ false });
                    break;
                }
            }

            void registerConditionJoins(const FilterExpr::Condition& condition, IsConjunct isConjunct)
            {
                std::visit(core::utils::overloads{
                               [&](const RelationCondition& relationCondition) {
                                   if (!isConjunct.value())
                                       return; // The condition will compile to a membership test later

                                   if (findJoinedCondition(relationCondition))
                                       return; // Equal conjuncts share the same join

                                   const Relation relation{ getRelation(relationCondition) };
                                   std::string alias{ "f_j" + std::to_string(++_relationJoinCount) };

                                   std::string clause;
                                   clause.append(relation.table).append(" ").append(alias).append(" ON ").append(alias).append(".").append(relation.foreignKey);
                                   clause.append(" = ").append(createEntityIdExpr(relation.entity));
                                   _joins.add(clause);

                                   _joinedConditions.push_back(JoinedCondition{ relationCondition, std::move(alias) });
                               },
                               [&](const IsCompilationCondition&) {
                                   registerReleaseJoin();
                               },
                               [&](const StringCondition& stringCondition) {
                                   if (getStringFieldEntity(stringCondition.field) == FilterEntity::Release)
                                       registerReleaseJoin();
                               },
                               [&](const RatingCondition&) {
                                   registerPerUserTrackJoin("rated_track", ratedTrackJoinAlias);
                               },
                               [&](const FeedbackCondition&) {
                                   registerPerUserTrackJoin("track_feedback", trackFeedbackJoinAlias);
                               },
                               [](const MediaLibraryId&) {},
                               [](const core::media::Codec&) {},
                               [](const Advisory&) {},
                               [](const IntegerCondition&) {},
                               [](const DurationCondition&) {},
                               [](const AddedDateTimeCondition&) {},
                               [](const ListenCountCondition&) {},
                               [](const LastListenDateTimeCondition&) {} },
                           condition);
            }

            const JoinedCondition* findJoinedCondition(const RelationCondition& condition) const
            {
                const auto it{ std::find_if(std::cbegin(_joinedConditions), std::cend(_joinedConditions), [&](const JoinedCondition& joinedCondition) { return joinedCondition.condition == condition; }) };
                return it != std::cend(_joinedConditions) ? &*it : nullptr;
            }

            void registerPerUserTrackJoin(std::string_view table, std::string_view alias)
            {
                std::string clause;
                clause.append(table).append(" ").append(alias).append(" ON ").append(alias).append(".track_id = ").append(createQualifiedTrackColumn("id")).append(" AND ").append(alias).append(".user_id = ?");
                _joins.add(clause, JoinSet::Type::Left, { toBindValue(_user) });
            }

            void registerReleaseJoin()
            {
                if (_queriedEntity == FilterEntity::Release)
                    return;

                std::string clause;
                clause.append("release ").append(releaseJoinAlias).append(" ON ").append(releaseJoinAlias).append(".id = ").append(createQualifiedTrackColumn("release_id"));
                _joins.add(clause, JoinSet::Type::Left);
            }

            // Expression of the id of the entity, as seen from the rows of the queried entity
            std::string createEntityIdExpr(FilterEntity entity) const
            {
                switch (entity)
                {
                case FilterEntity::Track:
                    return createQualifiedTrackColumn("id");
                case FilterEntity::Release:
                    // no need to join the release table just for its id
                    return _queriedEntity == FilterEntity::Release ? createQualifiedColumn(releaseAlias, "id") : createQualifiedTrackColumn("release_id");
                case FilterEntity::Artist:
                    return createQualifiedColumn(artistAlias, "id");
                }

                throw Exception{ "Unhandled filter entity" };
            }

            std::string createQualifiedReleaseColumn(std::string_view column) const
            {
                if (_queriedEntity == FilterEntity::Release)
                    return createQualifiedColumn(releaseAlias, column);

                return createQualifiedColumn(releaseJoinAlias, column);
            }

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

            // Relations that reuse a join are left alone: they already are a cheap comparison on the row
            const RelationCondition* getMergeableRelation(const FilterExpr& expr) const
            {
                if (expr.getType() != FilterExpr::Type::Condition)
                    return nullptr;

                const RelationCondition* relationCondition{ std::get_if<RelationCondition>(&expr.getCondition()) };
                if (!relationCondition || findJoinedCondition(*relationCondition))
                    return nullptr;

                return relationCondition;
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
                sql.append(createEntityIdExpr(relation.entity)).append(" IN (SELECT f_e.").append(relation.foreignKey).append(" FROM ").append(relation.table).append(" f_e WHERE f_e.").append(relation.valueColumn);
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
                                   // A condition equal to a joined conjunct is true on every row that survives the WHERE,
                                   // so it can use the join even when it is not a conjunct itself
                                   if (const JoinedCondition * joinedCondition{ findJoinedCondition(relationCondition) })
                                   {
                                       const Relation relation{ getRelation(relationCondition) };
                                       fragment.append(joinedCondition->alias + "." + std::string{ relation.valueColumn } + " = ?", { toBindValue(relationCondition) });
                                       return;
                                   }

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
                                   fragment.append(createComparison(createQualifiedReleaseColumn("is_compilation"), FilterOperator::Equal, isUnderNot), { toBindValue(isCompilationCondition.value) });
                               },
                               [&](const StringCondition& stringCondition) {
                                   compileStringCondition(stringCondition, isUnderNot, fragment);
                               },
                               [&](const ListenCountCondition& listenCountCondition) {
                                   fragment.append(createComparison(createListenCountExpr(), listenCountCondition.op, isUnderNot), createOperandBinds(listenCountCondition.value, { toBindValue(_user) }));
                               },
                               [&](const LastListenDateTimeCondition& lastListenDateTimeCondition) {
                                   fragment.append(createComparison(createLastListenDateTimeExpr(), lastListenDateTimeCondition.op, isUnderNot), createOperandBinds(lastListenDateTimeCondition.value, { toBindValue(_user) }));
                               },
                               [&](const RatingCondition& ratingCondition) {
                                   fragment.append(createComparison(createQualifiedColumn(ratedTrackJoinAlias, "rating"), ratingCondition.op, isUnderNot), createOperandBinds(ratingCondition.value));
                               },
                               [&](const FeedbackCondition& feedbackCondition) {
                                   fragment.append(createComparison(createQualifiedColumn(trackFeedbackJoinAlias, "value"), FilterOperator::Equal, isUnderNot), { toBindValue(feedbackCondition.value) });
                               } },
                           condition);
            }

            void compileStringCondition(const StringCondition& condition, IsUnderNot isUnderNot, SqlFragment& fragment) const
            {
                const std::string pattern{ createLikePattern(condition.op, condition.value) };

                const auto compileColumn = [&](const std::string& column) {
                    fragment.append(createComparison(column, sqlLikeComparison, isUnderNot), { toBindValue(pattern) });
                };

                switch (condition.field)
                {
                case StringField::TrackName:
                    compileColumn(createQualifiedTrackColumn("name"));
                    return;
                case StringField::ReleaseName:
                    compileColumn(createQualifiedReleaseColumn("name"));
                    return;
                case StringField::ReleaseSortName:
                    compileColumn(createSortNameExpr(createQualifiedReleaseColumn("sort_name"), createQualifiedReleaseColumn("name")));
                    return;
                case StringField::ArtistName:
                case StringField::ArtistSortName:
                    compileArtistCondition(condition, isUnderNot, fragment);
                    return;
                }

                throw Exception{ "Unhandled string filter field" };
            }

            // An artist linked to the track or to its release, matching by the name of the link or by its own name.
            void compileArtistCondition(const StringCondition& condition, IsUnderNot isUnderNot, SqlFragment& fragment) const
            {
                const bool sortName{ condition.field == StringField::ArtistSortName };
                const std::string pattern{ createLikePattern(condition.op, condition.value) };

                const std::string linkName{ sortName ? createSortNameExpr("f_l.artist_sort_name", "f_l.artist_name") : "f_l.artist_name" };
                const std::string ownName{ sortName ? createSortNameExpr("f_a.sort_name", "f_a.name") : "f_a.name" };
                const bool isArtistQuery{ _queriedEntity == FilterEntity::Artist };

                // Under NOT, the release of a track may be missing, and NULL IN (...) is unknown (which NOT keeps out) instead of false.
                // Optim: CASE WHEN rather than COALESCE(..., 0) or a bare NOT
                const bool needsCase{ isUnderNot.value() };

                std::string sql{ needsCase ? "CASE WHEN (" : "(" };

                const auto appendMemberships = [&](std::string_view linkTable, std::string_view linkEntityColumn, const std::string& entityId) {
                    // when querying artists, the pair is tested: only the links of the queried artist count
                    const std::string key{ isArtistQuery ? "(" + createEntityIdExpr(FilterEntity::Artist) + ", " + entityId + ")" : entityId };
                    const std::string columns{ isArtistQuery ? "f_l.artist_id, f_l." + std::string{ linkEntityColumn } : "f_l." + std::string{ linkEntityColumn } };

                    sql.append(key).append(" IN (SELECT ").append(columns).append(" FROM ").append(linkTable).append(" f_l WHERE ").append(linkName).append(" ").append(sqlLikeComparison).append(")");
                    sql.append(" OR ");
                    sql.append(key).append(" IN (SELECT ").append(columns).append(" FROM artist f_a JOIN ").append(linkTable).append(" f_l ON f_l.artist_id = f_a.id WHERE ").append(ownName).append(" ").append(sqlLikeComparison).append(")");
                };

                appendMemberships("track_artist_link", "track_id", createEntityIdExpr(FilterEntity::Track));
                sql += " OR ";
                appendMemberships("release_artist_link", "release_id", createEntityIdExpr(FilterEntity::Release));
                sql += needsCase ? ") THEN 1 ELSE 0 END" : ")";

                // One placeholder per membership
                const BindValue patternValue{ toBindValue(pattern) };
                fragment.append(sql, { patternValue, patternValue, patternValue, patternValue });
            }

            FilterEntity _queriedEntity;
            UserId _user;
            JoinSet& _joins; // several conditions can need the same join
            std::size_t _relationJoinCount{};
            std::vector<JoinedCondition> _joinedConditions; // TODO flat_set
        };
    } // namespace

    bool requiresTracks(const FilterExpr& expr, FilterEntity queriedEntity)
    {
        if (queriedEntity == FilterEntity::Track)
            return true;

        if (expr.getType() == FilterExpr::Type::Condition)
            return getConditionEntity(expr.getCondition()) != queriedEntity;

        return std::any_of(std::cbegin(expr.getChildren()), std::cend(expr.getChildren()), [&](const FilterExpr& child) { return requiresTracks(child, queriedEntity); });
    }

    SqlFragment compileFilterExpr(const std::optional<FilterExpr>& expr, FilterEntity queriedEntity, UserId user, JoinSet& joins)
    {
        if (!expr)
            return {};

        SqlFragment where{ Compiler{ queriedEntity, user, joins }.compile(*expr) };
        assert(queriedEntity == FilterEntity::Track || !requiresTracks(*expr, queriedEntity) || joins.hasAlias(trackAlias));
        return where;
    }
} // namespace lms::db
