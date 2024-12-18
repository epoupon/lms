/*
 * Copyright (C) 2020 Emeric Poupon
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

#pragma once

#include "av/IAudioFile.hpp"

struct AVFormatContext;

namespace lms::av
{
    class AudioFile final : public IAudioFile
    {
    public:
        AudioFile(const std::filesystem::path& p);
        ~AudioFile() override;
        AudioFile(const AudioFile&) = delete;
        AudioFile& operator=(const AudioFile&) = delete;

        const std::filesystem::path& getPath() const override;
        ContainerInfo getContainerInfo() const override;
        MetadataMap getMetaData() const override;
        std::vector<StreamInfo> getStreamInfo() const override;
        std::optional<StreamInfo> getBestStreamInfo() const override;
        std::optional<std::size_t> getBestStreamIndex() const override;
        bool hasAttachedPictures() const override;
        void visitAttachedPictures(std::function<void(const Picture&, const MetadataMap&)> func) const override;

    private:
        std::optional<StreamInfo> getStreamInfo(std::size_t streamIndex) const;

        const std::filesystem::path _p;
        AVFormatContext* _context{};
    };
} // namespace lms::av
