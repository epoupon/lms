/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "AudioFileInfo.hpp"

#include <cassert>

#include "TagLibDefs.hpp"

#include <taglib/aifffile.h>
#include <taglib/apefile.h>
#include <taglib/asffile.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/trueaudiofile.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>

#if LMS_TAGLIB_HAS_DSF
    #include <taglib/dsffile.h>
#endif
#if LMS_TAGLIB_HAS_SHORTEN
    #include <taglib/shortenfile.h>
#endif

#include "audio/IAudioFileInfo.hpp"

#include "Utils.hpp"

namespace lms::audio::taglib
{
    namespace
    {
        AudioProperties computeAudioProperties(const ::TagLib::File& file)
        {
            assert(file.audioProperties());
            const ::TagLib::AudioProperties& properties{ *file.audioProperties() };

            AudioProperties audioProperties;

            // Common properties
            audioProperties.bitrate = static_cast<std::size_t>(properties.bitrate() * 1000);
            audioProperties.channelCount = static_cast<std::size_t>(properties.channels());
            audioProperties.duration = std::chrono::milliseconds{ properties.lengthInMilliseconds() };
            audioProperties.sampleRate = static_cast<std::size_t>(properties.sampleRate());

            // Guess container from the file type
            if (const auto* apeFile{ dynamic_cast<const ::TagLib::APE::File*>(&file) })
            {
                audioProperties.container = ContainerType::APE;
                audioProperties.codec = CodecType::APE; // TODO version?
                audioProperties.bitsPerSample = apeFile->audioProperties()->bitsPerSample();
            }
            else if (const auto* asfFile{ dynamic_cast<const ::TagLib::ASF::File*>(&file) })
            {
                audioProperties.container = ContainerType::ASF;

                switch (asfFile->audioProperties()->codec())
                {
                case ::TagLib::ASF::Properties::Codec::WMA1:
                    audioProperties.codec = CodecType::WMA1;
                    break;
                case ::TagLib::ASF::Properties::Codec::WMA2:
                    audioProperties.codec = CodecType::WMA2;
                    break;
                case ::TagLib::ASF::Properties::Codec::WMA9Lossless:
                    audioProperties.codec = CodecType::WMA9Lossless;
                    break;
                case ::TagLib::ASF::Properties::Codec::WMA9Pro:
                    audioProperties.codec = CodecType::WMA9Pro;
                    break;
                case ::TagLib::ASF::Properties::Codec::Unknown:
                    audioProperties.codec = std::nullopt;
                    break;
                }

                audioProperties.bitsPerSample = asfFile->audioProperties()->bitsPerSample();
            }
#if LMS_TAGLIB_HAS_DSF
            else if (const auto* dsfFile{ dynamic_cast<const ::TagLib::DSF::File*>(&file) })
            {
                audioProperties.container = ContainerType::DSF;
                audioProperties.codec = CodecType::DSD;
                audioProperties.bitsPerSample = dsfFile->audioProperties()->bitsPerSample();
            }
#endif // LMS_TAGLIB_HAS_DSF
            else if (const auto* flacFile{ dynamic_cast<const ::TagLib::FLAC::File*>(&file) })
            {
                audioProperties.container = ContainerType::FLAC;
                audioProperties.codec = CodecType::FLAC;
                audioProperties.bitsPerSample = flacFile->audioProperties()->bitsPerSample();
            }
            else if (const auto* mp4File{ dynamic_cast<const ::TagLib::MP4::File*>(&file) })
            {
                audioProperties.container = ContainerType::MP4;
                switch (mp4File->audioProperties()->codec())
                {
                case ::TagLib::MP4::Properties::Codec::AAC:
                    audioProperties.codec = CodecType::AAC;
                    break;
                case ::TagLib::MP4::Properties::Codec::ALAC:
                    audioProperties.codec = CodecType::ALAC;
                    break;
                case ::TagLib::MP4::Properties::Codec::Unknown:
                    audioProperties.codec = std::nullopt;
                    break;
                }

                audioProperties.bitsPerSample = mp4File->audioProperties()->bitsPerSample();
            }
            else if (const auto* mpcFile{ dynamic_cast<const ::TagLib::MPC::File*>(&file) })
            {
                audioProperties.container = ContainerType::MPC;

                switch (mpcFile->audioProperties()->mpcVersion())
                {
                case 7:
                    audioProperties.codec = CodecType::MPC7;
                    break;
                case 8:
                    audioProperties.codec = CodecType::MPC8;
                    break;
                }
            }
            else if (const auto* mpegFile{ dynamic_cast<const ::TagLib::MPEG::File*>(&file) })
            {
                const auto& properties{ *mpegFile->audioProperties() };

                audioProperties.container = ContainerType::MPEG;
                if ((properties.version() == TagLib::MPEG::Header::Version::Version1 || properties.version() == TagLib::MPEG::Header::Version::Version2 || properties.version() == TagLib::MPEG::Header::Version::Version2_5)
                    && mpegFile->audioProperties()->layer() == 3)
                    audioProperties.codec = CodecType::MP3;     // could be MPEG-1 layer 3 or MPEG-2(.5) layer 3
                else if (mpegFile->audioProperties()->isADTS()) // likely AAC
                    audioProperties.codec = CodecType::AAC;
            }
            else if (dynamic_cast<const ::TagLib::Ogg::Opus::File*>(&file))
            {
                audioProperties.container = ContainerType::Ogg;
                audioProperties.codec = CodecType::Opus;
            }
            else if (dynamic_cast<const ::TagLib::Ogg::Vorbis::File*>(&file))
            {
                audioProperties.container = ContainerType::Ogg;
                audioProperties.codec = CodecType::Vorbis;
            }
            else if (const auto* aiffFile{ dynamic_cast<const ::TagLib::RIFF::AIFF::File*>(&file) })
            {
                audioProperties.container = ContainerType::AIFF;
                audioProperties.codec = CodecType::PCM;
                audioProperties.bitsPerSample = aiffFile->audioProperties()->bitsPerSample();
            }
            else if (const auto* wavFile{ dynamic_cast<const ::TagLib::RIFF::WAV::File*>(&file) })
            {
                audioProperties.container = ContainerType::WAV;
                audioProperties.codec = CodecType::PCM;
                audioProperties.bitsPerSample = wavFile->audioProperties()->bitsPerSample();
            }
#if LMS_TAGLIB_HAS_SHORTEN
            else if (const auto* shortenFile{ dynamic_cast<const ::TagLib::Shorten::File*>(&file) })
            {
                audioProperties.container = ContainerType::Shorten;
                audioProperties.codec = CodecType::Shorten;
                audioProperties.bitsPerSample = shortenFile->audioProperties()->bitsPerSample();
            }
#endif // LMS_TAGLIB_HAS_SHORTEN
            else if (const auto* trueAudioFile{ dynamic_cast<const ::TagLib::TrueAudio::File*>(&file) })
            {
                audioProperties.container = ContainerType::TrueAudio;
                audioProperties.codec = CodecType::TrueAudio;
                audioProperties.bitsPerSample = trueAudioFile->audioProperties()->bitsPerSample();
            }
            else if (const auto* wavPackFile{ dynamic_cast<const ::TagLib::WavPack::File*>(&file) })
            {
                audioProperties.container = ContainerType::WavPack;
                audioProperties.codec = CodecType::WavPack;
                audioProperties.bitsPerSample = wavPackFile->audioProperties()->bitsPerSample();
            }

            return audioProperties;
        }
    } // namespace

    AudioFileInfo::AudioFileInfo(const std::filesystem::path& filePath, ParserOptions::AudioPropertiesReadStyle readStyle, bool enableExtraDebugLogs)
        : _filePath{ filePath }
        , _file{ utils::parseFile(filePath, readStyle) }
        , _audioProperties{ computeAudioProperties(*_file) }
        , _tagReader{ *_file, enableExtraDebugLogs }
        , _imageReader{ *_file }
    {
    }

    const AudioProperties& AudioFileInfo::getAudioProperties() const
    {
        return _audioProperties;
    }

    const IImageReader& AudioFileInfo::getImageReader() const
    {
        return _imageReader;
    }

    const ITagReader& AudioFileInfo::getTagReader() const
    {
        return _tagReader;
    }

} // namespace lms::audio::taglib
