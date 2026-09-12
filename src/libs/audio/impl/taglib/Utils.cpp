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

#include "Utils.hpp"

#include "TagLibDefs.hpp"

#include <algorithm>
#include <vector>

#include <taglib/audioproperties.h>
#include <taglib/id3v2framefactory.h>
#include <taglib/mpegfile.h>
#include <taglib/tfile.h>
#include <taglib/tfilestream.h>

#if LMS_TAGLIB_HAS_RIFF
    #include <taglib/aifffile.h>
    #include <taglib/wavfile.h>
#endif
#if LMS_TAGLIB_HAS_APE
    #include <taglib/apefile.h>
    #include <taglib/mpcfile.h>
    #include <taglib/wavpackfile.h>
#endif
#if LMS_TAGLIB_HAS_ASF
    #include <taglib/asffile.h>
#endif
#if LMS_TAGLIB_HAS_VORBIS
    #include <taglib/flacfile.h>
    #include <taglib/oggflacfile.h>
    #include <taglib/opusfile.h>
    #include <taglib/speexfile.h>
    #include <taglib/vorbisfile.h>
#endif
#if LMS_TAGLIB_HAS_MP4
    #include <taglib/mp4file.h>
#endif
#if LMS_TAGLIB_HAS_TRUEAUDIO
    #include <taglib/trueaudiofile.h>
#endif
#if LMS_TAGLIB_HAS_DSF
    #include <taglib/dsffile.h>
#endif
#if LMS_TAGLIB_HAS_SHORTEN
    #include <taglib/shortenfile.h>
#endif

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/String.hpp"
#include "core/media/AudioFormat.hpp"

#include "audio/Exception.hpp"

namespace lms::audio::taglib::utils
{
    namespace
    {
        // Mirrors which #if LMS_TAGLIB_HAS_* block below actually compiles support for each container in
        constexpr bool isContainerSupported(core::media::Container container)
        {
            switch (container)
            {
            case core::media::Container::MPEG:
                return true;
            case core::media::Container::Ogg:
            case core::media::Container::FLAC:
                return LMS_TAGLIB_HAS_VORBIS;
            case core::media::Container::APE:
            case core::media::Container::MPC:
            case core::media::Container::WavPack:
                return LMS_TAGLIB_HAS_APE;
            case core::media::Container::TrueAudio:
                return LMS_TAGLIB_HAS_TRUEAUDIO;
            case core::media::Container::MP4:
                return LMS_TAGLIB_HAS_MP4;
            case core::media::Container::ASF:
                return LMS_TAGLIB_HAS_ASF;
            case core::media::Container::AIFF:
            case core::media::Container::WAV:
                return LMS_TAGLIB_HAS_RIFF;
            case core::media::Container::DSF:
                return LMS_TAGLIB_HAS_DSF;
            case core::media::Container::Shorten:
                return LMS_TAGLIB_HAS_SHORTEN;
            }

            return false;
        }

        std::unique_ptr<TagLib::File> createFile(TagLib::FileStream* stream, core::media::Container container, core::media::Codec codec, bool readAudioProperties, TagLib::AudioProperties::ReadStyle style)
        {
            using core::media::Codec;
            using core::media::Container;

            if (container == Container::MPEG)
                return std::make_unique<TagLib::MPEG::File>(stream, TagLib::ID3v2::FrameFactory::instance(), readAudioProperties, style);
#if LMS_TAGLIB_HAS_VORBIS
            if (container == Container::Ogg && codec == Codec::Vorbis)
                return std::make_unique<TagLib::Ogg::Vorbis::File>(stream, readAudioProperties, style);
            if (container == Container::Ogg && codec == Codec::FLAC)
                return std::make_unique<TagLib::Ogg::FLAC::File>(stream, readAudioProperties, style);
            if (container == Container::Ogg && codec == Codec::Opus)
                return std::make_unique<TagLib::Ogg::Opus::File>(stream, readAudioProperties, style);
            if (container == Container::FLAC)
                return std::make_unique<TagLib::FLAC::File>(stream, TagLib::ID3v2::FrameFactory::instance(), readAudioProperties, style);
#endif // LMS_TAGLIB_HAS_VORBIS
#if LMS_TAGLIB_HAS_APE
            if (container == Container::MPC)
                return std::make_unique<TagLib::MPC::File>(stream, readAudioProperties, style);
            if (container == Container::WavPack)
                return std::make_unique<TagLib::WavPack::File>(stream, readAudioProperties, style);
            if (container == Container::APE)
                return std::make_unique<TagLib::APE::File>(stream, readAudioProperties, style);
#endif // LMS_TAGLIB_HAS_APE
#if LMS_TAGLIB_HAS_TRUEAUDIO
            if (container == Container::TrueAudio)
                return std::make_unique<TagLib::TrueAudio::File>(stream, readAudioProperties, style);
#endif // LMS_TAGLIB_HAS_TRUEAUDIO
#if LMS_TAGLIB_HAS_MP4
            if (container == Container::MP4)
                return std::make_unique<TagLib::MP4::File>(stream, readAudioProperties, style);
#endif // LMS_TAGLIB_HAS_MP4
#if LMS_TAGLIB_HAS_ASF
            if (container == Container::ASF)
                return std::make_unique<TagLib::ASF::File>(stream, readAudioProperties, style);
#endif // LMS_TAGLIB_HAS_ASF
#if LMS_TAGLIB_HAS_RIFF
            if (container == Container::AIFF)
                return std::make_unique<TagLib::RIFF::AIFF::File>(stream, readAudioProperties, style);
            if (container == Container::WAV)
                return std::make_unique<TagLib::RIFF::WAV::File>(stream, readAudioProperties, style);
#endif // LMS_TAGLIB_HAS_RIFF
#if LMS_TAGLIB_HAS_DSF
            if (container == Container::DSF)
                return std::make_unique<TagLib::DSF::File>(stream, readAudioProperties, style);
#endif // LMS_TAGLIB_HAS_DSF
#if LMS_TAGLIB_HAS_SHORTEN
            if (container == Container::Shorten)
                return std::make_unique<TagLib::Shorten::File>(stream, readAudioProperties, style);
#endif // LMS_TAGLIB_HAS_SHORTEN

            return nullptr;
        }

