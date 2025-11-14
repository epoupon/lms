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

#include "ImageReader.hpp"

#include "TagLibDefs.hpp"

#include <taglib/aifffile.h>
#include <taglib/apetag.h>
#include <taglib/asffile.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4file.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/tfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>

#include "core/String.hpp"

namespace lms::audio::taglib
{
    namespace
    {
        Image::Type imageTypeFromfromID3v2(TagLib::ID3v2::AttachedPictureFrame::Type type)
        {
            switch (type)
            {
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::Other:
                return Image::Type::Other;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::FileIcon:
                return Image::Type::FileIcon;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::OtherFileIcon:
                return Image::Type::OtherFileIcon;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::FrontCover:
                return Image::Type::FrontCover;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::BackCover:
                return Image::Type::BackCover;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::LeafletPage:
                return Image::Type::LeafletPage;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::Media:
                return Image::Type::Media;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::LeadArtist:
                return Image::Type::LeadArtist;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::Artist:
                return Image::Type::Artist;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::Conductor:
                return Image::Type::Conductor;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::Band:
                return Image::Type::Band;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::Composer:
                return Image::Type::Composer;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::Lyricist:
                return Image::Type::Lyricist;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::RecordingLocation:
                return Image::Type::RecordingLocation;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::DuringRecording:
                return Image::Type::DuringRecording;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::DuringPerformance:
                return Image::Type::DuringPerformance;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::MovieScreenCapture:
                return Image::Type::MovieScreenCapture;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::ColouredFish:
                return Image::Type::ColouredFish;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::Illustration:
                return Image::Type::Illustration;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::BandLogo:
                return Image::Type::BandLogo;
            case ::TagLib::ID3v2::AttachedPictureFrame::Type::PublisherLogo:
                return Image::Type::PublisherLogo;
            }

            return Image::Type::Unknown;
        }

        Image::Type imageTypeFromfromASF(TagLib::ASF::Picture::Type type)
        {
            switch (type)
            {
            case ::TagLib::ASF::Picture::Type::Other:
                return Image::Type::Other;
            case ::TagLib::ASF::Picture::Type::FileIcon:
                return Image::Type::FileIcon;
            case ::TagLib::ASF::Picture::Type::OtherFileIcon:
                return Image::Type::OtherFileIcon;
            case ::TagLib::ASF::Picture::Type::FrontCover:
                return Image::Type::FrontCover;
            case ::TagLib::ASF::Picture::Type::BackCover:
                return Image::Type::BackCover;
            case ::TagLib::ASF::Picture::Type::LeafletPage:
                return Image::Type::LeafletPage;
            case ::TagLib::ASF::Picture::Type::Media:
                return Image::Type::Media;
            case ::TagLib::ASF::Picture::Type::LeadArtist:
                return Image::Type::LeadArtist;
            case ::TagLib::ASF::Picture::Type::Artist:
                return Image::Type::Artist;
            case ::TagLib::ASF::Picture::Type::Conductor:
                return Image::Type::Conductor;
            case ::TagLib::ASF::Picture::Type::Band:
                return Image::Type::Band;
            case ::TagLib::ASF::Picture::Type::Composer:
                return Image::Type::Composer;
            case ::TagLib::ASF::Picture::Type::Lyricist:
                return Image::Type::Lyricist;
            case ::TagLib::ASF::Picture::Type::RecordingLocation:
                return Image::Type::RecordingLocation;
            case ::TagLib::ASF::Picture::Type::DuringRecording:
                return Image::Type::DuringRecording;
            case ::TagLib::ASF::Picture::Type::DuringPerformance:
                return Image::Type::DuringPerformance;
            case ::TagLib::ASF::Picture::Type::MovieScreenCapture:
                return Image::Type::MovieScreenCapture;
            case ::TagLib::ASF::Picture::Type::ColouredFish:
                return Image::Type::ColouredFish;
            case ::TagLib::ASF::Picture::Type::Illustration:
                return Image::Type::Illustration;
            case ::TagLib::ASF::Picture::Type::BandLogo:
                return Image::Type::BandLogo;
            case ::TagLib::ASF::Picture::Type::PublisherLogo:
                return Image::Type::PublisherLogo;
            }

            return Image::Type::Unknown;
        }

