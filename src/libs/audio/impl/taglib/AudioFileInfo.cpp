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

#include <taglib/mpegfile.h>

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
    #include <taglib/opusfile.h>
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

#include "audio/AudioProperties.hpp"
#include "audio/IAudioFileInfo.hpp"
#include "audio/IAudioFileInfoParser.hpp"

#include "ImageReader.hpp"
#include "TagReader.hpp"
#include "Utils.hpp"

namespace lms::audio::taglib
{
    namespace
    {
        std::optional<AudioProperties> computeAudioProperties(const ::TagLib::File& file, const std::filesystem::path& filePath)
        {
            assert(file.audioProperties());

            AudioProperties audioProperties;

            {
                const ::TagLib::AudioProperties* properties{ file.audioProperties() };
                if (!properties)
                {
                    LMS_LOG(AUDIO, DEBUG, "Cannot determine audio properties in " << filePath);
                    return std::nullopt;
                }

                // Common properties
                audioProperties.bitrate = static_cast<std::size_t>(properties->bitrate() * 1000);
                if (audioProperties.bitrate == 0)
                {
                    LMS_LOG(AUDIO, DEBUG, "Cannot determine bitrate in " << filePath);
                    return std::nullopt;
                }

                audioProperties.channelCount = static_cast<std::size_t>(properties->channels());
                if (audioProperties.channelCount == 0)
                {
                    LMS_LOG(AUDIO, DEBUG, "Cannot determine channel count in " << filePath);
                    return std::nullopt;
                }

                audioProperties.duration = std::chrono::milliseconds{ properties->lengthInMilliseconds() };
                if (audioProperties.duration == decltype(audioProperties.duration)::zero())
                {
                    LMS_LOG(AUDIO, DEBUG, "Cannot determine duration in " << filePath);
                    return std::nullopt;
                }

                audioProperties.sampleRate = static_cast<std::size_t>(properties->sampleRate());
                if (audioProperties.sampleRate == 0)
                {
                    LMS_LOG(AUDIO, DEBUG, "Cannot determine sample rate in " << filePath);
                    return std::nullopt;
                }
            }

            // Guess container from the file type
            if (const auto* mpegFile{ dynamic_cast<const ::TagLib::MPEG::File*>(&file) })
            {
                const auto& properties{ *mpegFile->audioProperties() };

                audioProperties.container = core::media::Container::MPEG;
                if ((properties.version() == TagLib::MPEG::Header::Version::Version1 || properties.version() == TagLib::MPEG::Header::Version::Version2 || properties.version() == TagLib::MPEG::Header::Version::Version2_5)
                    && mpegFile->audioProperties()->layer() == 3)
                    audioProperties.codec = core::media::Codec::MP3; // could be MPEG-1 layer 3 or MPEG-2(.5) layer 3
                else if (mpegFile->audioProperties()->isADTS())      // likely AAC
                    audioProperties.codec = core::media::Codec::AAC;
                else
                {
                    LMS_LOG(AUDIO, DEBUG, "Unhandled MPEG codec in " << filePath);
                    return std::nullopt;
                }
            }
#if LMS_TAGLIB_HAS_RIFF
            else if (const auto* aiffFile{ dynamic_cast<const ::TagLib::RIFF::AIFF::File*>(&file) })
            {
                // We don't check for potential Aiff-C format here, we considerer it PCM for now
                audioProperties.container = core::media::Container::AIFF;
                audioProperties.codec = core::media::Codec::PCM;
                audioProperties.bitsPerSample = aiffFile->audioProperties()->bitsPerSample();
            }
            else if (const auto* wavFile{ dynamic_cast<const ::TagLib::RIFF::WAV::File*>(&file) })
            {
                // We don't check for format here, we considerer it PCM for now
                audioProperties.container = core::media::Container::WAV;
                audioProperties.codec = core::media::Codec::PCM;
                audioProperties.bitsPerSample = wavFile->audioProperties()->bitsPerSample();
            }
#endif // LMS_TAGLIB_HAS_RIFF
#if LMS_TAGLIB_HAS_APE
            else if (const auto* apeFile{ dynamic_cast<const ::TagLib::APE::File*>(&file) })
            {
                audioProperties.container = core::media::Container::APE;
                audioProperties.codec = core::media::Codec::APE; // TODO version?
                audioProperties.bitsPerSample = apeFile->audioProperties()->bitsPerSample();
            }
            else if (const auto* mpcFile{ dynamic_cast<const ::TagLib::MPC::File*>(&file) })
            {
                audioProperties.container = core::media::Container::MPC;

                switch (mpcFile->audioProperties()->mpcVersion())
                {
                case 7:
                    audioProperties.codec = core::media::Codec::MPC7;
                    break;
                case 8:
                    audioProperties.codec = core::media::Codec::MPC8;
                    break;
                default:
                    LMS_LOG(AUDIO, DEBUG, "Unhandled MPC codec version " << mpcFile->audioProperties()->mpcVersion() << " in " << filePath);
                    return std::nullopt;
                }
            }
            else if (const auto* wavPackFile{ dynamic_cast<const ::TagLib::WavPack::File*>(&file) })
            {
                audioProperties.container = core::media::Container::WavPack;
                audioProperties.codec = core::media::Codec::WavPack;
                audioProperties.bitsPerSample = wavPackFile->audioProperties()->bitsPerSample();
            }
#endif // LMS_TAGLIB_HAS_APE
#if LMS_TAGLIB_HAS_ASF
            else if (const auto* asfFile{ dynamic_cast<const ::TagLib::ASF::File*>(&file) })
            {
                audioProperties.container = core::media::Container::ASF;

                switch (asfFile->audioProperties()->codec())
                {
                case ::TagLib::ASF::Properties::Codec::WMA1:
                    audioProperties.codec = core::media::Codec::WMA1;
                    break;
                case ::TagLib::ASF::Properties::Codec::WMA2:
                    audioProperties.codec = core::media::Codec::WMA2;
                    break;
                case ::TagLib::ASF::Properties::Codec::WMA9Lossless:
                    audioProperties.codec = core::media::Codec::WMA9Lossless;
                    break;
                case ::TagLib::ASF::Properties::Codec::WMA9Pro:
                    audioProperties.codec = core::media::Codec::WMA9Pro;
                    break;
                case ::TagLib::ASF::Properties::Codec::Unknown:
                    LMS_LOG(AUDIO, DEBUG, "Unhandled ASF codec in " << filePath);
                    return std::nullopt;
                }

                audioProperties.bitsPerSample = asfFile->audioProperties()->bitsPerSample();
            }
#endif // LMS_TAGLIB_HAS_ASF
#if LMS_TAGLIB_HAS_VORBIS
            else if (const auto* flacFile{ dynamic_cast<const ::TagLib::FLAC::File*>(&file) })
            {
                audioProperties.container = core::media::Container::FLAC;
                audioProperties.codec = core::media::Codec::FLAC;
                audioProperties.bitsPerSample = flacFile->audioProperties()->bitsPerSample();
            }
            else if (dynamic_cast<const ::TagLib::Ogg::Opus::File*>(&file))
            {
                audioProperties.container = core::media::Container::Ogg;
                audioProperties.codec = core::media::Codec::Opus;
            }
            else if (dynamic_cast<const ::TagLib::Ogg::Vorbis::File*>(&file))
            {
                audioProperties.container = core::media::Container::Ogg;
                audioProperties.codec = core::media::Codec::Vorbis;
            }
#endif // LMS_TAGLIB_HAS_VORBIS
#if LMS_TAGLIB_HAS_MP4
            else if (const auto* mp4File{ dynamic_cast<const ::TagLib::MP4::File*>(&file) })
            {
                audioProperties.container = core::media::Container::MP4;
                switch (mp4File->audioProperties()->codec())
                {
                case ::TagLib::MP4::Properties::Codec::AAC:
                    audioProperties.codec = core::media::Codec::AAC;
                    break;
                case ::TagLib::MP4::Properties::Codec::ALAC:
                    audioProperties.codec = core::media::Codec::ALAC;
                    break;
                case ::TagLib::MP4::Properties::Codec::Unknown:
                    LMS_LOG(AUDIO, DEBUG, "Unhandled MP4 codec in " << filePath);
                    return std::nullopt;
                }

                audioProperties.bitsPerSample = mp4File->audioProperties()->bitsPerSample();
            }
#endif // LMS_TAGLIB_HAS_MP4
#if LMS_TAGLIB_HAS_TRUEAUDIO
            else if (const auto* trueAudioFile{ dynamic_cast<const ::TagLib::TrueAudio::File*>(&file) })
            {
                audioProperties.container = core::media::Container::TrueAudio;
                audioProperties.codec = core::media::Codec::TrueAudio;
                audioProperties.bitsPerSample = trueAudioFile->audioProperties()->bitsPerSample();
            }
#endif // LMS_TAGLIB_HAS_TRUEAUDIO
#if LMS_TAGLIB_HAS_DSF
            else if (const auto* dsfFile{ dynamic_cast<const ::TagLib::DSF::File*>(&file) })
            {
                audioProperties.container = core::media::Container::DSF;
                audioProperties.codec = core::media::Codec::DSD;
                audioProperties.bitsPerSample = dsfFile->audioProperties()->bitsPerSample();
            }
#endif // LMS_TAGLIB_HAS_DSF
#if LMS_TAGLIB_HAS_SHORTEN
            else if (const auto* shortenFile{ dynamic_cast<const ::TagLib::Shorten::File*>(&file) })
            {
                audioProperties.container = core::media::Container::Shorten;
                audioProperties.codec = core::media::Codec::Shorten;
                audioProperties.bitsPerSample = shortenFile->audioProperties()->bitsPerSample();
            }
#endif // LMS_TAGLIB_HAS_SHORTEN
            else
            {
                LMS_LOG(AUDIO, DEBUG, "Unhandled file type in " << filePath);
                return std::nullopt;
            }

            if (audioProperties.bitsPerSample && *audioProperties.bitsPerSample == 0)
                audioProperties.bitsPerSample.reset();

            return audioProperties;
        }
    } // namespace

    AudioFileInfo::AudioFileInfo(const std::filesystem::path& filePath, const AudioFileInfoParseOptions& parseOptions)
        : _filePath{ filePath }
        , _fileDesc{ utils::parseFile(filePath, parseOptions.audioPropertiesReadStyle) }
        , _audioProperties{ computeAudioProperties(*_fileDesc.file, filePath) }
    {
        if (parseOptions.readTags)
            _tagReader = std::make_unique<TagReader>(*_fileDesc.file, parseOptions.enableExtraDebugLogs);

        if (parseOptions.readImages)
            _imageReader = std::make_unique<ImageReader>(*_fileDesc.file);
    }

    AudioFileInfo::~AudioFileInfo() = default;

    const AudioProperties* AudioFileInfo::getAudioProperties() const
    {
        return _audioProperties.has_value() ? &_audioProperties.value() : nullptr;
    }

    const IImageReader* AudioFileInfo::getImageReader() const
    {
        return _imageReader.get();
    }

    const ITagReader* AudioFileInfo::getTagReader() const
    {
        return _tagReader.get();
    }
} // namespace lms::audio::taglib