        std::vector<std::filesystem::path> buildSupportedExtensions()
        {
            std::vector<std::filesystem::path> result;

            core::media::visitAudioFormats([&](const core::media::AudioFormat& format, core::media::ExtensionSpan extensions) {
                if (!isContainerSupported(format.container))
                    return core::Continue;

                for (const std::string_view extension : extensions)
                {
                    if (std::find(std::cbegin(result), std::cend(result), extension) == std::cend(result))
                        result.emplace_back(extension);
                }

                return core::Continue;
            });

            return result;
        }
    } // namespace

    std::span<const std::filesystem::path> getSupportedExtensions()
    {
        static const std::vector<std::filesystem::path> supportedExtensions{ buildSupportedExtensions() };

        return std::span<const std::filesystem::path>{ supportedExtensions };
    }

    TagLib::AudioProperties::ReadStyle readStyleToTagLibReadStyle(AudioFileInfoParseOptions::AudioPropertiesReadStyle readStyle)
    {
        switch (readStyle)
        {
        case AudioFileInfoParseOptions::AudioPropertiesReadStyle::Fast:
            return TagLib::AudioProperties::ReadStyle::Fast;
        case AudioFileInfoParseOptions::AudioPropertiesReadStyle::Average:
            return TagLib::AudioProperties::ReadStyle::Average;
        case AudioFileInfoParseOptions::AudioPropertiesReadStyle::Accurate:
            return TagLib::AudioProperties::ReadStyle::Accurate;
        }

        throw Exception{ "Cannot convert read style" };
    }

    std::unique_ptr<TagLib::FileStream> createFileStream(const std::filesystem::path& p)
    {
        FILE* file{ std::fopen(p.c_str(), "r") };
        if (!file)
        {
            const std::error_code ec{ errno, std::generic_category() };
            LMS_LOG(METADATA, DEBUG, "fopen failed for " << p << ": " << ec.message());
            throw IOFileException{ p, "fopen failed", ec };
        }

        int fd{ ::fileno(file) };
        if (fd == -1)
        {
            const std::error_code ec{ errno, std::generic_category() };
            LMS_LOG(METADATA, DEBUG, "fileno failed for " << p << ": " << ec.message());
            throw IOFileException{ p, "fileno failed", ec };
        }

        return std::make_unique<TagLib::FileStream>(fd, true);
    }

    std::unique_ptr<TagLib::File> parseFileByExtension(TagLib::FileStream* stream, const std::filesystem::path& extension, TagLib::AudioProperties::ReadStyle audioPropertiesStyle)
    {
        constexpr bool readAudioProperties{ true };
        std::unique_ptr<TagLib::File> file;

        if (extension.empty())
            return file;

        const std::string ext{ core::stringUtils::stringToLower(extension.string()) };

        // Extensions can be ambiguous (e.g. .oga is both Ogg+FLAC and Ogg+Vorbis): try every candidate
        // pairing for this extension, in table order, until one actually validates
        core::media::visitAudioFormatsForExtension(ext, [&](const core::media::AudioFormat& format) {
            if (file || !isContainerSupported(format.container))
                return;

            std::unique_ptr<TagLib::File> candidate{ createFile(stream, format.container, format.codec, readAudioProperties, audioPropertiesStyle) };
            if (candidate && candidate->isValid())
                file = std::move(candidate);
        });

        return file;
    }

