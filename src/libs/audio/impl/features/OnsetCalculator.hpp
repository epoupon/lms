#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace lms::audio::features
{
    template<std::size_t BandCount, typename FloatType = float>
    class OnsetCalculator
    {
    public:
        using Input = std::span<const FloatType, BandCount>;

        [[nodiscard]] FloatType apply(Input melEnergies)
        {
            FloatType frameEnergy{};
            for (std::size_t i{}; i < BandCount; ++i)
                frameEnergy += melEnergies[i];

            constexpr FloatType silenceThreshold{ 1e-12F };
            if (frameEnergy <= silenceThreshold)
            {
                _hasPrev = false;
                return {};
            }

            FloatType novelty{};
            if (_hasPrev)
            {
                for (std::size_t i{}; i < BandCount; ++i)
                {
                    const FloatType diff{ melEnergies[i] - _prev[i] };
                    if (diff > FloatType{})
                        novelty += diff;
                }
            }

            std::copy(melEnergies.begin(), melEnergies.end(), _prev.begin());
            _hasPrev = true;

            return novelty;
        }

        void reset() noexcept
        {
            _hasPrev = false;
        }

    private:
        std::array<FloatType, BandCount> _prev{};
        bool _hasPrev{};
    };
} // namespace lms::audio::features
