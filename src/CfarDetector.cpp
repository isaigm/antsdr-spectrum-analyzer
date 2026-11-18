#include "CfarDetector.hpp"
#include <algorithm>
#include <cmath>

DetectionResult CfarDetector::detect(const Spectrum& power) const {
    DetectionResult result;
    for (int j = 0; j < static_cast<int>(config::FFT_SIZE); ++j) {
        double sum = 0.0;
        int count = 0;
        for (int i = 0; i < config::CFAR_REFERENCE_CELLS; ++i) {
            const int left = j - config::CFAR_GUARD_CELLS - config::CFAR_REFERENCE_CELLS + i;
            const int right = j + config::CFAR_GUARD_CELLS + 1 + i;
            if (left >= 0) { sum += power[fftIndex(left)]; ++count; }
            if (right < static_cast<int>(config::FFT_SIZE)) {
                sum += power[fftIndex(right)];
                ++count;
            }
        }
        const double threshold = config::CFAR_ALPHA * (count ? sum / count : 0.0);
        result.threshold[j] = threshold;
        const bool near_dc = std::abs(j - static_cast<int>(config::FFT_SIZE) / 2)
                             <= config::DC_SKIP_BINS;
        result.detected[j] = !near_dc && power[fftIndex(j)] > threshold;
        if (result.detected[j]) ++result.detected_bins;
    }

    for (int p = 0; p < static_cast<int>(config::FFT_SIZE) &&
                    result.signals.size() < config::MAX_DETECTIONS;) {
        if (!result.detected[p]) { ++p; continue; }
        int start = p;
        int end = p;
        int gap = 0;
        for (++p; p < static_cast<int>(config::FFT_SIZE); ++p) {
            if (result.detected[p]) { end = p; gap = 0; }
            else if (++gap > config::DETECTION_MERGE_GAP) break;
        }
        int peak = start;
        for (int q = start + 1; q <= end; ++q) {
            if (power[fftIndex(q)] > power[fftIndex(peak)]) peak = q;
        }
        result.signals.push_back({peak, end - start + 1});
    }
    return result;
}
