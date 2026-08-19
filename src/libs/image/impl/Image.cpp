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

#include "image/Image.hpp"

#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "core/ITraceLogger.hpp"
#include "core/media/ImageFormat.hpp"

#include "Backend.hpp"
#include "FormatDesc.hpp"
#include "FormatSignature.hpp"
#include "image/Exception.hpp"

namespace lms::image
{
    namespace
    {
        std::vector<std::byte> readHeader(const std::filesystem::path& path)
        {
            std::ifstream ifs{ path, std::ios::binary };
            if (!ifs)
            {
                const std::error_code ec{ errno, std::generic_category() };
                throw IOFileException{ path, "Cannot open file", ec };
            }

            std::vector<std::byte> header(maxSignatureHeaderSize);
            ifs.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
            if (ifs.bad()) // a short read here is expected (file smaller than the header size) but a real I/O error is not
            {
                const std::error_code ec{ errno, std::generic_category() };
                throw IOFileException{ path, "Cannot read file content", ec };
            }
            header.resize(static_cast<std::size_t>(ifs.gcount()));

            return header;
        }
    } // namespace

    void init(const std::filesystem::path& path)
    {
        backend::init(path);
    }

    std::span<const std::filesystem::path> getSupportedFileExtensions()
    {
        static const std::vector<std::filesystem::path> extensions{ [] {
            std::vector<std::filesystem::path> exts;
            visitFormatDescs([&](const FormatDesc& policy) {
                exts.insert(std::cend(exts), std::cbegin(policy.fileExtensions), std::cend(policy.fileExtensions));
            });
            return exts;
        }() };

        return extensions;
    }

    ImageProperties probeImage(const std::filesystem::path& path)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "ProbeFile");

        const std::vector<std::byte> header{ readHeader(path) };
        const auto format{ identifyFormat(header) };
        if (!format)
            throw Exception{ "Unrecognized image format for file '" + path.string() + "' (used " + std::to_string(header.size()) + " bytes)" };

        if (!canFormatBeDecoded(*format))
            return ImageProperties{ *format, std::nullopt };

        return ImageProperties{ *format, backend::probeImage(path) };
    }

    ImageProperties probeImage(std::span<const std::byte> encodedData)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "ProbeBuffer");

        const auto format{ identifyFormat(encodedData) };
        if (!format)
            throw Exception{ "Unrecognized image format (used " + std::to_string(encodedData.size()) + " bytes)" };

        if (!canFormatBeDecoded(*format))
            return ImageProperties{ *format, std::nullopt };

        return ImageProperties{ *format, backend::probeImage(encodedData) };
    }

    std::unique_ptr<IRawImage> decodeImage(std::span<const std::byte> encodedData)
    {
        return backend::decodeImage(encodedData);
    }

    std::unique_ptr<IRawImage> decodeImage(const std::filesystem::path& path)
    {
        return backend::decodeImage(path);
    }

    std::unique_ptr<IEncodedImage> encodeToJPEG(const IRawImage& rawImage, unsigned quality)
    {
        return backend::encodeToJPEG(rawImage, quality);
    }

    bool canFormatBeDecoded(core::media::ImageFormat format)
    {
        const FormatDesc* desc{ findFormatDesc(format) };
        return desc && desc->canDecode;
    }
} // namespace lms::image
