#include "SdrReceiver.hpp"
#include "Config.hpp"
#include <iio/iio.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
std::runtime_error iioError(int err, const std::string& action) {
    char message[128] = {};
    iio_strerror(err, message, sizeof(message));
    return std::runtime_error(action + ": " + message + " (" + std::to_string(err) + ")");
}

const iio_attr* findAttribute(iio_device* device, bool output,
                              const char* channel_name, const char* attr_name) {
    iio_channel* channel = iio_device_find_channel(device, channel_name, output);
    if (!channel)
        throw std::runtime_error(std::string("Missing IIO channel: ") + channel_name);
    const iio_attr* attr = iio_channel_find_attr(channel, attr_name);
    if (!attr)
        throw std::runtime_error(std::string("Missing IIO attribute: ") + attr_name);
    return attr;
}
} // namespace

SdrReceiver::SdrReceiver(const std::string& uri) {
    try {
        context_ = iio_create_context(nullptr, uri.c_str());
        const int err = iio_err(context_);
        if (err) {
            context_ = nullptr;
            throw iioError(err, "Connect to " + uri);
        }
        configure();
    } catch (...) {
        cleanup();
        throw;
    }
}

SdrReceiver::~SdrReceiver() { cleanup(); }

void SdrReceiver::cleanup() noexcept {
    if (stream_) { iio_stream_destroy(stream_); stream_ = nullptr; }
    if (channel_mask_) { iio_channels_mask_destroy(channel_mask_); channel_mask_ = nullptr; }
    if (context_) { iio_context_destroy(context_); context_ = nullptr; }
}

void SdrReceiver::writeLong(bool output, const char* channel,
                            const char* name, std::int64_t value) {
    const int err = iio_attr_write_longlong(
        findAttribute(phy_, output, channel, name), static_cast<long long>(value));
    if (err < 0) throw iioError(err, std::string("Write ") + name);
}

void SdrReceiver::writeDouble(bool output, const char* channel,
                              const char* name, double value) {
    const int err = iio_attr_write_double(findAttribute(phy_, output, channel, name), value);
    if (err < 0) throw iioError(err, std::string("Write ") + name);
}

void SdrReceiver::writeString(bool output, const char* channel,
                              const char* name, const char* value) {
    const int err = iio_attr_write_string(findAttribute(phy_, output, channel, name), value);
    if (err < 0) throw iioError(err, std::string("Write ") + name);
}

std::int64_t SdrReceiver::readLong(bool output, const char* channel,
                                   const char* name) {
    long long value = 0;
    const int err = iio_attr_read_longlong(findAttribute(phy_, output, channel, name), &value);
    if (err < 0) throw iioError(err, std::string("Read ") + name);
    return value;
}

void SdrReceiver::configure() {
    phy_ = iio_context_find_device(context_, "ad9361-phy");
    rx_device_ = iio_context_find_device(context_, "cf-ad9361-lpc");
    if (!phy_ || !rx_device_)
        throw std::runtime_error("Missing ad9361-phy or cf-ad9361-lpc IIO device");

    writeLong(false, "voltage0", "sampling_frequency", config::SAMPLE_RATE_HZ);
    writeLong(false, "voltage0", "rf_bandwidth", config::RF_BANDWIDTH_HZ);
    writeString(false, "voltage0", "gain_control_mode", config::GAIN_MODE);
    writeDouble(false, "voltage0", "hardwaregain", config::RX_GAIN_DB);
    tune(config::INITIAL_FREQUENCY_HZ);

    i_channel_ = iio_device_find_channel(rx_device_, "voltage0", false);
    q_channel_ = iio_device_find_channel(rx_device_, "voltage1", false);
    if (!i_channel_ || !q_channel_)
        throw std::runtime_error("Missing RX voltage0/voltage1 IQ channel");

    channel_mask_ = iio_create_channels_mask(iio_device_get_channels_count(rx_device_));
    if (!channel_mask_)
        throw std::runtime_error("Unable to create IIO channels mask");
    iio_channel_enable(i_channel_, channel_mask_);
    iio_channel_enable(q_channel_, channel_mask_);

    sample_stride_ = iio_device_get_sample_size(rx_device_, channel_mask_);
    if (sample_stride_ != 4)
        throw std::runtime_error("Unsupported IQ packing: expected interleaved I/Q int16 (4 bytes), stride="
                                 + std::to_string(sample_stride_));

    iio_buffer* buffer = iio_device_get_buffer(rx_device_, 0);
    if (!buffer)
        throw std::runtime_error("Unable to obtain cf-ad9361-lpc buffer");
    stream_ = iio_buffer_create_stream(buffer, config::STREAM_BLOCKS,
                                       config::BLOCK_SAMPLES, channel_mask_);
    const int err = iio_err(stream_);
    if (err) {
        stream_ = nullptr;
        throw iioError(err, "Create IIO receive stream");
    }
}

void SdrReceiver::tune(std::int64_t frequency_hz) {
    const auto clamped = std::clamp(frequency_hz, config::MIN_LO_HZ, config::MAX_LO_HZ);
    writeLong(true, "altvoltage0", "frequency", clamped);
    center_frequency_hz_ = readLong(true, "altvoltage0", "frequency");
}

void SdrReceiver::discardBlocks(int count) {
    for (int i = 0; i < count; ++i) {
        const iio_block* block = iio_stream_get_next_block(stream_);
        const int err = iio_err(block);
        if (err) throw iioError(err, "Discard IIO block");
    }
}

void SdrReceiver::readBlock(std::vector<IQSample>& out) {
    const iio_block* block = iio_stream_get_next_block(stream_);
    const int err = iio_err(block);
    if (err) throw iioError(err, "Read IIO block");

    const auto* first = static_cast<const std::uint8_t*>(iio_block_first(block, i_channel_));
    const auto* end = static_cast<const std::uint8_t*>(iio_block_end(block));
    if (!first || !end || end < first || (end - first) % sample_stride_ != 0)
        throw std::runtime_error("Invalid IIO block span");

    const std::size_t count = static_cast<std::size_t>((end - first) / sample_stride_);
    if (count == 0) throw std::runtime_error("Empty IQ block");
    out.resize(count);
    for (std::size_t n = 0; n < count; ++n) {
        const auto* cursor = first + n * sample_stride_;
        // No unaligned loads or strict-aliasing violations. Assumes little-endian host.
        std::memcpy(&out[n].i, cursor, sizeof(std::int16_t));
        std::memcpy(&out[n].q, cursor + sizeof(std::int16_t), sizeof(std::int16_t));
    }
}
