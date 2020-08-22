/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "AuthModeModel.hpp"

#include "auth/IPasswordService.hpp"
#include "utils/Service.hpp"

namespace UserInterface
{

std::unique_ptr<AuthModeModel>
createAuthModeModel()
{
	auto model {std::make_unique<AuthModeModel>()};

	if (Service<::Auth::IPasswordService>::get()->isAuthModeSupported(Database::User::AuthMode::Internal))
		model->add(Wt::WString::tr("Lms.Admin.User.auth-mode.internal"), Database::User::AuthMode::Internal);
	if (Service<::Auth::IPasswordService>::get()->isAuthModeSupported(Database::User::AuthMode::PAM))
		model->add(Wt::WString::tr("Lms.Admin.User.auth-mode.pam"), Database::User::AuthMode::PAM);

	return model;
}

}

