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

#include <Wt/Http/Response.h>
#include <Wt/Utils.h>
#include <Wt/WDateTime.h>
#include <Wt/WPushButton.h>
#include <Wt/WResource.h>

#include "core/ITraceLogger.hpp"
#include "core/String.hpp"
#include "database/IQueryPlanRecorder.hpp"

namespace lms::ui
{
    namespace
    {
        class QueryPlansReportResource : public Wt::WResource
        {
        public:
            QueryPlansReportResource(const db::IQueryPlanRecorder& recorder)
                : _recorder{ recorder }
            {
            }

            ~QueryPlansReportResource()
            {
                beingDeleted();
            }
            QueryPlansReportResource(const QueryPlansReportResource&) = delete;
            QueryPlansReportResource& operator=(const QueryPlansReportResource&) = delete;

        private:
            void handleRequest(const Wt::Http::Request&, Wt::Http::Response& response)
            {
                response.setMimeType("application/text");

                auto encodeHttpHeaderField = [](const std::string& fieldName, const std::string& fieldValue) {
                    // This implements RFC 5987
                    return fieldName + "*=UTF-8''" + Wt::Utils::urlEncode(fieldValue);
                };

                const std::string cdp{ encodeHttpHeaderField("filename", "LMS_db_query_plans_" + core::stringUtils::toISO8601String(Wt::WDateTime::currentDateTime()) + ".txt") };
                response.addHeader("Content-Disposition", "attachment; " + cdp);

                _recorder.visitQueryPlans([&](std::string_view query, std::string_view plan) {
                    response.out() << query << '\n';
                    response.out() << plan << "\n-------------------------\n";
                });
            }

            const db::IQueryPlanRecorder& _recorder;
        };
    } // namespace

    Database::Database()
        : Wt::WTemplate{ Wt::WString::tr("Lms.Admin.DebugTools.Db.template") }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);

        Wt::WPushButton* dumpBtn{ bindNew<Wt::WPushButton>("export-query-plans-btn", Wt::WString::tr("Lms.Admin.DebugTools.Db.export-query-plans")) };

        if (const auto* recorder{ core::Service<db::IQueryPlanRecorder>::get() })
        {
            Wt::WLink link{ std::make_shared<QueryPlansReportResource>(*recorder) };
            link.setTarget(Wt::LinkTarget::NewWindow);
            dumpBtn->setLink(link);
        }
        else
            dumpBtn->setEnabled(false);
    }

} // namespace lms::ui
