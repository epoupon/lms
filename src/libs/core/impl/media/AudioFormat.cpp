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

#include "core/media/AudioFormat.hpp"

#include <algorithm>
#include <array>
#include <cassert>

namespace lms::core::media
{
    namespace
    {
        constexpr std::array aiffExtensions{ std::string_view{ ".aiff" }, std::string_view{ ".aif" }, std::string_view{ ".aifc" }, std::string_view{ ".afc" } };
        constexpr std::array apeExtensions{ std::string_view{ ".ape" } };
        constexpr std::array wmaExtensions{ std::string_view{ ".wma" }, std::string_view{ ".asf" } };
        constexpr std::array dsfExtensions{ std::string_view{ ".dsf" } };
        constexpr std::array flacExtensions{ std::string_view{ ".flac" } };
        constexpr std::array mp4AacExtensions{ std::string_view{ ".m4a" }, std::string_view{ ".m4b" }, std::string_view{ ".mp4" }, std::string_view{ ".m4r" }, std::string_view{ ".m4p" }, std::string_view{ ".3g2" }, std::string_view{ ".m4v" } };
        constexpr std::array mp4AlacExtensions{ std::string_view{ ".m4a" }, std::string_view{ ".alac" } };
        constexpr std::array mpcExtensions{ std::string_view{ ".mpc" } };
        constexpr std::array mp3Extensions{ std::string_view{ ".mp3" }, std::string_view{ ".mp2" } };
        constexpr std::array rawAacExtensions{ std::string_view{ ".aac" } };
        constexpr std::array oggFlacExtensions{ std::string_view{ ".oga" } };
        constexpr std::array oggVorbisExtensions{ std::string_view{ ".ogg" }, std::string_view{ ".oga" } };
        constexpr std::array oggOpusExtensions{ std::string_view{ ".opus" } };
        constexpr std::array shortenExtensions{ std::string_view{ ".shn" } };
        constexpr std::array ttaExtensions{ std::string_view{ ".tta" } };
        constexpr std::array wavExtensions{ std::string_view{ ".wav" } };
        constexpr std::array wavPackExtensions{ std::string_view{ ".wv" } };

        struct AudioFormatEntry
        {
            AudioFormat format;
            ExtensionSpan extensions;
        };

        // order is important
        constexpr std::array audioFormatEntries{
            AudioFormatEntry{ .format = { Container::AIFF, Codec::PCM }, .extensions = aiffExtensions },
            AudioFormatEntry{ .format = { Container::APE, Codec::APE }, .extensions = apeExtensions },
            AudioFormatEntry{ .format = { Container::ASF, Codec::WMA1 }, .extensions = wmaExtensions },
            AudioFormatEntry{ .format = { Container::ASF, Codec::WMA2 }, .extensions = wmaExtensions },
            AudioFormatEntry{ .format = { Container::ASF, Codec::WMA9Pro }, .extensions = wmaExtensions },
            AudioFormatEntry{ .format = { Container::ASF, Codec::WMA9Lossless }, .extensions = wmaExtensions },
            AudioFormatEntry{ .format = { Container::DSF, Codec::DSD }, .extensions = dsfExtensions },
            AudioFormatEntry{ .format = { Container::FLAC, Codec::FLAC }, .extensions = flacExtensions },
            AudioFormatEntry{ .format = { Container::MP4, Codec::AAC }, .extensions = mp4AacExtensions },
            AudioFormatEntry{ .format = { Container::MP4, Codec::ALAC }, .extensions = mp4AlacExtensions },
            AudioFormatEntry{ .format = { Container::MPC, Codec::MPC7 }, .extensions = mpcExtensions },
            AudioFormatEntry{ .format = { Container::MPC, Codec::MPC8 }, .extensions = mpcExtensions },
            AudioFormatEntry{ .format = { Container::MPEG, Codec::MP3 }, .extensions = mp3Extensions },
            AudioFormatEntry{ .format = { Container::MPEG, Codec::AAC }, .extensions = rawAacExtensions }, // raw ADTS AAC has no container of its own
            AudioFormatEntry{ .format = { Container::Ogg, Codec::FLAC }, .extensions = oggFlacExtensions },
            AudioFormatEntry{ .format = { Container::Ogg, Codec::Vorbis }, .extensions = oggVorbisExtensions },
            AudioFormatEntry{ .format = { Container::Ogg, Codec::Opus }, .extensions = oggOpusExtensions },
            AudioFormatEntry{ .format = { Container::Shorten, Codec::Shorten }, .extensions = shortenExtensions },
            AudioFormatEntry{ .format = { Container::TrueAudio, Codec::TrueAudio }, .extensions = ttaExtensions },
            AudioFormatEntry{ .format = { Container::WAV, Codec::PCM }, .extensions = wavExtensions },
            AudioFormatEntry{ .format = { Container::WavPack, Codec::WavPack }, .extensions = wavPackExtensions },
        };
    } // namespace

    void visitAudioFormats(const std::function<core::VisitorResult(const AudioFormat&, ExtensionSpan)>& visitor)
    {
        for (const AudioFormatEntry& entry : audioFormatEntries)
        {
            if (visitor(entry.format, entry.extensions) == core::Break)
                break;
        }
    }

    void visitAudioFormatsForExtension(std::string_view extension, const std::function<void(const AudioFormat&)>& visitor)
    {
        assert(extension.size() > 1 && extension.at(0) == '.');

        for (const AudioFormatEntry& entry : audioFormatEntries)
        {
            if (std::find(std::cbegin(entry.extensions), std::cend(entry.extensions), extension) != std::cend(entry.extensions))
                visitor(entry.format);
        }
    }

    ExtensionSpan getExtensionsForAudioFormat(AudioFormat format)
    {
        for (const AudioFormatEntry& entry : audioFormatEntries)
        {
            if (entry.format.container == format.container && entry.format.codec == format.codec)
                return entry.extensions;
        }

        return {};
    }
} // namespace lms::core::media
