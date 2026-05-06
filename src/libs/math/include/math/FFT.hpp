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

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <span>

namespace lms::math
{
    template<std::size_t Size, typename FloatType = float>
    class FixedRealFFTPlan
    {
        static_assert(std::has_single_bit(Size), "Size must be power of two");

    public:
        static constexpr std::size_t minBufferAlignment{ 32 };

        constexpr FixedRealFFTPlan()
        {
            // Twiddles
            for (std::size_t k{}; k < halfSize; ++k)
            {
                FloatType angle{ FloatType(-2) * std::numbers::pi_v<FloatType> * k / Size };
                _twiddles[k] = std::complex<FloatType>{ std::cos(angle), std::sin(angle) };
            }

            // Bit-reversal for halfSize FFT
            constexpr std::size_t logHalf{ std::countr_zero(halfSize) };
            for (std::size_t i{}; i < halfSize; ++i)
                _bitrev[i] = reverseBits(i, logHalf);
        }

        constexpr static std::size_t getInputSize() noexcept { return Size; }
        constexpr static std::size_t getOutputSize() noexcept { return halfSize + 1; }

        constexpr void apply(std::span<const FloatType> input, std::span<std::complex<FloatType>> output) const noexcept
        {
            assert(input.size() == Size);
            assert(output.size() == halfSize + 1);

            assert(reinterpret_cast<std::uintptr_t>(input.data()) % minBufferAlignment == 0);
            assert(reinterpret_cast<std::uintptr_t>(output.data()) % minBufferAlignment == 0);

            // Pack real -> complex
            alignas(minBufferAlignment) std::array<std::complex<FloatType>, halfSize> data;
            for (std::size_t i{}; i < halfSize; ++i)
                data[i] = std::complex<FloatType>{ input[2 * i], input[2 * i + 1] };

            fft(data);

            // Real FFT post-process
            output[0] = std::complex<FloatType>{ data[0].real() + data[0].imag(), FloatType{} };
            output[halfSize] = std::complex<FloatType>{ data[0].real() - data[0].imag(), FloatType{} };
            for (std::size_t k{ 1 }; k <= halfSize / 2; ++k)
            {
                const auto a{ data[k] };
                const auto b{ std::conj(data[(halfSize - k) & (halfSize - 1)]) };

                const auto even{ (a + b) * std::complex<FloatType>{ FloatType(0.5), FloatType{} } };
                const auto odd{ (a - b) * std::complex<FloatType>{ FloatType{}, FloatType(-0.5) } };

                const auto& W{ _twiddles[k] };
                const auto t{ W * odd };

                output[k] = even + t;
                output[halfSize - k] = std::conj(even - t);
            }
        }

    private:
        static constexpr std::size_t halfSize{ Size / 2 };

        alignas(minBufferAlignment) std::array<std::complex<FloatType>, halfSize> _twiddles{};
        std::array<std::size_t, halfSize> _bitrev{};

        static constexpr std::size_t reverseBits(std::size_t x, std::size_t bitCount) noexcept
        {
            std::size_t y{};
            for (std::size_t i{}; i < bitCount; ++i)
            {
                y = (y << 1) | (x & 1);
                x >>= 1;
            }
            return y;
        }

        constexpr void fft(std::array<std::complex<FloatType>, halfSize>& data) const noexcept
        {
            // Bit reversal
            for (std::size_t i{}; i < halfSize; ++i)
            {
                const auto j{ _bitrev[i] };
                if (i < j)
                    std::swap(data[i], data[j]);
            }

            fftStages<1>(data);
        }

        template<std::size_t Stage>
        constexpr void fftStages(std::array<std::complex<FloatType>, halfSize>& data) const noexcept
        {
            constexpr std::size_t len{ 1U << Stage };

            if constexpr (len <= halfSize)
            {
                constexpr std::size_t half{ len >> 1 };
                constexpr std::size_t step{ Size / len };

                for (std::size_t i{}; i < halfSize; i += len)
                {
                    for (std::size_t j{}; j < half; ++j)
                    {
                        auto& u{ data[i + j] };
                        auto& v{ data[i + j + half] };

                        const auto t{ _twiddles[j * step] * v };

                        v = u - t;
                        u = u + t;
                    }
                }

                fftStages<Stage + 1>(data);
            }
        }
    };
} // namespace lms::math