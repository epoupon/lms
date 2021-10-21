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

#pragma once

#include <string>
#include "services/database/Types.hpp"
#include "utils/Exception.hpp"

namespace Auth
{
	class Exception : public ::LmsException
	{
		using LmsException::LmsException;
	};

	class NotImplementedException : public Exception
	{
		public:
			NotImplementedException() : Auth::Exception {"Not implemented"} {}
	};

	class UserNotFoundException : public Exception
	{
		public:
			UserNotFoundException() : Auth::Exception {"User not found"} {}
	};

	struct PasswordValidationContext
	{
		std::string loginName;
		Database::UserType userType;
	};

	class PasswordException : public Exception
	{
		public:
			using Exception::Exception;
	};

	class PasswordTooWeakException : public PasswordException
	{
		public:
			PasswordTooWeakException() : PasswordException {"Password too weak"} {}
	};

	class PasswordMustMatchLoginNameException : public PasswordException
	{
		public:
			PasswordMustMatchLoginNameException() : PasswordException {"Password must match login name"} {}
	};
}

