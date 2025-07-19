/*
 * Copyright (C) 2013 Emeric Poupon
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

#include "LoginNameValidator.hpp"

#include <Wt/WLengthValidator.h>

#include "database/objects/User.hpp"

namespace lms::ui
{
    namespace
    {
        class LengthValidator : public Wt::WLengthValidator
        {
        private:
            std::string javaScriptValidate() const override { return {}; }
        };
    } // namespace

    std::unique_ptr<Wt::WValidator> createLoginNameValidator()
    {
        auto v = std::make_unique<LengthValidator>();
        v->setMandatory(true);
        v->setMinimumLength(db::User::minNameLength);
        v->setMaximumLength(db::User::maxNameLength);
        return v;
    }
} // namespace lms::ui
