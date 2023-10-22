/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <Wt/Dbo/Dbo.h>

namespace Database
{
    class Session;

    using Version = std::size_t;
    static constexpr Version LMS_DATABASE_VERSION{ 42 };
    class VersionInfo
    {
    public:
        using pointer = Wt::Dbo::ptr<VersionInfo>;

        static VersionInfo::pointer getOrCreate(Session& session);
        static VersionInfo::pointer get(Session& session);

        Version getVersion() const { return _version; }
        void setVersion(Version version) { _version = static_cast<int>(version); }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _version, "db_version");
        }

    private:
        int _version{ LMS_DATABASE_VERSION };
    };

    namespace Migration
    {
        void doDbMigration(Session& session);
    }
}
