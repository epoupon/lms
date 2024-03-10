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

#include "TracingView.hpp"

#include <Wt/Http/Response.h>
#include <Wt/WDateTime.h>
#include <Wt/WPushButton.h>
#include <Wt/WResource.h>

#include "utils/ITraceLogger.hpp"
#include "utils/String.hpp"

namespace UserInterface
{
    namespace
    {
        class ReportResource : public Wt::WResource
        {
        public:
            ReportResource(::tracing::ITraceLogger& traceLogger)
                : _traceLogger{ traceLogger }
            {
            }

            ~ReportResource()
            {
                beingDeleted();
            }

            void handleRequest(const Wt::Http::Request&, Wt::Http::Response& response)
            {
                suggestFileName(StringUtils::toISO8601String(Wt::WDateTime::currentDateTime()) + "-traces.json");
                response.setMimeType("application/json");
                _traceLogger.dumpCurrentBuffer(response.out());
            }

        private:
            tracing::ITraceLogger& _traceLogger;
        };
    }

    TracingView::TracingView()
        : Wt::WTemplate{ Wt::WString::tr("Lms.Admin.Tracing.template") }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);

        Wt::WPushButton* dumpBtn{ bindNew<Wt::WPushButton>("export-btn", Wt::WString::tr("Lms.Admin.Tracing.export-current-buffer")) };

        if (auto traceLogger{ Service<tracing::ITraceLogger>::get() })
        {
            Wt::WLink link{ std::make_shared<ReportResource>(*traceLogger) };
            link.setTarget(Wt::LinkTarget::NewWindow);
            dumpBtn->setLink(link);
        }
        else
            dumpBtn->setEnabled(false);
    }
} // namespace UserInterface
