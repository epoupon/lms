/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "TranscodeDecisionTracker.hpp"

#include <mutex>
#include <unordered_map>

#include "core/Random.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        class TranscodeDecisionTracker : public ITranscodeDecisionTracker
        {
        public:
            core::UUID add(AudioFileId audioFileId, const StreamDetails& targetStreamInfo) override
            {
                const core::UUID uuid{ core::UUID::generate() };
                const Clock::time_point now{ Clock::now() };

                auto entry{ std::make_shared<Entry>(now, audioFileId, targetStreamInfo) };

                {
                    std::scoped_lock lock{ mutex };

                    purgeOutdatedEntries(now);

                    entries.emplace(uuid, entry);
                }

                return uuid;
            }

            std::shared_ptr<Entry> get(const core::UUID& uuid) override
            {
                const Clock::time_point now{ Clock::now() };
                std::shared_ptr<Entry> res;

                std::scoped_lock lock{ mutex };

                auto it{ entries.find(uuid) };
                if (it != entries.end())
                {
                    if (now > it->second->addedTimePoint + maxEntryDuration)
                        entries.erase(it);
                    else
                        res = it->second;
                }

                return res;
            }

        private:
            void purgeOutdatedEntries(Clock::time_point now)
            {
                std::erase_if(entries, [&](const auto& entry) { return now > entry.second->addedTimePoint + maxEntryDuration; });
                while (entries.size() > maxEntryCount)
                    entries.erase(core::random::pickRandom(entries)); // TODO kill oldest one?
            }

            std::mutex mutex;
            std::unordered_map<core::UUID, std::shared_ptr<Entry>> entries;

            static constexpr std::size_t maxEntryCount{ 1'000 };
            static constexpr std::chrono::hours maxEntryDuration{ 12 };
        };
    } // namespace

    ITranscodeDecisionTracker& getTranscodeDecisionTracker()
    {
        static TranscodeDecisionTracker tracker;
        return tracker;
    }
} // namespace lms::api::subsonic