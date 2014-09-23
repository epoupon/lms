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

#include <boost/filesystem.hpp>

#include "DirectoryValidator.hpp"

namespace UserInterface {

DirectoryValidator::DirectoryValidator(Wt::WObject *parent)
: Wt::WValidator(parent)
{
}


Wt::WValidator::Result
DirectoryValidator::validate(const Wt::WString& input) const
{
	if (input.empty())
		return Wt::WValidator::validate(input);

	boost::filesystem::path p(input.toUTF8());
	boost::system::error_code ec;

	bool res = boost::filesystem::is_directory(p, ec);
	if (ec)
		return Wt::WValidator::Result(Wt::WValidator::Invalid, ec.message());
	else if (res)
		return Wt::WValidator::Result(Wt::WValidator::Valid);
	else
		return Wt::WValidator::Result(Wt::WValidator::Invalid, "Not a directory");
}


} // namespace UserInterface
