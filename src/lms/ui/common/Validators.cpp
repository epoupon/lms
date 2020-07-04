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

#include "Validators.hpp"

#include <filesystem>

#include <Wt/WLengthValidator.h>

namespace UserInterface {

std::shared_ptr<Wt::WValidator>
createNameValidator()
{
	auto v = std::make_shared<Wt::WLengthValidator>();
	v->setMandatory(true);
	v->setMinimumLength(::Database::User::MinNameLength);
	v->setMaximumLength(::Database::User::MaxNameLength);
	return v;
}

std::shared_ptr<Wt::WValidator>
createMandatoryValidator()
{
	auto v = std::make_shared<Wt::WValidator>();
	//sv->setMandatory(true);
	return v;
}

DirectoryValidator::DirectoryValidator() : Wt::WValidator()
{
}

Wt::WValidator::Result
DirectoryValidator::validate(const Wt::WString& input) const
{
	if (input.empty())
		return Wt::WValidator::validate(input);

	const std::filesystem::path p {input.toUTF8()};
	std::error_code ec;

	// TODO check rights
	bool res = std::filesystem::is_directory(p, ec);
	if (ec)
		return Wt::WValidator::Result(Wt::ValidationState::Invalid, ec.message()); // TODO translate common errors
	else if (res)
		return Wt::WValidator::Result(Wt::ValidationState::Valid);
	else
		return Wt::WValidator::Result(Wt::ValidationState::Invalid, Wt::WString::tr("Lms.not-a-directory"));
}


} // namespace UserInterface
