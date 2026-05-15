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

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>

namespace lms::audio::musicnn
{
    // The ONNX model must be exported with:
    //   input  "mel_patch"  shape [1, 1, 187, 96]
    //   output "embedding"  shape [1, 200]
    //
    // Export script: tools/musicnn/export_onnx.py
    class MusicNNModel
    {
    public:
        explicit MusicNNModel(const std::filesystem::path& onnxPath);
        ~MusicNNModel();

        MusicNNModel(const MusicNNModel&) = delete;
        MusicNNModel& operator=(const MusicNNModel&) = delete;

        static inline constexpr std::size_t inputFrames{ 187 };
        static inline constexpr std::size_t inputBands{ 96 };
        static inline constexpr std::size_t outputSize{ 200 };

        [[nodiscard]] std::array<float, outputSize> forward(std::span<const float, inputFrames * inputBands> melPatch) const;

    private:
        // Pimpl: keep ORT headers out of translation units that include this header.
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
} // namespace lms::audio::musicnn
