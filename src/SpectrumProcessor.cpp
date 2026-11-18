#include "SpectrumProcessor.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

SpectrumProcessor::SpectrumProcessor() {
    constexpr double PI = 3.14159265358979323846;
    for (std::size_t n = 0; n < config::FFT_SIZE; ++n) {
        hann_[n] = 0.5 - 0.5 * std::cos(2.0 * PI * n / (config::FFT_SIZE - 1));
    }
    plan_ = fftw_plan_dft_1d(static_cast<int>(config::FFT_SIZE),
                             fft_input_.data(), fft_output_.data(),
                             FFTW_FORWARD, FFTW_ESTIMATE);
    if (!plan_) throw std::runtime_error("Unable to create FFTW plan");
}

SpectrumProcessor::~SpectrumProcessor() {
    if (plan_) fftw_destroy_plan(plan_);
}

void SpectrumProcessor::resetMaxHold() { max_hold_.fill(0.0); }

void SpectrumProcessor::process(const std::vector<IQSample>& iq) {
    const std::size_t windows = iq.size() / config::FFT_SIZE;
    if (windows == 0)
        throw std::runtime_error("Not enough IQ samples for one FFT");
    power_.fill(0.0);

    for (std::size_t frame = 0; frame < windows; ++frame) {
        for (std::size_t n = 0; n < config::FFT_SIZE; ++n) {
            const IQSample& value = iq[frame * config::FFT_SIZE + n];
            fft_input_[n][0] = value.i * hann_[n];
            fft_input_[n][1] = value.q * hann_[n];
        }
        fftw_execute(plan_);
        for (std::size_t k = 0; k < config::FFT_SIZE; ++k) {
            const double re = fft_output_[k][0];
            const double im = fft_output_[k][1];
            power_[k] += (re * re + im * im) / static_cast<double>(windows);
        }
    }
    for (std::size_t k = 0; k < config::FFT_SIZE; ++k)
        max_hold_[k] = std::max(max_hold_[k], power_[k]);
}
