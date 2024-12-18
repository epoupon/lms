/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "FeedbacksParser.hpp"

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Value.h>

#include "Exception.hpp"
#include "Utils.hpp"

namespace lms::feedback::listenBrainz
{
    namespace
    {
        Feedback parseFeedback(const Wt::Json::Object& feedbackObj)
        {
            const std::optional<core::UUID> recordingMBID{ core::UUID::fromString(static_cast<std::string>(feedbackObj.get("recording_mbid"))) };
            if (!recordingMBID)
                throw Exception{ "MBID not found!" };

            return Feedback{
                .created = Wt::WDateTime::fromTime_t(static_cast<int>(feedbackObj.get("created"))),
                .recordingMBID = *recordingMBID,
                .score = static_cast<FeedbackType>(static_cast<int>(feedbackObj.get("score")))
            };
        }
    } // namespace

    FeedbacksParser::Result FeedbacksParser::parse(std::string_view msgBody)
    {
        Result res;

        try
        {
            Wt::Json::Object root;
            Wt::Json::parse(std::string{ msgBody }, root);

            const Wt::Json::Array& feedbacks = root.get("feedback");

            LOG(DEBUG, "Got " << feedbacks.size() << " feedbacks");

            if (feedbacks.empty())
                return res;

            res.feedbackCount = feedbacks.size();

            for (const Wt::Json::Value& value : feedbacks)
            {
                try
                {
                    res.feedbacks.push_back(parseFeedback(value));
                }
                catch (const Exception& e)
                {
                    LOG(DEBUG, "Cannot parse feedback: " << e.what() << ", skipping");
                }
                catch (const Wt::WException& e)
                {
                    LOG(DEBUG, "Cannot parse feedback: " << e.what() << ", skipping");
                }
            }
        }
        catch (const Wt::WException& error)
        {
            LOG(ERROR, "Cannot parse 'feedback' result: " << error.what());
        }

        return res;
    }
} // namespace lms::feedback::listenBrainz
