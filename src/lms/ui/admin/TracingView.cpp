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
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#include "core/ITraceLogger.hpp"
#include "core/String.hpp"

namespace lms::ui
{
    namespace
    {
        class ReportResource : public Wt::WResource
        {
        public:
            ReportResource(core::tracing::ITraceLogger& traceLogger)
                : _traceLogger{ traceLogger }
            {
            }

            ~ReportResource()
            {
                beingDeleted();
            }

            void handleRequest(const Wt::Http::Request&, Wt::Http::Response& response)
            {
                response.setMimeType("application/gzip");
                suggestFileName(core::stringUtils::toISO8601String(Wt::WDateTime::currentDateTime()) + "-traces.json.gz");

                boost::iostreams::filtering_ostream gzipStream;
                gzipStream.push(boost::iostreams::gzip_compressor{});
                gzipStream.push(response.out());

                _traceLogger.dumpCurrentBuffer(gzipStream);
            }

        private:
            core::tracing::ITraceLogger& _traceLogger;
        };
    } // namespace

    TracingView::TracingView()
        : Wt::WTemplate{ Wt::WString::tr("Lms.Admin.Tracing.template") }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);

        Wt::WPushButton* dumpBtn{ bindNew<Wt::WPushButton>("export-btn", Wt::WString::tr("Lms.Admin.Tracing.export-current-buffer")) };

        if (auto traceLogger{ core::Service<core::tracing::ITraceLogger>::get() })
        {
            Wt::WLink link{ std::make_shared<ReportResource>(*traceLogger) };
            link.setTarget(Wt::LinkTarget::NewWindow);
            dumpBtn->setLink(link);
        }
        else
            dumpBtn->setEnabled(false);
    }
} // namespace lms::ui
