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

#include "TestData.hpp"

#include <array>

namespace lms::audio::tests
{
    std::filesystem::path TestAudioFile::getPath() const
    {
        return std::filesystem::path{ LMS_TEST_DATA_DIR } / fileName;
    }

    std::span<const TestAudioFile> getTestAudioFiles()
    {
        static constexpr std::array files{
            TestAudioFile{ .name = "mp3", .fileName = "sine.mp3", .container = core::media::Container::MPEG, .codec = core::media::Codec::MP3, .channelCount = 2, .sampleRate = 44'100, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = true },
            TestAudioFile{ .name = "flac", .fileName = "sine.flac", .container = core::media::Container::FLAC, .codec = core::media::Codec::FLAC, .channelCount = 2, .sampleRate = 44'100, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = true },
            TestAudioFile{ .name = "vorbis", .fileName = "sine_vorbis.ogg", .container = core::media::Container::Ogg, .codec = core::media::Codec::Vorbis, .channelCount = 2, .sampleRate = 44'100, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = true },
            TestAudioFile{ .name = "opus", .fileName = "sine.opus", .container = core::media::Container::Ogg, .codec = core::media::Codec::Opus, .channelCount = 2, .sampleRate = 48'000, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = true },
            TestAudioFile{ .name = "aac", .fileName = "sine_aac.m4a", .container = core::media::Container::MP4, .codec = core::media::Codec::AAC, .channelCount = 2, .sampleRate = 44'100, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = true },
            TestAudioFile{ .name = "alac", .fileName = "sine_alac.m4a", .container = core::media::Container::MP4, .codec = core::media::Codec::ALAC, .channelCount = 2, .sampleRate = 44'100, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = true },
            TestAudioFile{ .name = "wav", .fileName = "sine.wav", .container = core::media::Container::WAV, .codec = core::media::Codec::PCM, .channelCount = 2, .sampleRate = 44'100, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = true },
            TestAudioFile{ .name = "aiff", .fileName = "sine.aiff", .container = core::media::Container::AIFF, .codec = core::media::Codec::PCM, .channelCount = 2, .sampleRate = 44'100, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = true },
            TestAudioFile{ .name = "wma", .fileName = "sine.wma", .container = core::media::Container::ASF, .codec = core::media::Codec::WMA2, .channelCount = 2, .sampleRate = 44'100, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = false },
            TestAudioFile{ .name = "wavpack", .fileName = "sine.wv", .container = core::media::Container::WavPack, .codec = core::media::Codec::WavPack, .channelCount = 2, .sampleRate = 44'100, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = false },
            TestAudioFile{ .name = "tta", .fileName = "sine.tta", .container = core::media::Container::TrueAudio, .codec = core::media::Codec::TrueAudio, .channelCount = 2, .sampleRate = 44'100, .duration = std::chrono::milliseconds{ 3'000 }, .hasCoverArt = false },
        };
        return files;
    }
} // namespace lms::audio::tests
