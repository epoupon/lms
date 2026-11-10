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

#include <array>
#include <optional>

#include "FeedbackService.hpp"

#include "database/IDb.hpp"
#include "database/ISession.hpp"
#include "database/objects/User.hpp"

namespace lms::feedback
{
    template<typename ObjType, typename ObjIdType, typename FeedbackObjType>
    void FeedbackService::setFeedback(db::UserId userId, ObjIdType id, db::FeedbackValue value)
    {
        typename FeedbackObjType::IdType feedbackObjId;
        core::EnumSet<db::FeedbackBackend> backends;
        {
            db::ISession& session{ _db.getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            const std::optional<db::User> user{ db::User::find(session, userId) };
            if (!user)
                return;

            // Recording feedback locally is backend-independent
            std::optional<FeedbackObjType> feedbackObj{ FeedbackObjType::find(session, id, userId) };
            if (!feedbackObj)
            {
                if (value == db::FeedbackValue::None)
                    return; // nothing to clear

                if (!ObjType::exists(session, id))
                    return;

                feedbackObj = FeedbackObjType::create(session, id, userId);
            }

            feedbackObj->setValue(value);
            if (value != db::FeedbackValue::None)
                feedbackObj->setDateTime(Wt::WDateTime::currentDateTime());
            FeedbackObjType::update(session, *feedbackObj);

            feedbackObjId = feedbackObj->getId();
            backends = user->getFeedbackBackends();
        }

        for (const db::FeedbackBackend backend : backends)
        {
            if (!_backends[backend]->canBeFeedbacked(id))
                continue;

            _backends[backend]->onFeedbackChanged(feedbackObjId);
        }
    }

    template<typename ObjType, typename ObjIdType, typename FeedbackObjType>
    db::FeedbackValue FeedbackService::getFeedback(db::UserId userId, ObjIdType id)
    {
        db::ISession& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const std::optional<FeedbackObjType> feedbackObj{ FeedbackObjType::find(session, id, userId) };
        return feedbackObj ? feedbackObj->getValue() : db::FeedbackValue::None;
    }

    template<typename ObjType, typename ObjIdType, typename FeedbackObjType>
    Wt::WDateTime FeedbackService::getFeedbackDateTime(db::UserId userId, ObjIdType id)
    {
        db::ISession& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const std::optional<FeedbackObjType> feedbackObj{ FeedbackObjType::find(session, id, userId) };
        if (feedbackObj && feedbackObj->getValue() == db::FeedbackValue::Loved)
            return feedbackObj->getDateTime();

        return {};
    }

    template<typename ObjType, typename ObjIdType, typename RatedObjType>
    void FeedbackService::setRating(db::UserId userId, ObjIdType objectId, std::optional<db::Rating> rating)
    {
        db::ISession& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        std::optional<RatedObjType> ratedObject{ RatedObjType::find(session, objectId, userId) };
        if (rating)
        {
            if (!ratedObject)
            {
                const std::optional<db::User> user{ db::User::find(session, userId) };
                if (!user)
                    return;

                if (!ObjType::exists(session, objectId))
                    return;

                ratedObject = RatedObjType::create(session, objectId, userId);
            }

            ratedObject->setRating(*rating);
            ratedObject->setLastUpdated(Wt::WDateTime::currentDateTime());
            RatedObjType::update(session, *ratedObject);
        }
        else
        {
            if (ratedObject)
                RatedObjType::destroy(session, std::array{ ratedObject->getId() });
        }
    }

    template<typename ObjType, typename ObjIdType, typename RatedObjType>
    std::optional<db::Rating> FeedbackService::getRating(db::UserId userId, ObjIdType objectId)
    {
        db::ISession& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const std::optional<RatedObjType> ratedObj{ RatedObjType::find(session, objectId, userId) };
        if (!ratedObj)
            return std::nullopt;

        return ratedObj->getRating();
    }
} // namespace lms::feedback