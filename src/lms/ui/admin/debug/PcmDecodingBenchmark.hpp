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

#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <Wt/WSignal.h>

namespace lms::db
{
    class IDb;
}

namespace lms::ui
{
    class PcmDecodingBenchmark
    {
    public:
        enum class State
        {
            Idle,
            Running,
            Completed
        };

        struct TrackDecodeResult
        {
            std::filesystem::path path;
            std::size_t bitrate{};
            std::chrono::milliseconds duration;
            float realTimeFactor{};
            float speedKBs{};
        };

        struct CodecResult
        {
            std::string codecName;
            std::vector<TrackDecodeResult> tracks;
            float minRealTimeFactor{};
            float maxRealTimeFactor{};
            float meanRealTimeFactor{};
            float stdDevRealTimeFactor{};
        };

        static PcmDecodingBenchmark& instance();

        State getState() const;
        std::vector<CodecResult> getResults() const;
        std::chrono::milliseconds getElapsed() const;
        std::string getReportFilename() const;

        void start(db::IDb& db);

        void registerOnStateChanged(const std::string& sessionId, std::function<void(State oldState, State newState)> f);
        void unregisterOnStateChanged(const std::string& sessionId);

    private:
        void postStateToAllSessions(State oldState, State newState);

        State _state{ State::Idle };
        mutable std::mutex _mutex;
        std::vector<CodecResult> _results;
        std::chrono::milliseconds _elapsed{};
        std::string _reportFilename;

        std::mutex _signalMutex;
        std::unordered_map<std::string, Wt::Signal<State, State>> _sessionSignals;
    };
} // namespace lms::ui
