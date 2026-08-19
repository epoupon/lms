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

#include "Types.hpp"

namespace lms::db::detail
{
    std::optional<core::media::Container> getMediaContainerType(db::detail::Container container)
    {
        switch (container)
        {
        case Container::AIFF:
            return core::media::Container::AIFF;
        case Container::APE:
            return core::media::Container::APE;
        case Container::ASF:
            return core::media::Container::ASF;
        case Container::DSF:
            return core::media::Container::DSF;
        case Container::FLAC:
            return core::media::Container::FLAC;
        case Container::MP4:
            return core::media::Container::MP4;
        case Container::MPC:
            return core::media::Container::MPC;
        case Container::MPEG:
            return core::media::Container::MPEG;
        case Container::Ogg:
            return core::media::Container::Ogg;
        case Container::Shorten:
            return core::media::Container::Shorten;
        case Container::TrueAudio:
            return core::media::Container::TrueAudio;
        case Container::WAV:
            return core::media::Container::WAV;
        case Container::WavPack:
            return core::media::Container::WavPack;

        case Container::Unknown:
            break;
        }

        return std::nullopt;
    }

    db::detail::Container getDbContainer(core::media::Container container)
    {
        switch (container)
        {
        case core::media::Container::AIFF:
            return Container::AIFF;
        case core::media::Container::APE:
            return Container::APE;
        case core::media::Container::ASF:
            return Container::ASF;
        case core::media::Container::DSF:
            return Container::DSF;
        case core::media::Container::FLAC:
            return Container::FLAC;
        case core::media::Container::MP4:
            return Container::MP4;
        case core::media::Container::MPC:
            return Container::MPC;
        case core::media::Container::MPEG:
            return Container::MPEG;
        case core::media::Container::Ogg:
            return Container::Ogg;
        case core::media::Container::Shorten:
            return Container::Shorten;
        case core::media::Container::TrueAudio:
            return Container::TrueAudio;
        case core::media::Container::WAV:
            return Container::WAV;
        case core::media::Container::WavPack:
            return Container::WavPack;
        }

        return Container::Unknown;
    }

    std::optional<core::media::Codec> getMediaCodecType(db::detail::Codec codec)
    {
        switch (codec)
        {
        case Codec::AAC:
            return core::media::Codec::AAC;
        case Codec::AC3:
            return core::media::Codec::AC3;
        case Codec::ALAC:
            return core::media::Codec::ALAC;
        case Codec::APE:
            return core::media::Codec::APE;
        case Codec::DSD:
            return core::media::Codec::DSD;
        case Codec::EAC3:
            return core::media::Codec::EAC3;
        case Codec::FLAC:
            return core::media::Codec::FLAC;
        case Codec::MP3:
            return core::media::Codec::MP3;
        case Codec::MP4ALS:
            return core::media::Codec::MP4ALS;
        case Codec::MPC7:
            return core::media::Codec::MPC7;
        case Codec::MPC8:
            return core::media::Codec::MPC8;
        case Codec::Opus:
            return core::media::Codec::Opus;
        case Codec::PCM:
            return core::media::Codec::PCM;
        case Codec::Shorten:
            return core::media::Codec::Shorten;
        case Codec::TrueAudio:
            return core::media::Codec::TrueAudio;
        case Codec::Vorbis:
            return core::media::Codec::Vorbis;
        case Codec::WavPack:
            return core::media::Codec::WavPack;
        case Codec::WMA1:
            return core::media::Codec::WMA1;
        case Codec::WMA2:
            return core::media::Codec::WMA2;
        case Codec::WMA9Pro:
            return core::media::Codec::WMA9Pro;
        case Codec::WMA9Lossless:
            return core::media::Codec::WMA9Lossless;

        case Codec::Unknown:
            break;
        }

        return std::nullopt;
    }

    db::detail::Codec getDbCodec(core::media::Codec codec)
    {
        switch (codec)
        {
        case core::media::Codec::AAC:
            return Codec::AAC;
        case core::media::Codec::AC3:
            return Codec::AC3;
        case core::media::Codec::ALAC:
            return Codec::ALAC;
        case core::media::Codec::APE:
            return Codec::APE;
        case core::media::Codec::DSD:
            return Codec::DSD;
        case core::media::Codec::EAC3:
            return Codec::EAC3;
        case core::media::Codec::FLAC:
            return Codec::FLAC;
        case core::media::Codec::MP3:
            return Codec::MP3;
        case core::media::Codec::MP4ALS:
            return Codec::MP4ALS;
        case core::media::Codec::MPC7:
            return Codec::MPC7;
        case core::media::Codec::MPC8:
            return Codec::MPC8;
        case core::media::Codec::Opus:
            return Codec::Opus;
        case core::media::Codec::PCM:
            return Codec::PCM;
        case core::media::Codec::Shorten:
            return Codec::Shorten;
        case core::media::Codec::TrueAudio:
            return Codec::TrueAudio;
        case core::media::Codec::Vorbis:
            return Codec::Vorbis;
        case core::media::Codec::WavPack:
            return Codec::WavPack;
        case core::media::Codec::WMA1:
            return Codec::WMA1;
        case core::media::Codec::WMA2:
            return Codec::WMA2;
        case core::media::Codec::WMA9Pro:
            return Codec::WMA9Pro;
        case core::media::Codec::WMA9Lossless:
            return Codec::WMA9Lossless;
        }

        return Codec::Unknown;
    }

