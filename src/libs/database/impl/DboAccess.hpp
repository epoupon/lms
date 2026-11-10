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

#include <concepts>
#include <utility>

#include <Wt/Dbo/Session.h>
#include <Wt/Dbo/ptr.h>

#include "database/objects/detail/Object.hpp"

#define LMS_DB_INSTANTIATE_OBJECT(C) DBO_INSTANTIATE_TEMPLATES(lms::db::DboOf<C>::type)

namespace lms::db
{
    // Mapped class of an entity: specialized next to each private mapped subclass
    template<typename T>
    struct DboOf;

    template<typename T, typename IdType>
    T entityOf(const detail::Object<T, IdType>&);

    template<typename DboType>
    concept MappedEntity = std::derived_from<DboType, Wt::Dbo::Dbo<DboType>> && requires(const DboType& dbo) { entityOf(dbo); };

    template<MappedEntity DboType>
    using EntityOf = decltype(entityOf(std::declval<const DboType&>()));

    // Points a relation to the given id, without loading the object
    template<typename DboType, typename IdType>
    void setRelation(Wt::Dbo::Session& session, Wt::Dbo::ptr<DboType>& relation, IdType id)
    {
        if (!id.isValid())
            relation = Wt::Dbo::ptr<DboType>{};
        else if (relation.id() != id.getValue())
            relation = session.loadLazy<DboType>(id.getValue());
    }

    // The ids carried by the value are not mapped: they are refreshed from Wt each time the value is handed out
    template<MappedEntity DboType>
    const EntityOf<DboType>& toEntity(const Wt::Dbo::ptr<DboType>& ptr)
    {
        // Wt allocates its objects non-const
        DboType& dbo{ const_cast<DboType&>(*ptr) };
        dbo.syncIds();
        return dbo;
    }
} // namespace lms::db
