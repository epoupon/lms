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

#include "MusicNNModel.hpp"

#include <string>

#include <onnxruntime_cxx_api.h>

#include "audio/Exception.hpp"

namespace lms::audio::musicnn
{
    namespace
    {
        // Shape of the ONNX model's single input tensor: [batch=1, T=187, mel=96]
        const std::array<std::int64_t, 3> inputShape{ 1,
                                                      static_cast<std::int64_t>(MusicNNModel::inputFrames),
                                                      static_cast<std::int64_t>(MusicNNModel::inputBands) };

        // Shape of the ONNX model's single output tensor: [batch=1, embedding=200]
        const std::array<std::int64_t, 2> outputShape{ 1,
                                                       static_cast<std::int64_t>(MusicNNModel::outputSize) };

        constexpr const char* inputName{ "mel_patch" };
        constexpr const char* outputName{ "embedding" };
    } // namespace

    struct MusicNNModel::Impl
    {
        Ort::Env env;
        Ort::SessionOptions sessionOptions;
        Ort::Session session;
        Ort::MemoryInfo memoryInfo;

        explicit Impl(const std::filesystem::path& onnxPath)
            : env{ ORT_LOGGING_LEVEL_ERROR, "MusicNN" }
            , session{ [&]() -> Ort::Session {
                sessionOptions.SetIntraOpNumThreads(1);
                sessionOptions.SetInterOpNumThreads(1);
                return Ort::Session{ env, onnxPath.c_str(), sessionOptions };
            }() }
            , memoryInfo{ Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault) }
        {
        }
    };

    MusicNNModel::MusicNNModel(const std::filesystem::path& onnxPath)
    {
        try
        {
            _impl = std::make_unique<Impl>(onnxPath);
        }
        catch (const Ort::Exception& e)
        {
            throw audio::Exception{ std::string{ "Failed to load ONNX model '" } + onnxPath.string() + "': " + e.what() };
        }
    }

    MusicNNModel::~MusicNNModel() = default;

    std::array<float, MusicNNModel::outputSize> MusicNNModel::forward(
        std::span<const float, inputFrames * inputBands> melPatch) const
    {
        Ort::Value inputTensor{ Ort::Value::CreateTensor<float>(
            _impl->memoryInfo,
            const_cast<float*>(melPatch.data()), // safe cast
            melPatch.size(),
            inputShape.data(),
            inputShape.size()) };

        std::array<float, outputSize> result{};
        Ort::Value outputTensor{ Ort::Value::CreateTensor<float>(
            _impl->memoryInfo,
            result.data(),
            result.size(),
            outputShape.data(),
            outputShape.size()) };

        try
        {
            auto inputNames{ std::to_array({ inputName }) };
            auto outputNames{ std::to_array({ outputName }) };
            _impl->session.Run(Ort::RunOptions{ nullptr },
                               inputNames.data(), &inputTensor, 1,
                               outputNames.data(), &outputTensor, 1);
        }
        catch (const Ort::Exception& e)
        {
            throw audio::Exception{ std::string{ "ONNX inference failed: " } + e.what() };
        }

        return result;
    }
} // namespace lms::audio::musicnn
