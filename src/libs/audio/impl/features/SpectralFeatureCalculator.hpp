#pragma once

#include <array>
#include <cmath>
#include <limits>
#include <span>

namespace lms::audio::features
{
    template<typename FloatType = float>
    struct SpectralFrameFeatures
    {
        FloatType spectralCentroid{};
        FloatType spectralRolloff{};
        FloatType spectralFlux{};
    };

    template<std::size_t N, typename FloatType = float>
    class SpectralFeatureCalculator
    {
    public:
        using Input = std::span<const FloatType, N>;
        using Output = SpectralFrameFeatures<FloatType>;

        [[nodiscard]] Output apply(Input powerSpectrum, FloatType binWidthHz)
        {
            constexpr FloatType silenceThreshold{ 1e-12 };
            Output result{};

            FloatType totalEnergy{};

            // centroid
            {
                FloatType weightedSum{};

                for (std::size_t k{}; k < N; ++k)
                {
                    const FloatType p{ powerSpectrum[k] };
                    weightedSum += (static_cast<FloatType>(k) * binWidthHz) * p;
                    totalEnergy += p;
                }

                result.spectralCentroid = (totalEnergy > silenceThreshold) ? (weightedSum / totalEnergy) : FloatType{};
            }

            // rolloff
            if (totalEnergy > silenceThreshold)
            {
                const FloatType cutoffEnergy{ totalEnergy * FloatType{ 0.85 } };
                FloatType cumulative{};

                for (std::size_t k{}; k < N; ++k)
                {
                    cumulative += powerSpectrum[k];
                    if (cumulative >= cutoffEnergy)
                    {
                        result.spectralRolloff = static_cast<FloatType>(k) * binWidthHz;
                        break;
                    }
                }
            }

            // spectral flux
            if (totalEnergy > silenceThreshold)
            {
                std::array<FloatType, N> normalized{};
                const FloatType invEnergy{ FloatType{ 1 } / totalEnergy };
                for (std::size_t k{}; k < N; ++k)
                    normalized[k] = powerSpectrum[k] * invEnergy;

                if (_hasPrev)
                {
                    FloatType flux{};

                    for (std::size_t k{}; k < N; ++k)
                    {
                        const FloatType diff{ normalized[k] - _prev[k] };
                        flux += diff * diff; // symmetric
                    }

                    result.spectralFlux = flux;
                }

                _prev = normalized;
                _hasPrev = true;
            }
            else
                _hasPrev = false; // reset history: avoids fake flux spikes

            return result;
        }

        void reset() noexcept
        {
            _hasPrev = false;
        }

    private:
        std::array<FloatType, N> _prev{};
        bool _hasPrev{};
    };
} // namespace lms::audio::features