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

#include "PAM.hpp"

#include <cstring>

#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

#include <security/pam_appl.h>

namespace Auth::PAM
{

static
	void
freeResp(int num_msg, pam_response *response)
{
	if (response == nullptr)
		return;

	for (int i = 0; i < num_msg; i++)
	{
		if (response[i].resp)
		{
			memset(response[i].resp, 0, strlen(response[i].resp));
			free(response[i].resp);
			response[i].resp = nullptr;
		}
	}

	free(response);
}

struct PAMConvData
{
	std::string loginName;
	std::string password;
};

static
int
lms_conv(int msgCount, const pam_message** msgs, pam_response** resps, void* userData)
{
	if (msgCount < 1)
		return PAM_CONV_ERR;
	if (!resps || !msgs || !userData)
		return PAM_CONV_ERR;

	const PAMConvData& convData {*static_cast<const PAMConvData*>(userData)};

	pam_response* response {static_cast<pam_response*>(malloc(sizeof(pam_response) * msgCount))};
	if (!response)
		return PAM_CONV_ERR;

	for (int i {}; i < msgCount; ++i)
	{
		response[i].resp_retcode = 0;
		response[i].resp = nullptr;

		switch (msgs[i]->msg_style)
		{
			case PAM_PROMPT_ECHO_ON:
				// on memory allocation failure, auth fails
				response[i].resp = strdup(convData.loginName.c_str());
				break;

			case PAM_PROMPT_ECHO_OFF:
				response[i].resp = strdup(convData.password.c_str());
				break;

			case PAM_ERROR_MSG:
			case PAM_TEXT_INFO:
			default:
				freeResp(i, response);
				return PAM_CONV_ERR;
		}
	}

	*resps = response;
	return PAM_SUCCESS;
}

class PAMError
{
	public:
		PAMError(std::string_view msg, pam_handle_t *pamh, int err)
		{
			_errorMsg = std::string {msg} + ": " + pam_strerror(pamh, err);
		}

		std::string_view message() const { return _errorMsg; }

	private:
		std::string _errorMsg;
};

class PAMContext
{
	public:
		PAMContext(std::string_view loginName)
			: _convData {std::string {loginName}, {}}
		{
			int err {pam_start("lms", _convData.loginName.c_str(), &_conv, &_pamh)};
			if (err != PAM_SUCCESS)
				throw PAMError {"start failed", _pamh, err};
		}

		~PAMContext()
		{
			int err {pam_end(_pamh, 0)};
			if (err != PAM_SUCCESS)
				LMS_LOG(AUTH, ERROR) << "end failed: " << pam_strerror(_pamh, err);
		}

		void authenticate(const std::string& password)
		{
			_convData.password = password;
			int err {pam_authenticate(_pamh, 0)};
			_convData.password.clear();

			if(err != PAM_SUCCESS)
				throw PAMError {"authenticate failed", _pamh, err};
		}

		void validateAccount()
		{
			int err {pam_acct_mgmt(_pamh, PAM_SILENT)};
			if (err != PAM_SUCCESS)
					throw PAMError {"acct_mgmt failed", _pamh, err};
		}

	private:
		PAMConvData _convData;
		pam_conv _conv {lms_conv, &_convData};
		pam_handle_t *_pamh {};

};

bool
checkUserPassword(const std::string& loginName, const std::string& password)
{
	try
	{
		PAMContext pamContext {loginName};

		pamContext.authenticate(password);
		pamContext.validateAccount();

		return true;
	}
	catch (const PAMError& error)
	{
		LMS_LOG(AUTH, ERROR) << "PAM error: " << error.message();
		return false;
	}
}

} // namespace Auth::PAM

