/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "Template.hpp"
#include "core/String.hpp"

namespace lms::ui
{
    void Template::applyArguments(WWidget* widget, const std::vector<Wt::WString>& args)
    {
        for (const Wt::WString& argStr : args)
        {
            std::string arg{ argStr.toUTF8() };
            const std::vector<std::string_view> operands{ core::stringUtils::splitString(arg, '=') };

            if (operands.size() == 2)
            {
                if (operands[0] == "class")
                    widget->addStyleClass(std::string{ operands[1] });
                else
                    widget->setAttributeValue(std::string{ operands[0] }, std::string{ operands[1] });
            }
        }
    }
} // namespace lms::ui
