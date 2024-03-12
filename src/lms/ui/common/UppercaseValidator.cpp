/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "UppercaseValidator.hpp"

#include <string>
#include <string_view>

namespace lms::ui
{
    class UppercaseValidator : public Wt::WValidator
    {
    private:
        Wt::WValidator::Result validate(const Wt::WString& input) const override;
        std::string javaScriptValidate() const override { return {}; }
    };

    Wt::WValidator::Result UppercaseValidator::validate(const Wt::WString& input) const
    {
        if (input.empty())
            return Wt::WValidator::validate(input);

        const std::string str{ input.toUTF8() };
        const bool valid{ std::all_of(std::cbegin(str), std::cend(str), [&](char c) { return !std::isalpha(c) || std::isupper(c);}) };

        if (!valid)
            return Wt::WValidator::Result(Wt::ValidationState::Invalid, Wt::WString::tr("Lms.field-must-be-in-upper-case"));

        return Wt::WValidator::Result(Wt::ValidationState::Valid);
    }

    std::unique_ptr<Wt::WValidator> createUppercaseValidator()
    {
        return std::make_unique<UppercaseValidator>();
    }
} // namespace lms::ui
