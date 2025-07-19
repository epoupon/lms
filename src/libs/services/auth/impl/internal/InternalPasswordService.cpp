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

#include "InternalPasswordService.hpp"

#include <Wt/WRandom.h>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"
#include "services/auth/Types.hpp"

namespace lms::auth
{
    InternalPasswordService::InternalPasswordService(db::IDb& db, std::size_t maxThrottlerEntries)
        : PasswordServiceBase{ db, maxThrottlerEntries }
        , _bcryptRoundCount{ static_cast<unsigned>(core::Service<core::IConfig>::get()->getULong("internal-password-bcrypt-round", 12)) }
    {
        if (_bcryptRoundCount < 7 || _bcryptRoundCount > 31)
            throw Exception{ "\"internal-password-bcrypt-round\" must be in range 7-31" };

        _validator.setMinimumLength(Wt::Auth::PasswordStrengthType::OneCharClass, 4);
        _validator.setMinimumLength(Wt::Auth::PasswordStrengthType::TwoCharClass, 4);
        _validator.setMinimumLength(Wt::Auth::PasswordStrengthType::PassPhrase, 4);
        _validator.setMinimumLength(Wt::Auth::PasswordStrengthType::ThreeCharClass, 4);
        _validator.setMinimumLength(Wt::Auth::PasswordStrengthType::FourCharClass, 4);
        _validator.setMinimumPassPhraseWords(1);
        _validator.setMinimumMatchLength(3);
    }

    bool InternalPasswordService::checkUserPassword(std::string_view loginName, std::string_view password)
    {
        LMS_LOG(AUTH, DEBUG, "Checking internal password for user '" << loginName << "'");

        db::User::PasswordHash passwordHash;
        {
            db::Session& session{ getDbSession() };
            auto transaction{ session.createReadTransaction() };

            const db::User::pointer user{ db::User::find(session, loginName) };
            if (!user)
            {
                LMS_LOG(AUTH, DEBUG, "hashing random stuff");
                // hash random stuff here to waste some time, don't give clue the user does not exist
                hashRandomPassword();
                return false;
            }

            // Don't allow users being created or coming from other backends
            passwordHash = user->getPasswordHash();
            if (passwordHash.salt.empty() || passwordHash.hash.empty())
            {
                // hash random stuff here to waste some time, don't give clue the user has no password set
                hashRandomPassword();
                return false;
            }
        }

        // Note: the round count set in the actual hash is used to verify, not the one used to construct _hashFunc
        bool passwordMatched{ _hashFunc.verify(std::string{ password }, std::string{ passwordHash.salt }, std::string{ passwordHash.hash }) };
        if (passwordMatched && passwordHash.bcryptRoundCount != _bcryptRoundCount)
        {
            LMS_LOG(AUTH, INFO, "Updating password hash for user '" << loginName << "' to match new bcrypt round count: previously " << passwordHash.bcryptRoundCount << " rounds, now " << _bcryptRoundCount << " rounds");
            const db::User::PasswordHash updatedPasswordHash{ hashPassword(password) };

            {
                db::Session& session{ getDbSession() };
                auto transaction{ session.createWriteTransaction() };

                if (db::User::pointer user{ db::User::find(session, loginName) })
                    user.modify()->setPasswordHash(updatedPasswordHash);
            }
        }

        return passwordMatched;
    }

    bool InternalPasswordService::canSetPasswords() const
    {
        return true;
    }

    IPasswordService::PasswordAcceptabilityResult InternalPasswordService::checkPasswordAcceptability(std::string_view password, const PasswordValidationContext& context) const
    {
        switch (context.userType)
        {
        case db::UserType::ADMIN:
        case db::UserType::REGULAR:
            return _validator.evaluateStrength(std::string{ password }, context.loginName, "").isValid() ? PasswordAcceptabilityResult::OK : PasswordAcceptabilityResult::TooWeak;
        case db::UserType::DEMO:
            return password == context.loginName ? PasswordAcceptabilityResult::OK : PasswordAcceptabilityResult::MustMatchLoginName;
        }

        throw NotImplementedException{};
    }

    void InternalPasswordService::setPassword(db::UserId userId, std::string_view newPassword)
    {
        const db::User::PasswordHash passwordHash{ hashPassword(newPassword) };

        {
            db::Session& session{ getDbSession() };
            auto transaction{ session.createWriteTransaction() };

            db::User::pointer user{ db::User::find(session, userId) };
            if (!user)
                throw Exception{ "User not found!" };

            switch (checkPasswordAcceptability(newPassword, PasswordValidationContext{ user->getLoginName(), user->getType() }))
            {
            case PasswordAcceptabilityResult::OK:
                break;
            case PasswordAcceptabilityResult::TooWeak:
                throw PasswordTooWeakException{};
            case PasswordAcceptabilityResult::MustMatchLoginName:
                throw PasswordMustMatchLoginNameException{};
            }

            user.modify()->setPasswordHash(passwordHash);
        }
    }

    db::User::PasswordHash InternalPasswordService::hashPassword(std::string_view password) const
    {
        const std::string salt{ Wt::WRandom::generateId(32) };

        return db::User::PasswordHash{ .bcryptRoundCount = _bcryptRoundCount, .salt = salt, .hash = _hashFunc.compute(std::string{ password }, salt) };
    }

    void InternalPasswordService::hashRandomPassword() const
    {
        hashPassword(Wt::WRandom::generateId(32));
    }
} // namespace lms::auth
