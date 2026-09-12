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

#include "TestUtils.hpp"

#include <array>
#include <fstream>
#include <random>

#include "audio/IAudioDecoder.hpp"
#include "audio/PcmTypes.hpp"

namespace lms::audio::tests
{
    ScopedTmpDirectory::ScopedTmpDirectory()
        : _path{ std::filesystem::temp_directory_path() / ("lms-audio-test-" + std::to_string(std::random_device{}())) }
    {
        std::filesystem::create_directories(_path);
    }

    ScopedTmpDirectory::~ScopedTmpDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(_path, ec);
    }

    std::filesystem::path ScopedTmpDirectory::operator/(std::string_view fileName) const
    {
        return _path / fileName;
    }

    void writeFile(const std::filesystem::path& path, std::span<const std::byte> data)
    {
        std::ofstream os{ path, std::ios::binary };
        os.write(static_cast<const char*>(static_cast<const void*>(data.data())), static_cast<std::streamsize>(data.size()));
    }

    std::vector<std::int16_t> decodeInterleaved(const std::filesystem::path& path, unsigned channelCount, unsigned sampleRate)
    {
        const PcmParameters pcmParameters{
            .channelCount = channelCount,
            .sampleRate = sampleRate,
            .sampleType = PcmSampleType::Signed16,
            .byteOrder = std::endian::native,
            .planar = false,
        };

        const std::unique_ptr<IAudioDecoder> decoder{ createAudioDecoder(path, std::chrono::microseconds{ 0 }, pcmParameters) };

        constexpr std::size_t decodeSampleCount{ 8'192 };
        std::vector<std::byte> buffer(decodeSampleCount * channelCount * sizeof(std::int16_t));
        std::array<IAudioDecoder::WritableBuffer, 1> buffers{ std::span{ buffer } };

        std::vector<std::int16_t> res;
        while (!decoder->finished())
        {
            const std::size_t sampleCount{ decoder->readSamples(buffers) };
            if (sampleCount == 0)
                break;

            const auto* samples{ static_cast<const std::int16_t*>(static_cast<const void*>(buffer.data())) };
            res.insert(std::end(res), samples, samples + sampleCount * channelCount);
        }

        return res;
    }
} // namespace lms::audio::tests