    core::media::ImageType getMediaImageType(db::detail::ImageType type)
    {
        switch (type)
        {
        case db::detail::ImageType::Unknown:
            return core::media::ImageType::Unknown;
        case db::detail::ImageType::Other:
            return core::media::ImageType::Other;
        case db::detail::ImageType::FileIcon:
            return core::media::ImageType::FileIcon;
        case db::detail::ImageType::OtherFileIcon:
            return core::media::ImageType::OtherFileIcon;
        case db::detail::ImageType::FrontCover:
            return core::media::ImageType::FrontCover;
        case db::detail::ImageType::BackCover:
            return core::media::ImageType::BackCover;
        case db::detail::ImageType::LeafletPage:
            return core::media::ImageType::LeafletPage;
        case db::detail::ImageType::Media:
            return core::media::ImageType::Media;
        case db::detail::ImageType::LeadArtist:
            return core::media::ImageType::LeadArtist;
        case db::detail::ImageType::Artist:
            return core::media::ImageType::Artist;
        case db::detail::ImageType::Conductor:
            return core::media::ImageType::Conductor;
        case db::detail::ImageType::Band:
            return core::media::ImageType::Band;
        case db::detail::ImageType::Composer:
            return core::media::ImageType::Composer;
        case db::detail::ImageType::Lyricist:
            return core::media::ImageType::Lyricist;
        case db::detail::ImageType::RecordingLocation:
            return core::media::ImageType::RecordingLocation;
        case db::detail::ImageType::DuringRecording:
            return core::media::ImageType::DuringRecording;
        case db::detail::ImageType::DuringPerformance:
            return core::media::ImageType::DuringPerformance;
        case db::detail::ImageType::MovieScreenCapture:
            return core::media::ImageType::MovieScreenCapture;
        case db::detail::ImageType::ColouredFish:
            return core::media::ImageType::ColouredFish;
        case db::detail::ImageType::Illustration:
            return core::media::ImageType::Illustration;
        case db::detail::ImageType::BandLogo:
            return core::media::ImageType::BandLogo;
        case db::detail::ImageType::PublisherLogo:
            return core::media::ImageType::PublisherLogo;
        }

        return core::media::ImageType::Unknown;
    }

    db::detail::ImageType getDbImageType(core::media::ImageType type)
    {
        switch (type)
        {
        case core::media::ImageType::Unknown:
            return db::detail::ImageType::Unknown;
        case core::media::ImageType::Other:
            return db::detail::ImageType::Other;
        case core::media::ImageType::FileIcon:
            return db::detail::ImageType::FileIcon;
        case core::media::ImageType::OtherFileIcon:
            return db::detail::ImageType::OtherFileIcon;
        case core::media::ImageType::FrontCover:
            return db::detail::ImageType::FrontCover;
        case core::media::ImageType::BackCover:
            return db::detail::ImageType::BackCover;
        case core::media::ImageType::LeafletPage:
            return db::detail::ImageType::LeafletPage;
        case core::media::ImageType::Media:
            return db::detail::ImageType::Media;
        case core::media::ImageType::LeadArtist:
            return db::detail::ImageType::LeadArtist;
        case core::media::ImageType::Artist:
            return db::detail::ImageType::Artist;
        case core::media::ImageType::Conductor:
            return db::detail::ImageType::Conductor;
        case core::media::ImageType::Band:
            return db::detail::ImageType::Band;
        case core::media::ImageType::Composer:
            return db::detail::ImageType::Composer;
        case core::media::ImageType::Lyricist:
            return db::detail::ImageType::Lyricist;
        case core::media::ImageType::RecordingLocation:
            return db::detail::ImageType::RecordingLocation;
        case core::media::ImageType::DuringRecording:
            return db::detail::ImageType::DuringRecording;
        case core::media::ImageType::DuringPerformance:
            return db::detail::ImageType::DuringPerformance;
        case core::media::ImageType::MovieScreenCapture:
            return db::detail::ImageType::MovieScreenCapture;
        case core::media::ImageType::ColouredFish:
            return db::detail::ImageType::ColouredFish;
        case core::media::ImageType::Illustration:
            return db::detail::ImageType::Illustration;
        case core::media::ImageType::BandLogo:
            return db::detail::ImageType::BandLogo;
        case core::media::ImageType::PublisherLogo:
            return db::detail::ImageType::PublisherLogo;
        }

        return db::detail::ImageType::Unknown;
    }

    std::optional<core::media::ImageFormat> getMediaImageFormat(db::detail::ImageFormat format)
    {
        switch (format)
        {
        case ImageFormat::BMP:
            return core::media::ImageFormat::BMP;
        case ImageFormat::GIF:
            return core::media::ImageFormat::GIF;
        case ImageFormat::JPEG:
            return core::media::ImageFormat::JPEG;
        case ImageFormat::PNG:
            return core::media::ImageFormat::PNG;
        case ImageFormat::SVG:
            return core::media::ImageFormat::SVG;
        case ImageFormat::WebP:
            return core::media::ImageFormat::WebP;

        case ImageFormat::Unknown:
            break;
        }

        return std::nullopt;
    }

    db::detail::ImageFormat getDbImageFormat(core::media::ImageFormat format)
    {
        switch (format)
        {
        case core::media::ImageFormat::BMP:
            return ImageFormat::BMP;
        case core::media::ImageFormat::GIF:
            return ImageFormat::GIF;
        case core::media::ImageFormat::JPEG:
            return ImageFormat::JPEG;
        case core::media::ImageFormat::PNG:
            return ImageFormat::PNG;
        case core::media::ImageFormat::SVG:
            return ImageFormat::SVG;
        case core::media::ImageFormat::WebP:
            return ImageFormat::WebP;
        }

        return ImageFormat::Unknown;
    }
} // namespace lms::db::detail