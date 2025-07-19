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

#include <taglib/aifffile.h>
#include <taglib/apefile.h>
#include <taglib/asffile.h>
#include <taglib/audioproperties.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2framefactory.h>
#include <taglib/mp4file.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/opusfile.h>
#include <taglib/speexfile.h>
#include <taglib/tfile.h>
#include <taglib/tfilestream.h>
#include <taglib/trueaudiofile.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>
#if TAGLIB_HAS_DSF
    #include <taglib/dsffile.h>
#endif

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

namespace lms::metadata::taglib::utils
{
    std::span<const std::filesystem::path> getSupportedExtensions()
    {
        static const std::vector<std::filesystem::path> supportedExtensions
        {
            ".mp3", ".mp2", ".aac", ".ogg", ".oga", ".flac", ".spx", ".opus",
                ".mpc", ".wv", ".ape", ".tta", ".m4a", ".m4r", ".m4b", ".m4p",
                ".3g2", ".m4v", ".wma", ".asf", ".aif", ".aiff", ".afc", ".aifc",
                ".wav",
#if TAGLIB_HAS_DSF
                ".dsf", ".dff", ".dsdiff"
#endif
        };

        return std::span<const std::filesystem::path>{ supportedExtensions };
    }

    TagLib::AudioProperties::ReadStyle readStyleToTagLibReadStyle(ParserReadStyle readStyle)
    {
        switch (readStyle)
        {
        case ParserReadStyle::Fast:
            return TagLib::AudioProperties::ReadStyle::Fast;
        case ParserReadStyle::Average:
            return TagLib::AudioProperties::ReadStyle::Average;
        case ParserReadStyle::Accurate:
            return TagLib::AudioProperties::ReadStyle::Accurate;
        }

        throw Exception{ "Cannot convert read style" };
    }

    TagLib::FileStream createFileStream(const std::filesystem::path& p)
    {
        FILE* file{ std::fopen(p.c_str(), "r") };
        if (!file)
        {
            const std::error_code ec{ errno, std::generic_category() };
            LMS_LOG(METADATA, DEBUG, "fopen failed for " << p << ": " << ec.message());
            throw IOException{ "fopen failed", ec };
        }

        int fd{ ::fileno(file) };
        if (fd == -1)
        {
            const std::error_code ec{ errno, std::generic_category() };
            LMS_LOG(METADATA, DEBUG, "fileno failed for " << p << ": " << ec.message());
            throw IOException{ "fileno failed", ec };
        }

        return TagLib::FileStream{ fd, true };
    }

    std::unique_ptr<TagLib::File> parseFileByExtension(TagLib::FileStream* stream, const std::filesystem::path& extension, TagLib::AudioProperties::ReadStyle audioPropertiesStyle, bool readAudioProperties)
    {
        const std::string ext{ core::stringUtils::stringToUpper(extension.string().substr(1)) };

        std::unique_ptr<TagLib::File> file;

        // MP3
        if (ext == "MP3" || ext == "MP2" || ext == "AAC")
            file = std::make_unique<TagLib::MPEG::File>(stream, TagLib::ID3v2::FrameFactory::instance(), readAudioProperties, audioPropertiesStyle);
        // VORBIS
        else if (ext == "OGG")
            file = std::make_unique<TagLib::Ogg::Vorbis::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (ext == "OGA")
        {
            /* .oga can be any audio in the Ogg container. First try FLAC, then Vorbis. */
            file = std::make_unique<TagLib::Ogg::FLAC::File>(stream, readAudioProperties, audioPropertiesStyle);
            if (!file->isValid())
                file = std::make_unique<TagLib::Ogg::Vorbis::File>(stream, readAudioProperties, audioPropertiesStyle);
        }
        else if (ext == "FLAC")
            file = std::make_unique<TagLib::FLAC::File>(stream, TagLib::ID3v2::FrameFactory::instance(), readAudioProperties, audioPropertiesStyle);
        else if (ext == "SPX")
            file = std::make_unique<TagLib::Ogg::Speex::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (ext == "OPUS")
            file = std::make_unique<TagLib::Ogg::Opus::File>(stream, readAudioProperties, audioPropertiesStyle);
        // APE
        else if (ext == "MPC")
            file = std::make_unique<TagLib::MPC::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (ext == "WV")
            file = std::make_unique<TagLib::WavPack::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (ext == "APE")
            file = std::make_unique<TagLib::APE::File>(stream, readAudioProperties, audioPropertiesStyle);
        // TRUEAUDIO
        else if (ext == "TTA")
            file = std::make_unique<TagLib::TrueAudio::File>(stream, readAudioProperties, audioPropertiesStyle);
        // MP4
        else if (ext == "M4A" || ext == "M4R" || ext == "M4B" || ext == "M4P" || ext == "MP4" || ext == "3G2" || ext == "M4V")
            file = std::make_unique<TagLib::MP4::File>(stream, readAudioProperties, audioPropertiesStyle);
        // ASF
        else if (ext == "WMA" || ext == "ASF")
            file = std::make_unique<TagLib::ASF::File>(stream, readAudioProperties, audioPropertiesStyle);
        // RIFF
        else if (ext == "AIF" || ext == "AIFF" || ext == "AFC" || ext == "AIFC")
            file = std::make_unique<TagLib::RIFF::AIFF::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (ext == "WAV")
            file = std::make_unique<TagLib::RIFF::WAV::File>(stream, readAudioProperties, audioPropertiesStyle);
#if TAGLIB_HAS_DSF
        else if (ext == "DSF")
            file = std::make_unique<TagLib::DSF::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (ext == "DFF" || ext == "DSDIFF")
            file = std::make_unique<TagLib::DSDIFF::File>(stream, readAudioProperties, audioPropertiesStyle);
#endif

        if (file && !file->isValid())
            file.reset();

        return file;
    }

