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

#include "Tooltip.hpp"

#include "core/ILogger.hpp"
#include <Wt/WWebWidget.h>

namespace lms::ui
{
    void initTooltipsForWidgetTree(Wt::WWebWidget& widget)
    {
        std::ostringstream oss;
        oss << R"({const rootElement = document.getElementById(')" << widget.id() << R"(');)"
            << R"(const tooltipTriggerList = rootElement.querySelectorAll('[data-bs-toggle="tooltip"]');
tooltipTriggerList.forEach(tooltipTriggerEl => {
new bootstrap.Tooltip(tooltipTriggerEl);
});})";
        LMS_LOG(UI, DEBUG, "Running JS '" << oss.str() << "'");

        widget.doJavaScript(oss.str());
    }
} // namespace lms::ui