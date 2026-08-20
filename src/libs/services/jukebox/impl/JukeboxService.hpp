/*
 * Copyright (C) 2026 Emeric Poupon
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
#include <optional>
#include <shared_mutex>
#include <vector>

#include <boost/asio/io_context.hpp>

#include "core/IOContextRunner.hpp"

#include "audio/IAudioOutput.hpp"
#include "audio/utils/IAudioDecodeStreamer.hpp"

#include "services/jukebox/IJukeboxService.hpp"

namespace lms::jukebox
{
    class JukeboxService : public IJukeboxService
    {
    public:
        JukeboxService(db::IDb& db, audio::AudioOutputBackend backend);
        ~JukeboxService() override;

        JukeboxService(const JukeboxService&) = delete;
        JukeboxService& operator=(const JukeboxService&) = delete;

    private:
        void startInit() override;
        ServiceState getState() const override;

        void play(std::size_t trackIndex, std::chrono::microseconds offset) override;

        void pause() override;
        void resume() override;
        bool isPaused() const override;

        void setVolume(float volume) override;
        float getVolume() const override;

        std::optional<std::size_t> getCurrentTrackIndex() const override;
        std::chrono::microseconds getPlaybackTrackTime() const override;

        // Play queue control
        void clearTracks() override;
        void removeTrack(std::size_t index) override;
        void appendTracks(std::span<const db::TrackId> tracks) override;
        void shuffleTracks() override;
        std::vector<db::TrackId> getTracks() const override;

        void checkState(ServiceState state) const;

        void onContextReady();
        void onStreamReady();

        bool startDecoder(std::size_t trackIndex, std::chrono::microseconds offset = {});
        void abortDecoder();
        void onDecodeFinished(bool aborted);

        // TODO: make configurable or use detected output params
        static inline constexpr audio::PcmParameters _pcmParams{
            .channelCount = 2,
            .sampleRate = 44100,
            .sampleType = audio::PcmSampleType::Signed16,
            .byteOrder = std::endian::little,
            .planar = false,
        };

        mutable std::shared_mutex _mutex;

        const audio::AudioOutputBackend _backend;
        ServiceState _state;

        std::vector<db::TrackId> _tracks;              // protected by mutex
        std::optional<std::size_t> _currentTrackIndex; // protected by mutex
        std::chrono::microseconds _currentTrackPlaybackTimeOffset{};
        std::chrono::microseconds _currentTrackStartTimeOffset{};

        boost::asio::io_context _ioContext;
        core::IOContextRunner _ioContextRunner;
        db::IDb& _db;
        std::unique_ptr<audio::IAudioOutputContext> _outputContext;
        std::unique_ptr<audio::IAudioOutputStream> _outputStream;

        std::unique_ptr<audio::utils::IAudioDecodeStreamer> _decoder;
    };
} // namespace lms::jukebox