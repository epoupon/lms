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

#include "core/media/ContainerCodec.hpp"

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

        // order is important
        constexpr std::array containerCodecPairs{
            ContainerCodec{ Container::AIFF, Codec::PCM, aiffExtensions },
            ContainerCodec{ Container::APE, Codec::APE, apeExtensions },
            ContainerCodec{ Container::ASF, Codec::WMA1, wmaExtensions },
            ContainerCodec{ Container::ASF, Codec::WMA2, wmaExtensions },
            ContainerCodec{ Container::ASF, Codec::WMA9Pro, wmaExtensions },
            ContainerCodec{ Container::ASF, Codec::WMA9Lossless, wmaExtensions },
            ContainerCodec{ Container::DSF, Codec::DSD, dsfExtensions },
            ContainerCodec{ Container::FLAC, Codec::FLAC, flacExtensions },
            ContainerCodec{ Container::MP4, Codec::AAC, mp4AacExtensions },
            ContainerCodec{ Container::MP4, Codec::ALAC, mp4AlacExtensions },
            ContainerCodec{ Container::MPC, Codec::MPC7, mpcExtensions },
            ContainerCodec{ Container::MPC, Codec::MPC8, mpcExtensions },
            ContainerCodec{ Container::MPEG, Codec::MP3, mp3Extensions },
            ContainerCodec{ Container::MPEG, Codec::AAC, rawAacExtensions }, // raw ADTS AAC has no container of its own
            ContainerCodec{ Container::Ogg, Codec::FLAC, oggFlacExtensions },
            ContainerCodec{ Container::Ogg, Codec::Vorbis, oggVorbisExtensions },
            ContainerCodec{ Container::Ogg, Codec::Opus, oggOpusExtensions },
            ContainerCodec{ Container::Shorten, Codec::Shorten, shortenExtensions },
            ContainerCodec{ Container::TrueAudio, Codec::TrueAudio, ttaExtensions },
            ContainerCodec{ Container::WAV, Codec::PCM, wavExtensions },
            ContainerCodec{ Container::WavPack, Codec::WavPack, wavPackExtensions },
        };

    } // namespace

    void visitContainerCodecPairs(const std::function<void(const ContainerCodec&)>& visitor)
    {
        std::for_each(std::cbegin(containerCodecPairs), std::cend(containerCodecPairs), visitor);
    }

    void visitContainerCodecPairsForExtension(std::string_view extension, const std::function<void(const ContainerCodec&)>& visitor)
    {
        assert(extension.size() > 1 && extension.at(0) == '.');

        for (const ContainerCodec& pair : containerCodecPairs)
        {
            if (std::find(std::cbegin(pair.extensions), std::cend(pair.extensions), extension) != std::cend(pair.extensions))
                visitor(pair);
        }
    }
} // namespace lms::core::media
