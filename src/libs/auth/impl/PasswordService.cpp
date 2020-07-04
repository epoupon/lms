/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "PasswordService.hpp"

#include <Wt/Auth/HashFunction.h>
#include <Wt/Auth/PasswordStrengthValidator.h>
#include <Wt/WRandom.h>

#include "database/Session.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

#include <security/pam_appl.h>

namespace Auth {

std::unique_ptr<IPasswordService> createPasswordService(std::size_t maxThrottlerEntries)
{
	return std::make_unique<PasswordService>(maxThrottlerEntries);
}

PasswordService::PasswordService(std::size_t maxThrottlerEntries)
: _loginThrottler{maxThrottlerEntries}
{
}

#ifdef USEPAM
static void
delete_resp(int num_msg, pam_response *response)
{
    if (response == nullptr)
        return;
    for (int i = 0; i < num_msg; i++) {
        if (response[i].resp) {
            /* clear before freeing -- might be a password */
            bzero(response[i].resp, strlen(response[i].resp));
            free(response[i].resp);
            response[i].resp = nullptr;
        }
    }
}

struct pam_conv_data
{
	const char *username;
	const char *password;
};

static
int lms_conv(int num_msg, const pam_message** msgs, pam_response** resps, void* appdata_ptr)
{
	if(num_msg < 1)
		return PAM_CONV_ERR;
	if (!resps || !msgs || !appdata_ptr)
		return PAM_CONV_ERR;
	
	pam_conv_data *data = static_cast<pam_conv_data*>( appdata_ptr );
	pam_response  *response = new (std::nothrow) pam_response[num_msg];
	if(!response)
		return PAM_CONV_ERR;

	for(int i = 0; i < num_msg; ++i)
	{
		response[i].resp_retcode = 0;
		response[i].resp = 0;
		switch (msgs[i]->msg_style) {
		case PAM_PROMPT_ECHO_ON:
			/* on memory allocation failure, auth fails */
			response[i].resp = strdup(data->username);
			break;
		case PAM_PROMPT_ECHO_OFF:
			response[i].resp = strdup(data->password);
			break;
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
		default:
			delete_resp(i, response);
			return PAM_CONV_ERR;
		}
	}
	
	*resps = response;
	return PAM_SUCCESS;
}
#endif

static bool
pamCheckUserPassword(const std::string& loginName, const std::string& password)
{
#ifdef USEPAM
	pam_conv_data authdata{loginName.c_str(), password.c_str()};
	pam_conv conv = { lms_conv, &authdata };
	pam_handle_t *pamh;
	bool authenticated{false};

	/* Initialize PAM framework */
	int err = pam_start("lms", loginName.c_str(), &conv, &pamh);
	if (err != PAM_SUCCESS) {
		return false;
	}

	err = pam_authenticate(pamh, 0);
	if(err == PAM_SUCCESS) 
	{
		/* Make sure account and password are still valid */
		err = pam_acct_mgmt(pamh, PAM_SILENT);
		authenticated = err == PAM_SUCCESS;
	}

	(void) pam_end(pamh, 0);
	return authenticated;
#else
	return false;
#endif
}

static
bool
checkUserPassword(Database::Session& session, const std::string& loginName, const std::string& password)
{
	bool hasExternalAuth;
	Database::User::PasswordHash passwordHash;
	{
		auto transaction {session.createSharedTransaction()};

		const Database::User::pointer user {Database::User::getByLoginName(session, loginName)};
		if (!user)
			return false;

		hasExternalAuth = user->hasExternalAuth();
		passwordHash = user->getPasswordHash();
	}

	if (hasExternalAuth) 
	{
		return pamCheckUserPassword(loginName, password);
	}
	else
	{
		const Wt::Auth::BCryptHashFunction hashFunc {6};
		return hashFunc.verify(password, passwordHash.salt, passwordHash.hash);
	}
}


PasswordService::PasswordCheckResult
PasswordService::checkUserPassword(Database::Session& session, const boost::asio::ip::address& clientAddress, const std::string& loginName, const std::string& password)
{
	// Do not waste too much resource on brute force attacks (optim)
	{
		std::shared_lock<std::shared_timed_mutex> lock {_mutex};

		if (_loginThrottler.isClientThrottled(clientAddress))
			return PasswordCheckResult::Throttled;
	}

	const bool match {Auth::checkUserPassword(session, loginName, password)};
	{
		std::unique_lock<std::shared_timed_mutex> lock {_mutex};

		if (_loginThrottler.isClientThrottled(clientAddress))
			return PasswordCheckResult::Throttled;

		if (match)
		{
			_loginThrottler.onGoodClientAttempt(clientAddress);
			return PasswordCheckResult::Match;
		}
		else
		{
			_loginThrottler.onBadClientAttempt(clientAddress);
			return PasswordCheckResult::Mismatch;
		}
	}
}

Database::User::PasswordHash
PasswordService::hashPassword(const std::string& password) const
{
	const std::string salt {Wt::WRandom::generateId(32)};

	const Wt::Auth::BCryptHashFunction hashFunc {6};
	return {salt, hashFunc.compute(password, salt)};
}

bool
PasswordService::evaluatePasswordStrength(const std::string& loginName, const std::string& password) const
{
	Wt::Auth::PasswordStrengthValidator validator;
	validator.setMinimumLength(Wt::Auth::PasswordStrengthType::OneCharClass, 4);
	validator.setMinimumLength(Wt::Auth::PasswordStrengthType::TwoCharClass, 4);
	validator.setMinimumLength(Wt::Auth::PasswordStrengthType::PassPhrase, 4);
	validator.setMinimumLength(Wt::Auth::PasswordStrengthType::ThreeCharClass, 4);
	validator.setMinimumLength(Wt::Auth::PasswordStrengthType::FourCharClass, 4);
	validator.setMinimumPassPhraseWords(1);
	validator.setMinimumMatchLength(3);

	return validator.evaluateStrength(password, loginName, "").isValid();
}

} // namespace Auth

