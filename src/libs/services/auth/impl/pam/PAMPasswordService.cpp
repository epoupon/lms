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

#include "PAMPasswordService.hpp"

#ifndef LMS_SUPPORT_PAM
    #error "Should not compile this"
#endif

#include <cstring>
#include <security/pam_appl.h>

#include "core/ILogger.hpp"
#include "services/auth/Types.hpp"

namespace lms::auth
{
    namespace
    {
        class PAMError
        {
        public:
            PAMError(std::string_view msg, pam_handle_t* pamh, int err)
            {
                _errorMsg = std::string{ msg } + ": " + pam_strerror(pamh, err);
            }

            std::string_view message() const { return _errorMsg; }

        private:
            std::string _errorMsg;
        };

        class PAMContext
        {
        public:
            PAMContext(std::string_view loginName)
            {
                int err{ pam_start("lms", std::string{ loginName }.c_str(), &_conv, &_pamh) };
                if (err != PAM_SUCCESS)
                    throw PAMError{ "start failed", _pamh, err };
            }

            ~PAMContext()
            {
                int err{ pam_end(_pamh, 0) };
                if (err != PAM_SUCCESS)
                    LMS_LOG(AUTH, ERROR, "end failed: " << pam_strerror(_pamh, err));
            }

            void authenticate(std::string_view password)
            {
                AuthenticateConvContext authContext{ password };
                ScopedConvContextSetter scopedContext{ *this, authContext };

                int err{ pam_authenticate(_pamh, 0) };
                if (err != PAM_SUCCESS)
                    throw PAMError{ "authenticate failed", _pamh, err };
            }

            void validateAccount()
            {
                int err{ pam_acct_mgmt(_pamh, PAM_SILENT) };
                if (err != PAM_SUCCESS)
                    throw PAMError{ "acct_mgmt failed", _pamh, err };
            }

        private:
            class ConvContext
            {
            public:
                virtual ~ConvContext() = default;
            };

            class AuthenticateConvContext final : public ConvContext
            {
            public:
                AuthenticateConvContext(std::string_view password)
                    : _password{ password } {}

                std::string_view getPassword() const { return _password; }

            private:
                std::string_view _password;
            };

            class ScopedConvContextSetter
            {
            public:
                ScopedConvContextSetter(PAMContext& pamContext, ConvContext& convContext)
                    : _pamContext{ pamContext }
                {
                    _pamContext._convContext = &convContext;
                }

                ~ScopedConvContextSetter()
                {
                    _pamContext._convContext = nullptr;
                }

                ScopedConvContextSetter(const ScopedConvContextSetter&) = delete;
                ScopedConvContextSetter(ScopedConvContextSetter&&) = delete;
                ScopedConvContextSetter& operator=(const ScopedConvContextSetter&) = delete;
                ScopedConvContextSetter& operator=(ScopedConvContextSetter&&) = delete;

            private:
                PAMContext& _pamContext;
            };

            static int conv(int msgCount, const pam_message** msgs, pam_response** resps, void* userData)
            {
                if (msgCount < 1)
                    return PAM_CONV_ERR;
                if (!resps || !msgs || !userData)
                    return PAM_CONV_ERR;

                PAMContext& context{ *static_cast<PAMContext*>(userData) };

                AuthenticateConvContext* authenticateContext = dynamic_cast<AuthenticateConvContext*>(context._convContext);
                if (!authenticateContext)
                {
                    LMS_LOG(AUTH, ERROR, "Unexpected conv!");
                    return PAM_CONV_ERR;
                }

                // Only expect a PAM_PROMPT_ECHO_OFF msg
                if (msgCount != 1 || msgs[0]->msg_style != PAM_PROMPT_ECHO_OFF)
                {
                    LMS_LOG(AUTH, ERROR, "Unexpected conv message. Count = " << msgCount);
                    return PAM_CONV_ERR;
                }

                pam_response* response{ static_cast<pam_response*>(malloc(sizeof(pam_response))) };
                if (!response)
                    return PAM_CONV_ERR;

                response->resp = strdup(std::string{ authenticateContext->getPassword() }.c_str());

                *resps = response;
                return PAM_SUCCESS;
            }

            ConvContext* _convContext{};
            pam_conv _conv{ &PAMContext::conv, this };
            pam_handle_t* _pamh{};
        };
    } // namespace

    bool PAMPasswordService::checkUserPassword(std::string_view loginName, std::string_view password)
    {
        try
        {
            LMS_LOG(AUTH, DEBUG, "Checking PAM password for user '" << loginName << "'");
            PAMContext pamContext{ loginName };

            pamContext.authenticate(password);
            pamContext.validateAccount();

            return true;
        }
        catch (const PAMError& error)
        {
            LMS_LOG(AUTH, ERROR, "PAM error: " << error.message());
            return false;
        }
    }

    bool PAMPasswordService::canSetPasswords() const
    {
        return false;
    }

    IPasswordService::PasswordAcceptabilityResult PAMPasswordService::checkPasswordAcceptability(std::string_view, const PasswordValidationContext&) const
    {
        throw NotImplementedException{};
    }

    void PAMPasswordService::setPassword(db::UserId /*userId*/, std::string_view /*newPassword*/)
    {
        throw NotImplementedException{};
    }

} // namespace lms::auth
