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

#include "database/objects/FilterExpr.hpp"

#include <algorithm>
#include <cassert>

namespace lms::db
{
    namespace
    {
#ifndef NDEBUG
        template<typename T>
        bool isValidOperand(FilterOperator op, const FilterOperand<T>& operand)
        {
            if (op == FilterOperator::Between)
            {
                const FilterRange<T>* range{ std::get_if<FilterRange<T>>(&operand) };
                return range && !(range->max < range->min);
            }

            return std::holds_alternative<T>(operand);
        }

        template<typename Id>
            requires requires(const Id& id) { id.isValid(); }
        bool isValid(const Id& id)
        {
            return id.isValid();
        }

        template<typename Condition>
            requires requires(const Condition& condition) { condition.op; condition.value; }
        bool isValid(const Condition& condition)
        {
            return isValidOperand(condition.op, condition.value);
        }

        bool isValid(const RelationCondition& condition)
        {
            return std::visit([](const auto& id) { return id.isValid(); }, condition.id);
        }

        bool isValid(core::media::Codec)
        {
            return true;
        }

        bool isValid(Advisory)
        {
            return true;
        }

        bool isValid(const IsCompilationCondition&)
        {
            return true;
        }

        bool isValid(const StringCondition&)
        {
            return true;
        }

        bool isValid(const FeedbackCondition&)
        {
            return true;
        }

        bool isValidCondition(const FilterCondition& condition)
        {
            return std::visit([](const auto& alternative) { return isValid(alternative); }, condition);
        }
#endif // NDEBUG

        template<typename T>
        concept PerUserCondition = requires { T::perUser; };
    } // namespace

    FilterExpr::FilterExpr(Type type, std::optional<Condition> condition, std::vector<FilterExpr> children)
        : _type{ type }
        , _condition{ std::move(condition) }
        , _children{ std::move(children) }
    {
    }

    FilterExpr FilterExpr::condition(Condition condition)
    {
        assert(isValidCondition(condition));

        return FilterExpr{ Type::Condition, std::move(condition), {} };
    }

    FilterExpr FilterExpr::equal(ClusterId cluster)
    {
        return condition(RelationCondition{ cluster });
    }

    FilterExpr FilterExpr::equal(CountryId country)
    {
        return condition(RelationCondition{ country });
    }

    FilterExpr FilterExpr::equal(GenreId genre)
    {
        return condition(RelationCondition{ genre });
    }

    FilterExpr FilterExpr::equal(GroupingId grouping)
    {
        return condition(RelationCondition{ grouping });
    }

    FilterExpr FilterExpr::equal(LabelId label)
    {
        return condition(RelationCondition{ label });
    }

    FilterExpr FilterExpr::equal(LanguageId language)
    {
        return condition(RelationCondition{ language });
    }

    FilterExpr FilterExpr::equal(MediaLibraryId mediaLibrary)
    {
        return condition(mediaLibrary);
    }

    FilterExpr FilterExpr::equal(MoodId mood)
    {
        return condition(RelationCondition{ mood });
    }

    FilterExpr FilterExpr::equal(ReleaseTypeId releaseType)
    {
        return condition(RelationCondition{ releaseType });
    }

    FilterExpr FilterExpr::equal(core::media::Codec codec)
    {
        return condition(codec);
    }

    FilterExpr FilterExpr::equal(Advisory advisory)
    {
        return condition(advisory);
    }

    FilterExpr FilterExpr::And(std::initializer_list<FilterExpr> children)
    {
        return And(std::span{ children.begin(), children.size() });
    }

    FilterExpr FilterExpr::And(std::span<const FilterExpr> children)
    {
        assert(!children.empty());

        return FilterExpr{ Type::And, std::nullopt, std::vector<FilterExpr>(std::cbegin(children), std::cend(children)) };
    }

    FilterExpr FilterExpr::Or(std::initializer_list<FilterExpr> children)
    {
        return Or(std::span{ children.begin(), children.size() });
    }

    FilterExpr FilterExpr::Or(std::span<const FilterExpr> children)
    {
        assert(!children.empty());

        return FilterExpr{ Type::Or, std::nullopt, std::vector<FilterExpr>(std::cbegin(children), std::cend(children)) };
    }

    FilterExpr FilterExpr::Not(const FilterExpr& child)
    {
        return FilterExpr{ Type::Not, std::nullopt, { child } };
    }

    const FilterExpr::Condition& FilterExpr::getCondition() const
    {
        assert(_type == Type::Condition);
        return *_condition;
    }

    bool FilterExpr::hasPerUserCondition() const
    {
        if (_type == Type::Condition)
            return std::visit([](const auto& alternative) { return PerUserCondition<std::decay_t<decltype(alternative)>>; }, *_condition);

        return std::any_of(std::cbegin(_children), std::cend(_children), [](const FilterExpr& child) { return child.hasPerUserCondition(); });
    }
} // namespace lms::db
