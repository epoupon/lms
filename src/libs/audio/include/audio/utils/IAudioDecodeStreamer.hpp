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

#include <filesystem>
#include <functional>
#include <memory>

#include <boost/asio/io_context.hpp>

#include "audio/PcmTypes.hpp"

namespace lms::audio
{
    class IAudioOutputStream;
    class IAudioDecoder;
} // namespace lms::audio

namespace lms::audio::utils
{
    // helper class to decode files to PCM samples, fed into the provided output stream
    class IAudioDecodeStreamer
    {
    public:
        virtual ~IAudioDecodeStreamer() = default;

        using DecodeCompleteCallback = std::function<void(bool aborted)>;

        // DecodeCompleteCallback is fired once the file has been fully decoded (but still buffered in output)
        // You can start a new file only if the previous one is finished (i.e. once the callback is fired)
        virtual void start(const std::filesystem::path& path, std::chrono::microseconds offset, DecodeCompleteCallback cb) = 0;
        virtual void abort() = 0; // will call DecodeCompleteCallback once aborted
        virtual bool isComplete() const = 0;
    };

    struct AudioDecodeStreamerParameters
    {
        audio::IAudioOutputStream& outputStream;
        std::size_t bufferCount;
        std::chrono::milliseconds bufferDuration;
    };
    std::unique_ptr<IAudioDecodeStreamer> createAudioDecodeStreamer(boost::asio::io_context& ioContext, const AudioDecodeStreamerParameters& parameters);
} // namespace lms::audio::utils