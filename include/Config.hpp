#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace config {
inline constexpr std::size_t FFT_SIZE = 1024;
inline constexpr std::size_t BLOCK_SAMPLES = 65536;
inline constexpr unsigned STREAM_BLOCKS = 4;
inline constexpr int DISCARD_BLOCKS_AFTER_TUNE = 4;

inline constexpr std::int64_t INITIAL_FREQUENCY_HZ = 93300000;
inline constexpr std::int64_t SAMPLE_RATE_HZ = 2400000;
inline constexpr std::int64_t RF_BANDWIDTH_HZ = 2000000;
inline constexpr std::int64_t MIN_LO_HZ = 70000000;
inline constexpr std::int64_t MAX_LO_HZ = 6000000000;
inline constexpr double RX_GAIN_DB = 30.0;
inline constexpr const char* GAIN_MODE = "manual";
inline constexpr const char* DEFAULT_URI = "ip:192.168.1.10";

inline constexpr std::array<std::int64_t, 8> HOPS_HZ = {
    10000, 25000, 100000, 250000, 1000000, 2000000, 10000000, 100000000
};
inline constexpr std::size_t INITIAL_HOP_INDEX = 2;

inline constexpr unsigned SPECTRUM_HEIGHT = 400;
inline constexpr unsigned WATERFALL_HEIGHT = 300;
inline constexpr unsigned PLOT_WIDTH = static_cast<unsigned>(FFT_SIZE);
inline constexpr unsigned SIDEBAR_WIDTH = 320;
inline constexpr unsigned WINDOW_WIDTH = PLOT_WIDTH + SIDEBAR_WIDTH;
inline constexpr unsigned WINDOW_HEIGHT = SPECTRUM_HEIGHT + WATERFALL_HEIGHT;

// Display units are uncalibrated power (not dBm/dBFS).
inline constexpr double SPECTRUM_DB_MIN = 20.0;
inline constexpr double SPECTRUM_DB_MAX = 100.0;
inline constexpr double WATERFALL_DB_MIN = 30.0;
inline constexpr double WATERFALL_DB_MAX = 70.0;

inline constexpr int CFAR_GUARD_CELLS = 2;
inline constexpr int CFAR_REFERENCE_CELLS = 8;
inline constexpr double CFAR_ALPHA = 2.0;
inline constexpr int DC_SKIP_BINS = 3;
inline constexpr int DETECTION_MERGE_GAP = 3;
inline constexpr std::size_t MAX_DETECTIONS = 32;
inline constexpr int LOG_EVERY_FRAMES = 37;
}
