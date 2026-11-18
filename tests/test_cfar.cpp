#include "CfarDetector.hpp"
#include <cassert>
#include <iostream>

int main() {
    CfarDetector detector;

    Spectrum noise{};
    noise.fill(100.0);
    const auto no_signal = detector.detect(noise);
    assert(no_signal.detected_bins == 0);
    assert(no_signal.signals.empty());

    // Inject a narrow peak away from the suppressed DC bins.
    const int expected_bin = 200;
    noise[fftIndex(expected_bin)] = 1000.0;
    const auto found = detector.detect(noise);
    assert(found.detected[expected_bin]);
    assert(!found.signals.empty());
    assert(found.signals.front().peak_bin == expected_bin);

    // DC and immediate neighboring bins are deliberately ignored.
    noise.fill(100.0);
    noise[fftIndex(config::FFT_SIZE / 2)] = 1000.0;
    const auto dc = detector.detect(noise);
    assert(!dc.detected[config::FFT_SIZE / 2]);
    std::cout << "CFAR tests passed.\n";
}