    std::unique_ptr<TagLib::File> parseFileByContent(TagLib::FileStream* stream, TagLib::AudioProperties::ReadStyle audioPropertiesStyle)
    {
        constexpr bool readAudioProperties{ true };
        std::unique_ptr<TagLib::File> file;

        if (TagLib::MPEG::File::isSupported(stream))
            file = std::make_unique<TagLib::MPEG::File>(stream, TagLib::ID3v2::FrameFactory::instance(), readAudioProperties, audioPropertiesStyle);
#if LMS_TAGLIB_HAS_VORBIS
        else if (TagLib::Ogg::Vorbis::File::isSupported(stream))
            file = std::make_unique<TagLib::Ogg::Vorbis::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (TagLib::Ogg::FLAC::File::isSupported(stream))
            file = std::make_unique<TagLib::Ogg::FLAC::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (TagLib::FLAC::File::isSupported(stream))
            file = std::make_unique<TagLib::FLAC::File>(stream, TagLib::ID3v2::FrameFactory::instance(), readAudioProperties, audioPropertiesStyle);
        else if (TagLib::Ogg::Speex::File::isSupported(stream))
            file = std::make_unique<TagLib::Ogg::Speex::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (TagLib::Ogg::Opus::File::isSupported(stream))
            file = std::make_unique<TagLib::Ogg::Opus::File>(stream, readAudioProperties, audioPropertiesStyle);
#endif // LMS_TAGLIB_HAS_VORBIS
#if LMS_TAGLIB_HAS_APE
        else if (TagLib::MPC::File::isSupported(stream))
            file = std::make_unique<TagLib::MPC::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (TagLib::WavPack::File::isSupported(stream))
            file = std::make_unique<TagLib::WavPack::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (TagLib::APE::File::isSupported(stream))
            file = std::make_unique<TagLib::APE::File>(stream, readAudioProperties, audioPropertiesStyle);
#endif // LMS_TAGLIB_HAS_APE
#if LMS_TAGLIB_HAS_TRUEAUDIO
        else if (TagLib::TrueAudio::File::isSupported(stream))
            file = std::make_unique<TagLib::TrueAudio::File>(stream, readAudioProperties, audioPropertiesStyle);
#endif // LMS_TAGLIB_HAS_TRUEAUDIO
#if LMS_TAGLIB_HAS_MP4
        else if (TagLib::MP4::File::isSupported(stream))
            file = std::make_unique<TagLib::MP4::File>(stream, readAudioProperties, audioPropertiesStyle);
#endif // LMS_TAGLIB_HAS_MP4
#if LMS_TAGLIB_HAS_ASF
        else if (TagLib::ASF::File::isSupported(stream))
            file = std::make_unique<TagLib::ASF::File>(stream, readAudioProperties, audioPropertiesStyle);
#endif // LMS_TAGLIB_HAS_ASF
#if LMS_TAGLIB_HAS_RIFF
        else if (TagLib::RIFF::AIFF::File::isSupported(stream))
            file = std::make_unique<TagLib::RIFF::AIFF::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (TagLib::RIFF::WAV::File::isSupported(stream))
            file = std::make_unique<TagLib::RIFF::WAV::File>(stream, readAudioProperties, audioPropertiesStyle);
#endif // LMS_TAGLIB_HAS_RIFF
#if LMS_TAGLIB_HAS_DSF
        else if (TagLib::DSF::File::isSupported(stream))
            file = std::make_unique<TagLib::DSF::File>(stream, readAudioProperties, audioPropertiesStyle);
#endif // LMS_TAGLIB_HAS_DSF
#if LMS_TAGLIB_HAS_SHORTEN
        else if (TagLib::Shorten::File::isSupported(stream))
            file = std::make_unique<TagLib::Shorten::File>(stream, readAudioProperties, audioPropertiesStyle);
#endif // LMS_TAGLIB_HAS_SHORTEN

        if (file && !file->isValid())
            file.reset();

        return file;
    }

    FileDesc parseFile(const std::filesystem::path& p, AudioFileInfoParseOptions::AudioPropertiesReadStyle readStyle)
    {
        LMS_SCOPED_TRACE_DETAILED("MetaData", "TagLibParseFile");

        const ::TagLib::AudioProperties::ReadStyle tagLibReadStyle{ readStyleToTagLibReadStyle(readStyle) };

        std::unique_ptr<TagLib::FileStream> fileStream{ createFileStream(p) };
        assert(fileStream);
        std::unique_ptr<TagLib::File> file{ parseFileByExtension(fileStream.get(), p.extension(), tagLibReadStyle) };
        if (!file)
        {
            LMS_LOG(METADATA, DEBUG, "File " << p << ": failed to parse by extension");
            file = parseFileByContent(fileStream.get(), tagLibReadStyle);
            if (!file)
                LMS_LOG(METADATA, DEBUG, "File " << p << ": failed to parse by content");
        }

        if (!file)
            throw Exception{ "Parsing failed" };

        return FileDesc{ .fileStream = std::move(fileStream), .file = std::move(file) };
    }
} // namespace lms::audio::taglib::utils