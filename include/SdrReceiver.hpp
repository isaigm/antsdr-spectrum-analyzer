#pragma once

#include "Types.hpp"
#include <cstdint>
#include <string>
#include <vector>

struct iio_context;
struct iio_device;
struct iio_channel;
struct iio_channels_mask;
struct iio_stream;

// Owns libiio objects, hardware configuration, tuning and IQ acquisition.
// Currently expects 16-bit little-endian interleaved I/Q words (4 bytes/sample).
class SdrReceiver {
public:
    explicit SdrReceiver(const std::string& uri);
    ~SdrReceiver();

    SdrReceiver(const SdrReceiver&) = delete;
    SdrReceiver& operator=(const SdrReceiver&) = delete;

    std::int64_t centerFrequencyHz() const noexcept { return center_frequency_hz_; }
    void tune(std::int64_t frequency_hz);
    void discardBlocks(int count);
    void readBlock(std::vector<IQSample>& out);

private:
    iio_context* context_ = nullptr;
    iio_device* phy_ = nullptr;
    iio_device* rx_device_ = nullptr;
    iio_channel* i_channel_ = nullptr;
    iio_channel* q_channel_ = nullptr;
    iio_channels_mask* channel_mask_ = nullptr;
    iio_stream* stream_ = nullptr;
    int sample_stride_ = 0;
    std::int64_t center_frequency_hz_ = 0;

    void configure();
    void cleanup() noexcept;
    void writeLong(bool output, const char* channel, const char* name,
                   std::int64_t value);
    void writeDouble(bool output, const char* channel, const char* name,
                     double value);
    void writeString(bool output, const char* channel, const char* name,
                     const char* value);
    std::int64_t readLong(bool output, const char* channel, const char* name);
};
