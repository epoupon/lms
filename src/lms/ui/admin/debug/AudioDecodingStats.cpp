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

#include "AudioDecodingStats.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

#include <Wt/Http/Response.h>
#include <Wt/Utils.h>
#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WPushButton.h>
#include <Wt/WResource.h>
#include <Wt/WString.h>
#include <Wt/WText.h>

#include "AudioDecodingBenchmark.hpp"
#include "LmsApplication.hpp"
#include "Notification.hpp"

namespace lms::ui
{
    namespace
    {
        class AudioDecodingStatsResource : public Wt::WResource
        {
        public:
            AudioDecodingStatsResource(std::vector<AudioDecodingBenchmark::CodecResult> results, std::chrono::milliseconds elapsed, std::string filename)
                : _results{ std::move(results) }
                , _elapsed{ elapsed }
                , _filename{ std::move(filename) }
            {
            }

            ~AudioDecodingStatsResource() override
            {
                beingDeleted();
            }

            AudioDecodingStatsResource(const AudioDecodingStatsResource&) = delete;
            AudioDecodingStatsResource& operator=(const AudioDecodingStatsResource&) = delete;

        private:
            void handleRequest(const Wt::Http::Request&, Wt::Http::Response& response) override
            {
                response.setMimeType("text/plain; charset=utf-8");

                auto encodeHttpHeaderField = [](const std::string& fieldName, const std::string& fieldValue) {
                    return fieldName + "*=UTF-8''" + Wt::Utils::urlEncode(fieldValue);
                };

                const std::string cdp{ encodeHttpHeaderField("filename", _filename) };
                response.addHeader("Content-Disposition", "attachment; " + cdp);

                response.out() << std::fixed << std::setprecision(1);
                response.out() << "Audio decoding bench (elapsed: " << std::chrono::duration_cast<std::chrono::seconds>(_elapsed).count() << "s)\n\n";

                std::size_t totalTracks{};
                float globalMin{ std::numeric_limits<float>::max() };
                float globalMax{ std::numeric_limits<float>::lowest() };
                float weightedMeanSum{};

                for (const AudioDecodingBenchmark::CodecResult& r : _results)
                {
                    response.out() << "=== " << r.codecName << " (" << r.tracks.size() << " track(s)) ===\n";

                    for (const AudioDecodingBenchmark::TrackDecodeResult& t : r.tracks)
                    {
                        const long durationSec{ static_cast<long>(t.duration.count() / 1000) };
                        response.out() << "  " << t.path
                                       << "  bitrate=" << t.bitrate / 1000 << " kbps"
                                       << "  duration=" << durationSec / 60 << "m" << durationSec % 60 << "s"
                                       << "  real_time_factor=" << t.realTimeFactor << "x"
                                       << "  speed=" << t.speedKBs << " KB/s\n";
                    }

                    response.out() << "  real_time_factor summary:"
                                   << " min=" << r.minRealTimeFactor << "x"
                                   << " max=" << r.maxRealTimeFactor << "x"
                                   << " mean=" << r.meanRealTimeFactor << "x"
                                   << " stddev=" << r.stdDevRealTimeFactor << "x\n\n";

                    totalTracks += r.tracks.size();
                    globalMin = std::min(globalMin, r.minRealTimeFactor);
                    globalMax = std::max(globalMax, r.maxRealTimeFactor);
                    weightedMeanSum += r.meanRealTimeFactor * static_cast<float>(r.tracks.size());
                }

                if (totalTracks > 0)
                {
                    const float globalMean{ weightedMeanSum / static_cast<float>(totalTracks) };

                    float weightedVarianceSum{};
                    for (const AudioDecodingBenchmark::CodecResult& r : _results)
                    {
                        const float d{ r.meanRealTimeFactor - globalMean };
                        weightedVarianceSum += static_cast<float>(r.tracks.size()) * (r.stdDevRealTimeFactor * r.stdDevRealTimeFactor + d * d);
                    }

                    response.out() << "=== OVERALL (" << totalTracks << " track(s)) ===\n";
                    response.out() << "  real_time_factor summary:"
                                   << " min=" << globalMin << "x"
                                   << " max=" << globalMax << "x"
                                   << " mean=" << globalMean << "x"
                                   << " stddev=" << std::sqrt(weightedVarianceSum / static_cast<float>(totalTracks)) << "x\n";
                }
            }

            std::vector<AudioDecodingBenchmark::CodecResult> _results;
            std::chrono::milliseconds _elapsed{};
            std::string _filename;
        };
    } // namespace

    AudioDecodingStats::AudioDecodingStats()
        : Wt::WTemplate{ Wt::WString::tr("Lms.Admin.DebugTools.AudioDecodingStats.template") }
        , _db{ LmsApp->getDb() }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);

        _runBtn = bindNew<Wt::WPushButton>("run-btn", Wt::WString::tr("Lms.Admin.DebugTools.AudioDecodingStats.run"));
        _downloadBtn = bindNew<Wt::WAnchor>("download-btn");
        _downloadBtn->setText(Wt::WString::tr("Lms.Admin.DebugTools.AudioDecodingStats.download"));
        _statusText = bindNew<Wt::WText>("status");

        _runBtn->clicked().connect(this, &AudioDecodingStats::onRunClicked);

        processState(AudioDecodingBenchmark::instance().getState());

        AudioDecodingBenchmark::instance().registerOnStateChanged(wApp->sessionId(), [this](AudioDecodingBenchmark::State oldState, AudioDecodingBenchmark::State newState) {
            onStateChanged(oldState, newState);
        });
    }

    AudioDecodingStats::~AudioDecodingStats()
    {
        AudioDecodingBenchmark::instance().unregisterOnStateChanged(wApp->sessionId());
    }

    void AudioDecodingStats::onRunClicked()
    {
        AudioDecodingBenchmark::instance().start(_db);
    }

    void AudioDecodingStats::processState(AudioDecodingBenchmark::State state)
    {
        switch (state)
        {
        case AudioDecodingBenchmark::State::Idle:
            _runBtn->setEnabled(true);
            _downloadBtn->hide();
            _statusText->setText({});
            break;
        case AudioDecodingBenchmark::State::Running:
            _runBtn->setEnabled(false);
            _downloadBtn->hide();
            _statusText->setText(Wt::WString::tr("Lms.Admin.DebugTools.AudioDecodingStats.running"));
            break;
        case AudioDecodingBenchmark::State::Completed:
            _runBtn->setEnabled(true);
            _statusText->setText({});
            setupDownloadButton();
            break;
        }
    }

    void AudioDecodingStats::onStateChanged(AudioDecodingBenchmark::State oldState, AudioDecodingBenchmark::State newState)
    {
        processState(newState);

        if (oldState == AudioDecodingBenchmark::State::Running && newState == AudioDecodingBenchmark::State::Completed)
        {
            std::ostringstream oss;
            oss << std::chrono::duration_cast<std::chrono::seconds>(AudioDecodingBenchmark::instance().getElapsed()).count() << "s";
            LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Admin.DebugTools.AudioDecodingStats.benchmark-completed").arg(oss.str()));
        }
    }

    void AudioDecodingStats::setupDownloadButton()
    {
        auto& bench{ AudioDecodingBenchmark::instance() };
        auto resource{ std::make_shared<AudioDecodingStatsResource>(bench.getResults(), bench.getElapsed(), bench.getReportFilename()) };

        Wt::WLink link{ resource };
        link.setTarget(Wt::LinkTarget::NewWindow);
        _downloadBtn->setLink(link);
        _downloadBtn->show();
    }
} // namespace lms::ui
