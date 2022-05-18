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

#include "DirectoryValidator.hpp"

#include <filesystem>

namespace UserInterface
{
	class DirectoryValidator : public Wt::WValidator
	{
		private:
			Wt::WValidator::Result validate(const Wt::WString& input) const override;
			std::string javaScriptValidate() const override { return {}; }
	};

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

	std::unique_ptr<Wt::WValidator>
	createDirectoryValidator()
	{
		return std::make_unique<DirectoryValidator>();
	}
} // namespace UserInterface
