#include "Application.hpp"
#include "Config.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

Application::Application(const std::string& uri) : m_receiver(uri) {
    iq.reserve(config::BLOCK_SAMPLES);
}

void Application::handle(UserAction action) {
    switch (action) {
        case UserAction::TuneDown:
        case UserAction::TuneUp: {
            const auto direction = action == UserAction::TuneUp ? 1LL : -1LL;
            m_receiver.tune(m_receiver.centerFrequencyHz() + direction * m_display.hopHz());
            m_receiver.discardBlocks(config::DISCARD_BLOCKS_AFTER_TUNE);
            m_processor.resetMaxHold();
            break;
        }
        case UserAction::StepDown:
            if (m_display.hop_index > 0) --m_display.hop_index;
            break;
        case UserAction::StepUp:
            if (m_display.hop_index + 1 < config::HOPS_HZ.size()) ++m_display.hop_index;
            break;
        case UserAction::ToggleMaxHold:
            m_display.show_max_hold = !m_display.show_max_hold;
            break;
        case UserAction::ResetMaxHold:
            m_processor.resetMaxHold();
            break;
        case UserAction::ToggleThreshold:
            m_display.show_threshold = !m_display.show_threshold;
            break;
    }
}

void Application::printStatistics(const DetectionResult& results) {
    detected_bins_sum += results.detected_bins;
    if (++frames_since_log < config::LOG_EVERY_FRAMES) return;

    std::printf("CFAR: %ld detected bins over %d frames |", detected_bins_sum,
                frames_since_log);
    for (const auto& signal : results.signals) {
        const double hz = m_receiver.centerFrequencyHz() +
                          (signal.peak_bin - static_cast<int>(config::FFT_SIZE) / 2) *
                          static_cast<double>(config::SAMPLE_RATE_HZ) / config::FFT_SIZE;
        const double width_hz = signal.width_bins *
                                static_cast<double>(config::SAMPLE_RATE_HZ) / config::FFT_SIZE;
        std::printf(" %.3f MHz (%.0f kHz)", hz / 1e6, width_hz / 1e3);
    }
    std::printf("\n");
    frames_since_log = 0;
    detected_bins_sum = 0;
}

void Application::run() {
    while (m_renderer.isOpen()) {
        for (const auto action : m_renderer.pollActions()) handle(action);
        if (!m_renderer.isOpen()) break;

        m_receiver.readBlock(iq);
        m_processor.process(iq);
        const DetectionResult detections = m_detector.detect(m_processor.power());

        if (first_frame) {
            double min_db = 1e300, max_db = -1e300;
            for (double p : m_processor.power()) {
                const double db = 10.0 * std::log10(p + 1e-12);
                min_db = std::min(min_db, db);
                max_db = std::max(max_db, db);
            }
            std::printf("First block: min %.1f dB, max %.1f dB (uncalibrated)\n",
                        min_db, max_db);
            first_frame = false;
        }
        printStatistics(detections);
        m_renderer.draw(m_processor.power(), m_processor.maxHold(), detections,
                       m_receiver.centerFrequencyHz(), m_display);
    }
}