        Image::Type imageTypeFromfromFLAC(TagLib::FLAC::Picture::Type type)
        {
            switch (type)
            {
            case ::TagLib::FLAC::Picture::Type::Other:
                return Image::Type::Other;
            case ::TagLib::FLAC::Picture::Type::FileIcon:
                return Image::Type::FileIcon;
            case ::TagLib::FLAC::Picture::Type::OtherFileIcon:
                return Image::Type::OtherFileIcon;
            case ::TagLib::FLAC::Picture::Type::FrontCover:
                return Image::Type::FrontCover;
            case ::TagLib::FLAC::Picture::Type::BackCover:
                return Image::Type::BackCover;
            case ::TagLib::FLAC::Picture::Type::LeafletPage:
                return Image::Type::LeafletPage;
            case ::TagLib::FLAC::Picture::Type::Media:
                return Image::Type::Media;
            case ::TagLib::FLAC::Picture::Type::LeadArtist:
                return Image::Type::LeadArtist;
            case ::TagLib::FLAC::Picture::Type::Artist:
                return Image::Type::Artist;
            case ::TagLib::FLAC::Picture::Type::Conductor:
                return Image::Type::Conductor;
            case ::TagLib::FLAC::Picture::Type::Band:
                return Image::Type::Band;
            case ::TagLib::FLAC::Picture::Type::Composer:
                return Image::Type::Composer;
            case ::TagLib::FLAC::Picture::Type::Lyricist:
                return Image::Type::Lyricist;
            case ::TagLib::FLAC::Picture::Type::RecordingLocation:
                return Image::Type::RecordingLocation;
            case ::TagLib::FLAC::Picture::Type::DuringRecording:
                return Image::Type::DuringRecording;
            case ::TagLib::FLAC::Picture::Type::DuringPerformance:
                return Image::Type::DuringPerformance;
            case ::TagLib::FLAC::Picture::Type::MovieScreenCapture:
                return Image::Type::MovieScreenCapture;
            case ::TagLib::FLAC::Picture::Type::ColouredFish:
                return Image::Type::ColouredFish;
            case ::TagLib::FLAC::Picture::Type::Illustration:
                return Image::Type::Illustration;
            case ::TagLib::FLAC::Picture::Type::BandLogo:
                return Image::Type::BandLogo;
            case ::TagLib::FLAC::Picture::Type::PublisherLogo:
                return Image::Type::PublisherLogo;
            }

            return Image::Type::Unknown;
        }

        const char* mp4ImageFormatToMimeType(TagLib::MP4::CoverArt::Format format)
        {
            switch (format)
            {
            case ::TagLib::MP4::CoverArt::Format::BMP:
                return "image/bmp";
            case ::TagLib::MP4::CoverArt::Format::GIF:
                return "image/gif";
            case ::TagLib::MP4::CoverArt::Format::JPEG:
                return "image/jpeg";
            case ::TagLib::MP4::CoverArt::Format::PNG:
                return "image/png";
            case ::TagLib::MP4::CoverArt::Format::Unknown:
                return "application/octet-stream";
            }

            return "application/octet-stream";
        }

#if LMS_TAGLIB_HAS_APE_COMPLEX_PROPERTIES
        Image::Type imageTypeFromAPEPictureType(std::string_view pictureType)
        {
            if (core::stringUtils::stringCaseInsensitiveContains(pictureType, "front"))
                return Image::Type::FrontCover;
            if (core::stringUtils::stringCaseInsensitiveContains(pictureType, "back"))
                return Image::Type::BackCover;

            return Image::Type::Unknown;
        }
#endif // LMS_TAGLIB_HAS_APE_COMPLEX_PROPERTIES

