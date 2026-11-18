#pragma once

#include "Config.hpp"
#include <array>
#include <cstdint>
#include <vector>

struct IQSample {
    std::int16_t i = 0;
    std::int16_t q = 0;
};

using Spectrum = std::array<double, config::FFT_SIZE>;

struct SignalDetection {
    int peak_bin = 0;       // Display bin, in FFT-shifted order
    int width_bins = 0;     // Width of the CFAR detection cluster, not occupied bandwidth
};

struct DetectionResult {
    Spectrum threshold{};  // FFT-shifted display order
    std::array<bool, config::FFT_SIZE> detected{};
    std::vector<SignalDetection> signals;
    int detected_bins = 0;
};

struct DisplayState {
    bool show_max_hold = true;
    bool show_threshold = false;
    std::size_t hop_index = config::INITIAL_HOP_INDEX;
    std::int64_t hopHz() const { return config::HOPS_HZ[hop_index]; }
};

// FFTW output is DC, positive frequencies, then negative frequencies.
// This maps a display position (negative to positive frequencies) to FFT index.
inline std::size_t fftIndex(std::size_t display_bin) {
    return (display_bin + config::FFT_SIZE / 2) % config::FFT_SIZE;
}
