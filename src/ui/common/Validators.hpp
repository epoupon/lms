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

#pragma once

#include <Wt/WRegExpValidator>
#include <Wt/WLengthValidator>

#include "database/User.hpp"

namespace UserInterface {

static inline Wt::WValidator *createEmailValidator()
{
	Wt::WValidator *res = new Wt::WRegExpValidator("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,4}");
	res->setMandatory(true);
	return res;
}

static inline Wt::WValidator *createNameValidator() {
	Wt::WLengthValidator *v = new Wt::WLengthValidator();
	v->setMandatory(true);
	v->setMinimumLength(3);
	v->setMaximumLength(::Database::User::MaxNameLength);
	return v;
}

} // namespace UserInterface