        void visitID3V2Images(const ::TagLib::ID3v2::Tag& id3v2Tags, const ImageReader::ImageVisitor& visitor)
        {
            const auto& frameListMap{ id3v2Tags.frameListMap() };

            for (const ::TagLib::ID3v2::Frame* frame : frameListMap["APIC"])
            {
                const auto* attachedPictureFrame{ dynamic_cast<const ::TagLib::ID3v2::AttachedPictureFrame*>(frame) };
                if (!attachedPictureFrame)
                    continue;

                ::TagLib::ByteVector picture{ attachedPictureFrame->picture() };
                std::span<const std::byte> pictureData{ reinterpret_cast<const std::byte*>(picture.data()), picture.size() };

                Image image;
                image.type = imageTypeFromfromID3v2(attachedPictureFrame->type());
                image.description = attachedPictureFrame->description().to8Bit(true);
                image.mimeType = attachedPictureFrame->mimeType().to8Bit(true);
                image.data = pictureData;
                visitor(image);
            }
        }

        void visitASFImages(const ::TagLib::ASF::Tag& asfTags, const ImageReader::ImageVisitor& visitor)
        {
            for (const ::TagLib::ASF::Attribute& attribute : asfTags.attribute("WM/Picture"))
            {
                ::TagLib::ASF::Picture asfPicture{ attribute.toPicture() };
                if (!asfPicture.isValid())
                    continue;

                ::TagLib::ByteVector picture{ asfPicture.picture() };
                std::span<const std::byte> pictureData{ reinterpret_cast<const std::byte*>(picture.data()), picture.size() };

                Image image;
                image.type = imageTypeFromfromASF(asfPicture.type());
                image.description = asfPicture.description().to8Bit(true);
                image.mimeType = asfPicture.mimeType().to8Bit(true);
                image.data = pictureData;

                visitor(image);
            }
        }

        void visitMP4Images(const ::TagLib::MP4::File& mp4File, const ImageReader::ImageVisitor& visitor)
        {
            const ::TagLib::MP4::Item coverItem{ mp4File.tag()->item("covr") };
            if (!coverItem.isValid())
                return;

#if LMS_TAGLIB_HAS_MP4_ITEM_TYPE
            if (coverItem.type() != ::TagLib::MP4::Item::Type::CoverArtList)
                return;
#endif
            ::TagLib::MP4::CoverArtList coverArtList{ coverItem.toCoverArtList() };

            bool firstCover{ true };
            for (TagLib::MP4::CoverArt& coverArt : coverArtList)
            {
                ::TagLib::ByteVector picture{ coverArt.data() };
                std::span<const std::byte> pictureData{ reinterpret_cast<const std::byte*>(picture.data()), picture.size() };

                Image image;
                image.mimeType = mp4ImageFormatToMimeType(coverArt.format());
                image.data = pictureData;

                // By convention, consider the first cover art as the front cover
                image.type = firstCover ? Image::Type::FrontCover : Image::Type::Unknown;
                firstCover = false;

                visitor(image);
            }
        }

        void visitFLACImages(const ::TagLib::List<TagLib::FLAC::Picture*>& pictureList, const ImageReader::ImageVisitor& visitor)
        {
            for (TagLib::FLAC::Picture* flacPicture : pictureList)
            {
                ::TagLib::ByteVector picture{ flacPicture->data() };
                std::span<const std::byte> pictureData{ reinterpret_cast<const std::byte*>(picture.data()), picture.size() };

                Image image;
                image.type = imageTypeFromfromFLAC(flacPicture->type());
                image.description = flacPicture->description().to8Bit(true);
                image.mimeType = flacPicture->mimeType().to8Bit(true);
                image.data = pictureData;

                visitor(image);
            }
        }

