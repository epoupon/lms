/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "UUIDValidator.hpp"

#include <Wt/WRegExpValidator.h>

namespace lms::ui
{
    class RegExpValidator : public Wt::WRegExpValidator
    {
    public:
        using Wt::WRegExpValidator::WRegExpValidator;

    private:
        std::string javaScriptValidate() const override { return {}; }
    };

    std::unique_ptr<Wt::WValidator> createUUIDValidator()
    {
        auto validator{ std::make_unique<RegExpValidator>("[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}") };
        validator->setInvalidNoMatchText(Wt::WString::tr("Lms.uuid-invalid"));

        return validator;
    }
} // namespace lms::ui