    std::unique_ptr<TagLib::File> parseFileByContent(TagLib::FileStream* stream, TagLib::AudioProperties::ReadStyle audioPropertiesStyle, bool readAudioProperties)
    {
        std::unique_ptr<TagLib::File> file;

        if (TagLib::MPEG::File::isSupported(stream))
            file = std::make_unique<TagLib::MPEG::File>(stream, TagLib::ID3v2::FrameFactory::instance(), readAudioProperties, audioPropertiesStyle);
        // VORBIS
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
        // APE
        else if (TagLib::MPC::File::isSupported(stream))
            file = std::make_unique<TagLib::MPC::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (TagLib::WavPack::File::isSupported(stream))
            file = std::make_unique<TagLib::WavPack::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (TagLib::APE::File::isSupported(stream))
            file = std::make_unique<TagLib::APE::File>(stream, readAudioProperties, audioPropertiesStyle);
        // TRUEAUDIO
        else if (TagLib::TrueAudio::File::isSupported(stream))
            file = std::make_unique<TagLib::TrueAudio::File>(stream, readAudioProperties, audioPropertiesStyle);
        // MP4
        else if (TagLib::MP4::File::isSupported(stream))
            file = std::make_unique<TagLib::MP4::File>(stream, readAudioProperties, audioPropertiesStyle);
        //_ASF
        else if (TagLib::ASF::File::isSupported(stream))
            file = std::make_unique<TagLib::ASF::File>(stream, readAudioProperties, audioPropertiesStyle);
        // RIFF
        else if (TagLib::RIFF::AIFF::File::isSupported(stream))
            file = std::make_unique<TagLib::RIFF::AIFF::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (TagLib::RIFF::WAV::File::isSupported(stream))
            file = std::make_unique<TagLib::RIFF::WAV::File>(stream, readAudioProperties, audioPropertiesStyle);
#if TAGLIB_HAS_DSF
        else if (DSF::File::isSupported(stream))
            file = std::make_unique<TagLib::DSF::File>(stream, readAudioProperties, audioPropertiesStyle);
        else if (DSDIFF::File::isSupported(stream))
            file = std::make_unique<TagLib::DSDIFF::File>(stream, readAudioProperties, audioPropertiesStyle);
#endif

        if (file && !file->isValid())
            file.reset();

        return file;
    }

    std::unique_ptr<TagLib::File> parseFile(const std::filesystem::path& p, TagLib::AudioProperties::ReadStyle readStyle, ReadAudioProperties readAudioProperties)
    {
        LMS_SCOPED_TRACE_DETAILED("MetaData", "TagLibParseFile");

        TagLib::FileStream fileStream{ createFileStream(p) };
        std::unique_ptr<TagLib::File> file{ parseFileByExtension(&fileStream, p.extension(), readStyle, readAudioProperties.value()) };
        if (!file)
        {
            LMS_LOG(METADATA, DEBUG, "File " << p << ": failed to parse by extension");
            file = parseFileByContent(&fileStream, readStyle, readAudioProperties.value());
            if (!file)
                LMS_LOG(METADATA, DEBUG, "File " << p << ": failed to parse by content");
        }

        return file;
    }
} // namespace lms::metadata::taglib::utils