        void visitAPEImages([[maybe_unused]] const ::TagLib::APE::Tag& apeTags, [[maybe_unused]] const ImageReader::ImageVisitor& visitor)
        {
#if LMS_TAGLIB_HAS_APE_COMPLEX_PROPERTIES
            const ::TagLib::List<TagLib::VariantMap> pictureProperties{ apeTags.complexProperties("PICTURE") };
            for (const ::TagLib::VariantMap& pictureProperty : pictureProperties)
            {
                Image image;
                ::TagLib::ByteVector picture;

                if (auto it{ pictureProperty.find("pictureType") }; it != pictureProperty.cend())
                    image.type = imageTypeFromAPEPictureType(it->second.toString().to8Bit(true));
                if (auto it{ pictureProperty.find("mimeType") }; it != pictureProperty.cend())
                    image.mimeType = it->second.toString().to8Bit(true);
                if (auto it{ pictureProperty.find("description") }; it != pictureProperty.cend())
                    image.description = it->second.toString().to8Bit(true);
                if (auto it{ pictureProperty.find("data") }; it != pictureProperty.cend())
                {
                    picture = it->second.toByteVector();
                    image.data = { reinterpret_cast<const std::byte*>(picture.data()), picture.size() };
                }

                if (!image.data.empty())
                    visitor(image);
            }
#endif // LMS_TAGLIB_HAS_APE_COMPLEX_PROPERTIES
        }
    } // namespace

    ImageReader::ImageReader(::TagLib::File& file)
        : _file{ file }
    {
    }

    ImageReader::~ImageReader() = default;

    void ImageReader::visitImages(const ImageVisitor& visitor) const
    {
        // MP3
        if (TagLib::MPEG::File * mp3File{ dynamic_cast<TagLib::MPEG::File*>(&_file) })
        {
            if (mp3File->hasID3v2Tag())
                visitID3V2Images(*mp3File->ID3v2Tag(), visitor);
        }
        // MP4
        else if (const TagLib::MP4::File * mp4File{ dynamic_cast<const TagLib::MP4::File*>(&_file) })
        {
            visitMP4Images(*mp4File, visitor);
        }
        // WMA
        else if (const TagLib::ASF::File * asfFile{ dynamic_cast<const TagLib::ASF::File*>(&_file) })
        {
            if (const ::TagLib::ASF::Tag * tag{ asfFile->tag() })
                visitASFImages(*tag, visitor);
        }
        // FLAC
        else if (TagLib::FLAC::File * flacFile{ dynamic_cast<TagLib::FLAC::File*>(&_file) })
        {
            if (flacFile->hasID3v2Tag()) // usage discouraged
                visitID3V2Images(*flacFile->ID3v2Tag(), visitor);
            else
                visitFLACImages(flacFile->pictureList(), visitor);
        }
        // Ogg vorbis
        else if (const TagLib::Ogg::Vorbis::File * vorbisFile{ dynamic_cast<const TagLib::Ogg::Vorbis::File*>(&_file) })
        {
            visitFLACImages(vorbisFile->tag()->pictureList(), visitor);
        }
        // Ogg Opus
        else if (const TagLib::Ogg::Opus::File * opusFile{ dynamic_cast<TagLib::Ogg::Opus::File*>(&_file) })
        {
            visitFLACImages(opusFile->tag()->pictureList(), visitor);
        }
        // Aiff
        else if (const TagLib::RIFF::AIFF::File * aiffFile{ dynamic_cast<TagLib::RIFF::AIFF::File*>(&_file) })
        {
            if (aiffFile->hasID3v2Tag())
                visitID3V2Images(*aiffFile->tag(), visitor);
        }
        // Wav
        else if (const TagLib::RIFF::WAV::File * wavFile{ dynamic_cast<TagLib::RIFF::WAV::File*>(&_file) })
        {
            if (wavFile->hasID3v2Tag())
                visitID3V2Images(*wavFile->ID3v2Tag(), visitor);
        }
        // MPC
        else if (TagLib::MPC::File * mpcFile{ dynamic_cast<TagLib::MPC::File*>(&_file) })
        {
            if (mpcFile->hasAPETag())
                visitAPEImages(*mpcFile->APETag(), visitor);
        }
        // WavPack
        else if (TagLib::WavPack::File * wavPackFile{ dynamic_cast<TagLib::WavPack::File*>(&_file) })
        {
            if (wavPackFile->hasAPETag())
                visitAPEImages(*wavPackFile->APETag(), visitor);
        }
    }
} // namespace lms::audio::taglib
