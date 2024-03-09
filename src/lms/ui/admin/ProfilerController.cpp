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

#include "ProfilerController.hpp"

#include <Wt/Http/Response.h>
#include <Wt/WDateTime.h>
#include <Wt/WPushButton.h>
#include <Wt/WResource.h>

#include "utils/IProfiler.hpp"
#include "utils/String.hpp"

namespace UserInterface
{
    namespace
    {
        class ReportResource : public Wt::WResource
        {
        public:
            ReportResource(::profiling::IProfiler& profiler)
                : _profiler{ profiler }
            {
            }

            ~ReportResource()
            {
                beingDeleted();
            }

            void handleRequest(const Wt::Http::Request&, Wt::Http::Response& response)
            {
                suggestFileName(StringUtils::toISO8601String(Wt::WDateTime::currentDateTime()) + "-profiling.json");
                response.setMimeType("application/json");
                _profiler.dumpCurrentBuffer(response.out());
            }

        private:
            profiling::IProfiler& _profiler;
        };
    }

    ProfilerController::ProfilerController()
        : Wt::WTemplate{ Wt::WString::tr("Lms.Admin.ProfilerController.template") }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);

        Wt::WPushButton* dumpBtn{ bindNew<Wt::WPushButton>("export-btn", Wt::WString::tr("Lms.Admin.ProfilerController.export-current-buffer")) };

        if (auto profiler{ Service<profiling::IProfiler>::get() })
        {
            Wt::WLink link{ std::make_shared<ReportResource>(*profiler) };
            link.setTarget(Wt::LinkTarget::NewWindow);
            dumpBtn->setLink(link);
        }
        else
            dumpBtn->setEnabled(false);
    }
} // namespace UserInterface
