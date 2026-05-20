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

#pragma once

#include "ScanStepBase.hpp"

namespace lms::audio
{
    class IMusicNNEmbeddingExtractor;
}

namespace lms::scanner
{
    class ScanStepExtractMusicNNEmbeddings : public ScanStepBase
    {
    public:
        ScanStepExtractMusicNNEmbeddings(InitParams& initParams, const std::filesystem::path& modelPath, std::size_t maxPatchCountPerTrack);
        ~ScanStepExtractMusicNNEmbeddings() override;
        ScanStepExtractMusicNNEmbeddings(const ScanStepExtractMusicNNEmbeddings&) = delete;
        ScanStepExtractMusicNNEmbeddings& operator=(const ScanStepExtractMusicNNEmbeddings&) = delete;

    private:
        ScanStep getStep() const override { return ScanStep::ExtractMusicNNEmbeddings; }
        core::LiteralString getStepName() const override { return "Extract MusicNN embeddings"; }
        bool needProcess(const ScanContext& context) const override;
        void process(ScanContext& context) override;

        std::unique_ptr<audio::IMusicNNEmbeddingExtractor> _embeddingExtractor;
    };
} // namespace lms::scanner
