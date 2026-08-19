/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "Database.hpp"

#include <algorithm>
#include <vector>

#include <Wt/Http/Response.h>
#include <Wt/Utils.h>
#include <Wt/WDateTime.h>
#include <Wt/WPushButton.h>
#include <Wt/WResource.h>

#include "core/Service.hpp"
#include "core/String.hpp"

#include "database/profiling/IQueryProfiler.hpp"

namespace lms::ui
{
    namespace
    {
        class QueryProfilingReportResource : public Wt::WResource
        {
        public:
            QueryProfilingReportResource(const db::IQueryProfiler& recorder)
                : _recorder{ recorder }
            {
            }

            ~QueryProfilingReportResource()
            {
                beingDeleted();
            }
            QueryProfilingReportResource(const QueryProfilingReportResource&) = delete;
            QueryProfilingReportResource& operator=(const QueryProfilingReportResource&) = delete;

        private:
            void handleRequest(const Wt::Http::Request&, Wt::Http::Response& response)
            {
                response.setMimeType("application/text");

                auto encodeHttpHeaderField = [](const std::string& fieldName, const std::string& fieldValue) {
                    // This implements RFC 5987
                    return fieldName + "*=UTF-8''" + Wt::Utils::urlEncode(fieldValue);
                };

                const std::string cdp{ encodeHttpHeaderField("filename", "LMS_db_query_profiling_" + core::stringUtils::toISO8601String(Wt::WDateTime::currentDateTime()) + ".txt") };
                response.addHeader("Content-Disposition", "attachment; " + cdp);

                std::vector<db::IQueryProfiler::QueryStats> entries;
                _recorder.visitQueries([&](const db::IQueryProfiler::QueryStats& stats) {
                    entries.push_back(stats);
                });

                std::sort(entries.begin(), entries.end(), [](const db::IQueryProfiler::QueryStats& a, const db::IQueryProfiler::QueryStats& b) {
                    return a.totalTime > b.totalTime;
                });

                for (const db::IQueryProfiler::QueryStats& stats : entries)
                {
                    response.out() << stats.query << '\n';
                    response.out() << "Calls: " << stats.callCount
                                   << " | Total: " << stats.totalTime.count() << " µs"
                                   << " | Mean: " << stats.meanTime.count() << " µs"
                                   << " | StdDev: " << stats.stdDevTime.count() << " µs\n";
                    response.out() << stats.plan << "\n-------------------------\n";
                }
            }

            const db::IQueryProfiler& _recorder;
        };
    } // namespace

    Database::Database()
        : Wt::WTemplate{ Wt::WString::tr("Lms.Admin.DebugTools.Db.template") }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);

        Wt::WPushButton* dumpBtn{ bindNew<Wt::WPushButton>("export-query-profiling-btn", Wt::WString::tr("Lms.Admin.DebugTools.Db.export-query-profiling")) };

        if (const auto* recorder{ core::Service<db::IQueryProfiler>::get() })
        {
            Wt::WLink link{ std::make_shared<QueryProfilingReportResource>(*recorder) };
            link.setTarget(Wt::LinkTarget::NewWindow);
            dumpBtn->setLink(link);
        }
        else
            dumpBtn->setEnabled(false);
    }

} // namespace lms::ui
