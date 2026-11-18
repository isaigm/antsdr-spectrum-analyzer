#pragma once

#include "Types.hpp"
#include <array>
#include <vector>
#include <fftw3.h>

// Windowed FFT and averaged power spectrum, plus persistent Max Hold.
class SpectrumProcessor {
public:
    SpectrumProcessor();
    ~SpectrumProcessor();
    SpectrumProcessor(const SpectrumProcessor&) = delete;
    SpectrumProcessor& operator=(const SpectrumProcessor&) = delete;

    void process(const std::vector<IQSample>& iq);
    void resetMaxHold();
    const Spectrum& power() const noexcept { return power_; }
    const Spectrum& maxHold() const noexcept { return max_hold_; }

private:
    std::array<double, config::FFT_SIZE> hann_{};
    std::array<fftw_complex, config::FFT_SIZE> fft_input_{};
    std::array<fftw_complex, config::FFT_SIZE> fft_output_{};
    fftw_plan plan_ = nullptr;
    Spectrum power_{};
    Spectrum max_hold_{};
};
