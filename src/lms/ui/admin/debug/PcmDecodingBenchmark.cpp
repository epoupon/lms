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

#include "PcmDecodingBenchmark.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <thread>
#include <vector>

#include <Wt/WApplication.h>
#include <Wt/WDateTime.h>
#include <Wt/WServer.h>

#include "audio/Exception.hpp"
#include "audio/IPcmDecoder.hpp"
#include "audio/PcmTypes.hpp"
#include "core/String.hpp"
#include "core/media/Codec.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Filters.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/Types.hpp"

#include "LmsApplication.hpp"

namespace lms::ui
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        struct DecodeResult
        {
            std::size_t fileSize{};
            std::size_t decodedSampleCount{};
            Clock::duration decodeDuration{};
        };

        constexpr std::chrono::milliseconds minTrackDuration{ std::chrono::minutes{ 3 } };
        constexpr std::chrono::milliseconds maxTrackDuration{ std::chrono::minutes{ 5 } };
        constexpr std::size_t maxTracksPerCodec{ 10 };
        constexpr std::size_t queryBatchSize{ 100 };

        struct TrackEntry
        {
            std::filesystem::path path;
            std::chrono::milliseconds duration;
            std::size_t fileSize{};
            std::size_t bitrate{};
        };

        std::vector<TrackEntry> pickTracksForCodec(db::Session& session, core::media::Codec codec)
        {
            auto transaction{ session.createReadTransaction() };

            db::Track::FindParameters params;
            params.setFilters(db::Filters{}.setCodec(codec));
            params.setSortMethod(db::TrackSortMethod::Random);
            params.setRange(db::Range{ .offset = 0, .size = queryBatchSize });

            const auto results{ db::Track::find(session, params) };

            std::vector<TrackEntry> entries;
            for (const db::Track::pointer& track : results)
            {
                const auto trackDuration{ track->getDuration() };
                if (trackDuration >= minTrackDuration && trackDuration <= maxTrackDuration)
                {
                    entries.push_back({ track->getAbsoluteFilePath(), trackDuration, static_cast<std::size_t>(track->getFileSize()), track->getBitrate() });
                    if (entries.size() == maxTracksPerCodec)
                        break;
                }
            }

            return entries;
        }

        DecodeResult decodeFile(const std::filesystem::path& path, std::size_t fileSize, const audio::PcmParameters& params)
        {
            constexpr std::size_t bufferSamples{ 4096 };
            const std::size_t bufferSize{ bufferSamples * params.channelCount * audio::getSampleSize(params.sampleType) };

            std::vector<std::byte> buffer(bufferSize);
            std::array<audio::IPcmDecoder::WritableBuffer, 1> outputBuffers{ audio::IPcmDecoder::WritableBuffer{ buffer } };

            const auto start{ Clock::now() };

            auto decoder{ audio::createPcmDecoder(path, std::chrono::microseconds{ 0 }, params) };

            std::size_t sampleCount{};
            while (!decoder->finished())
                sampleCount += decoder->readSamples(outputBuffers);

            return DecodeResult{
                .fileSize = fileSize,
                .decodedSampleCount = sampleCount,
                .decodeDuration = Clock::now() - start,
            };
        }

        std::vector<PcmDecodingBenchmark::CodecResult> runBench(db::IDb& db)
        {
            constexpr audio::PcmParameters params{
                .channelCount = 2,
                .sampleRate = 48000,
                .sampleType = audio::PcmSampleType::Signed16,
                .byteOrder = std::endian::native,
                .planar = false,
            };

            std::vector<PcmDecodingBenchmark::CodecResult> allResults;

            core::media::visitCodecs([&](const core::media::CodecDesc& codecDesc) {
                const std::vector<TrackEntry> tracks{ pickTracksForCodec(db.getTLSSession(), codecDesc.type) };
                if (tracks.empty())
                    return;

                std::vector<PcmDecodingBenchmark::TrackDecodeResult> trackResults;
                for (const TrackEntry& entry : tracks)
                {
                    try
                    {
                        const DecodeResult res{ decodeFile(entry.path, entry.fileSize, params) };
                        const float elapsedSeconds{ std::chrono::duration<float>{ res.decodeDuration }.count() };
                        if (elapsedSeconds <= 0.F || res.decodedSampleCount == 0)
                            continue;

                        const float decodedAudioSeconds{ static_cast<float>(res.decodedSampleCount) / params.sampleRate };
                        trackResults.push_back(PcmDecodingBenchmark::TrackDecodeResult{
                            .path = entry.path,
                            .bitrate = entry.bitrate,
                            .duration = entry.duration,
                            .realTimeFactor = decodedAudioSeconds / elapsedSeconds,
                            .speedKBs = (static_cast<float>(res.fileSize) / 1024.F) / elapsedSeconds,
                        });
                    }
                    catch (const audio::Exception&)
                    {
                    }
                }

                if (trackResults.empty())
                    return;

                const std::size_t n{ trackResults.size() };
                const float mean{ std::accumulate(trackResults.begin(), trackResults.end(), 0.F, [](float acc, const PcmDecodingBenchmark::TrackDecodeResult& t) {
                                      return acc + t.realTimeFactor;
                                  })
                                  / static_cast<float>(n) };
                const float variance{ std::accumulate(trackResults.begin(), trackResults.end(), 0.F, [mean](float acc, const PcmDecodingBenchmark::TrackDecodeResult& t) {
                                          return acc + (t.realTimeFactor - mean) * (t.realTimeFactor - mean);
                                      })
                                      / static_cast<float>(n) };
                const auto [minIt, maxIt]{ std::minmax_element(trackResults.begin(), trackResults.end(), [](const PcmDecodingBenchmark::TrackDecodeResult& a, const PcmDecodingBenchmark::TrackDecodeResult& b) {
                    return a.realTimeFactor < b.realTimeFactor;
                }) };
                allResults.push_back(PcmDecodingBenchmark::CodecResult{
                    .codecName = std::string{ codecDesc.name.str() },
                    .tracks = std::move(trackResults),
                    .minRealTimeFactor = minIt->realTimeFactor,
                    .maxRealTimeFactor = maxIt->realTimeFactor,
                    .meanRealTimeFactor = mean,
                    .stdDevRealTimeFactor = std::sqrt(variance),
                });
            });

            return allResults;
        }
    } // namespace

    PcmDecodingBenchmark& PcmDecodingBenchmark::instance()
    {
        static PcmDecodingBenchmark s_instance;
        return s_instance;
    }

    PcmDecodingBenchmark::State PcmDecodingBenchmark::getState() const
    {
        std::scoped_lock lock{ _mutex };
        return _state;
    }

    std::vector<PcmDecodingBenchmark::CodecResult> PcmDecodingBenchmark::getResults() const
    {
        std::scoped_lock lock{ _mutex };
        return _results;
    }

    std::chrono::milliseconds PcmDecodingBenchmark::getElapsed() const
    {
        std::scoped_lock lock{ _mutex };
        return _elapsed;
    }

    std::string PcmDecodingBenchmark::getReportFilename() const
    {
        std::scoped_lock lock{ _mutex };
        return _reportFilename;
    }

    void PcmDecodingBenchmark::start(db::IDb& db)
    {
        {
            State prevState;
            {
                std::scoped_lock lock{ _mutex };

                if (_state == State::Running)
                    return;

                _reportFilename = "LMS_pcm_decoding_stats_" + core::stringUtils::toISO8601String(Wt::WDateTime::currentDateTime()) + ".txt";
                prevState = _state;
                _state = State::Running;
            }

            postStateToAllSessions(prevState, State::Running);
        }

        std::thread{ [this, &db] {
            const auto start{ Clock::now() };
            std::vector<CodecResult> results{ runBench(db) };
            const auto elapsed{ std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start) };

            State prevState;
            {
                std::scoped_lock lock{ _mutex };

                _results = std::move(results);
                _elapsed = elapsed;
                prevState = _state;
                _state = State::Completed;
            }

            postStateToAllSessions(prevState, State::Completed);
        } }.detach();
    }

    void PcmDecodingBenchmark::registerOnStateChanged(const std::string& sessionId, std::function<void(State, State)> f)
    {
        std::scoped_lock lock{ _signalMutex };
        _sessionSignals[sessionId].connect(std::move(f));
    }

    void PcmDecodingBenchmark::unregisterOnStateChanged(const std::string& sessionId)
    {
        std::scoped_lock lock{ _signalMutex };
        _sessionSignals.erase(sessionId);
    }

    void PcmDecodingBenchmark::postStateToAllSessions(State oldState, State newState)
    {
        auto* server{ Wt::WServer::instance() };
        if (!server)
            return;

        server->postAll([this, oldState, newState] {
            if (!LmsApp)
                return;

            Wt::Signal<State, State>* sig{ nullptr };
            {
                std::scoped_lock lock{ _signalMutex };
                auto it{ _sessionSignals.find(wApp->sessionId()) };
                if (it != _sessionSignals.end())
                    sig = &it->second;
            }

            if (sig)
            {
                sig->emit(oldState, newState);
                LmsApp->triggerUpdate();
            }
        });
    }
} // namespace lms::ui
