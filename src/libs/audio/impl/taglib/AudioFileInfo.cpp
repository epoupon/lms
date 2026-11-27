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
#include <optional>
#include <string>
#include <string_view>
#include <variant>

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
#if LMS_TAGLIB_HAS_MATROSKA
    #include <taglib/matroskafile.h>
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
#if LMS_TAGLIB_HAS_MATROSKA
        // See https://www.matroska.org/technical/codec_specs.html
        std::optional<core::media::Codec> codecFromMatroskaCodecId(std::string_view codecId)
        {
            if (codecId == "A_AAC" || codecId.starts_with("A_AAC/")) // A_AAC/MPEG2/... and A_AAC/MPEG4/... are deprecated forms
                return core::media::Codec::AAC;
            if (codecId == "A_AC3" || codecId.starts_with("A_AC3/"))
                return core::media::Codec::AC3;
            if (codecId == "A_ALAC")
                return core::media::Codec::ALAC;
            if (codecId == "A_EAC3")
                return core::media::Codec::EAC3;
            if (codecId == "A_FLAC")
                return core::media::Codec::FLAC;
            if (codecId == "A_MPEG/L3")
                return core::media::Codec::MP3;
            if (codecId == "A_OPUS")
                return core::media::Codec::Opus;
            if (codecId.starts_with("A_PCM/"))
                return core::media::Codec::PCM;
            if (codecId == "A_TTA1")
                return core::media::Codec::TrueAudio;
            if (codecId == "A_VORBIS")
                return core::media::Codec::Vorbis;
            if (codecId == "A_WAVPACK4")
                return core::media::Codec::WavPack;

            return std::nullopt;
        }
#endif // LMS_TAGLIB_HAS_MATROSKA

        std::variant<AudioProperties, AudioPropertiesError> computeAudioProperties(const ::TagLib::File& file, const std::filesystem::path& filePath)
        {
            assert(file.audioProperties());

            AudioProperties audioProperties;

            {
                const ::TagLib::AudioProperties* properties{ file.audioProperties() };
                if (!properties)
                {
                    LMS_LOG(AUDIO, DEBUG, "Cannot determine audio properties in " << filePath);
                    return AudioPropertiesError::NoAudioStream;
                }

                // Common properties, checked once the format is known to be handled
                if (properties->bitrate() > 0)
                    audioProperties.bitrate = static_cast<unsigned>(properties->bitrate()) * 1000;
                audioProperties.channelCount = static_cast<std::size_t>(properties->channels());
                audioProperties.duration = std::chrono::milliseconds{ properties->lengthInMilliseconds() };
                audioProperties.sampleRate = static_cast<std::size_t>(properties->sampleRate());
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
                    return AudioPropertiesError::UnsupportedFormat;
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
                    return AudioPropertiesError::UnsupportedFormat;
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
                    return AudioPropertiesError::UnsupportedFormat;
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
    #if LMS_TAGLIB_HAS_MP4_EXTENDED_CODECS
                case ::TagLib::MP4::Properties::Codec::AC3:
                    audioProperties.codec = core::media::Codec::AC3;
                    break;
                case ::TagLib::MP4::Properties::Codec::EAC3:
                    audioProperties.codec = core::media::Codec::EAC3;
                    break;
                case ::TagLib::MP4::Properties::Codec::FLAC:
                    audioProperties.codec = core::media::Codec::FLAC;
                    break;
                case ::TagLib::MP4::Properties::Codec::Opus:
                    audioProperties.codec = core::media::Codec::Opus;
                    break;
                case ::TagLib::MP4::Properties::Codec::DTS:
                    LMS_LOG(AUDIO, DEBUG, "Unhandled MP4 codec (DTS) in " << filePath);
                    return AudioPropertiesError::UnsupportedFormat;
    #endif // LMS_TAGLIB_HAS_MP4_EXTENDED_CODECS
                case ::TagLib::MP4::Properties::Codec::Unknown:
                    LMS_LOG(AUDIO, DEBUG, "Unhandled MP4 codec in " << filePath);
                    return AudioPropertiesError::UnsupportedFormat;
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
#if LMS_TAGLIB_HAS_MATROSKA
            else if (const auto* matroskaFile{ dynamic_cast<const ::TagLib::Matroska::File*>(&file) })
            {
                const auto& properties{ *matroskaFile->audioProperties() };

                const std::string codecId{ properties.codecName().to8Bit(true) };
                const std::optional<core::media::Codec> codec{ codecFromMatroskaCodecId(codecId) };
                if (!codec)
                {
                    LMS_LOG(AUDIO, DEBUG, "Unhandled Matroska codec '" << codecId << "' in " << filePath);
                    return AudioPropertiesError::UnsupportedFormat;
                }

                // WebM only allows Opus and Vorbis: any other codec is labelled Matroska so that WebM-only clients never get it
                const bool isWebM{ properties.docType() == "webm" && (*codec == core::media::Codec::Opus || *codec == core::media::Codec::Vorbis) };
                audioProperties.container = isWebM ? core::media::Container::WebM : core::media::Container::Matroska;
                audioProperties.codec = *codec;
                // TagLib's bitrate is the whole file's (video and attachments included), and the per-track BPS
                // statistics tags cannot be matched to the audio track since TagLib does not expose track UIDs
                audioProperties.bitrate.reset();
                audioProperties.bitsPerSample = properties.bitsPerSample();
            }
#endif // LMS_TAGLIB_HAS_MATROSKA
            else
            {
                LMS_LOG(AUDIO, DEBUG, "Unhandled file type in " << filePath);
                return AudioPropertiesError::UnsupportedFormat;
            }

            if (audioProperties.channelCount == 0)
            {
                LMS_LOG(AUDIO, DEBUG, "Cannot determine channel count in " << filePath);
                return AudioPropertiesError::NoAudioStream;
            }

            if (audioProperties.sampleRate == 0)
            {
                LMS_LOG(AUDIO, DEBUG, "Cannot determine sample rate in " << filePath);
                return AudioPropertiesError::NoAudioStream;
            }

            if (audioProperties.duration <= std::chrono::milliseconds::zero())
            {
                LMS_LOG(AUDIO, DEBUG, "Cannot determine duration in " << filePath);
                return AudioPropertiesError::UnknownDuration;
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
        return std::get_if<AudioProperties>(&_audioProperties);
    }

    std::optional<AudioPropertiesError> AudioFileInfo::getAudioPropertiesError() const
    {
        if (const AudioPropertiesError * error{ std::get_if<AudioPropertiesError>(&_audioProperties) })
            return *error;

        return std::nullopt;
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
