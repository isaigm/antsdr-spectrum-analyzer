# AntSDR E200 Spectrum Analyzer

![AntSDR Spectrum Analyzer Demo](assets/demo.gif)

A small real-time spectrum analyzer for the **AntSDR E200 (AD9361)**, written in **C++17** with libiio 1.x, FFTW3 and SFML 2.x. It shows a live spectrum, a waterfall, Max Hold and an experimental CA-CFAR detector, with a sidebar for receiver status and controls.

> **Status:** Development project, tested with an AntSDR E200 over Ethernet (see the demo above). Displayed power is **uncalibrated** and is **not dBm or dBFS**. The CA-CFAR detector finds narrow peaks but often misses wide signals such as FM broadcast stations (see [Known limitations](#known-limitations)).

## Features

- Receive and tune IQ samples over an IIO URI (default `ip:192.168.1.10`)
- 1024-point Hann-windowed FFT; average power across 64 non-overlapping windows per 65,536-sample block
- FFT-shifted spectrum (negative to positive frequency), waterfall and persistent Max Hold
- CA-CFAR threshold/detection overlays and merged signal markers
- Detected signals printed to the console about once per second
- Keyboard tuning with configurable frequency step (10 kHz to 100 MHz)

## Build (Linux)

Requirements: a C++17 compiler, CMake, pkg-config, **libiio 1.x**, **FFTW3** and **SFML 2.5/2.6** (not SFML 3). Install the development packages for your distribution, plus DejaVu or Liberation fonts for UI labels. Ensure `pkg-config --modversion libiio` and `pkg-config --modversion fftw3` return versions.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/antsdr_spectrum ip:192.168.1.10
```

To use another IIO URI, pass it as the first argument.

## Tests

`tests/test_cfar.cpp` is a hardware-free CFAR smoke test. It checks results with `assert`, which is disabled when `NDEBUG` is defined (as in Release builds), so build and run it in Debug:

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --target cfar_tests
ctest --test-dir build-debug --output-on-failure
```

## Controls

| Key | Function |
| --- | --- |
| Left / Right | Tune RX center frequency down / up |
| Up / Down | Increase / decrease tuning step |
| M | Toggle Max Hold visibility |
| R | Reset Max Hold |
| T | Toggle CFAR threshold line |

## Default settings

All of these are set in `include/Config.hpp`.

| Setting | Value |
| --- | --- |
| Center frequency | 93.3 MHz |
| Sample rate (span) | 2.4 MSPS |
| RF bandwidth | 2.0 MHz |
| RX gain | 30 dB, manual gain mode |
| Tuning range | 70 MHz to 6 GHz (requests outside are clamped) |
| Initial tuning step | 100 kHz |

## Project layout

```text
include/Config.hpp              Hardware/UI constants
include/Types.hpp               IQ samples, spectrum, detection types
include/SdrReceiver.hpp         libiio hardware interface
include/SpectrumProcessor.hpp   Hann, FFTW, power averaging, Max Hold
include/CfarDetector.hpp        CA-CFAR and detection grouping
include/SpectrumRenderer.hpp    SFML spectrum, waterfall and sidebar
include/Application.hpp         Event loop / orchestration
src/                            Implementation files and main.cpp
tests/test_cfar.cpp             Hardware-free CFAR smoke test
```

## Hardware assumptions

- `ad9361-phy` exposes the receiver controls and `cf-ad9361-lpc` exposes the streaming samples.
- `voltage0` and `voltage1` are enabled as I and Q, respectively.
- The stream is assumed to contain **interleaved signed 16-bit little-endian I/Q**, exactly 4 bytes per complex sample. The code rejects unexpected sample strides but **does not yet check the channel data-format metadata**. Verify the format of your firmware/device with `iio_info` before relying on numeric amplitude values.
- Frequency settings and gain must be supported by the device. A failed libiio operation is treated as an error.
- A desktop Linux GUI is required, and the SDR must be reachable over IP.

## Technical notes

- Each FFT is 1024 samples; 65,536 / 1024 = 64 spectra are averaged per receive block.
- Bin spacing is `FS / FFT_SIZE = 2343.75 Hz` at the default 2.4 MSPS rate.
- One averaged spectrum and one waterfall row are produced per block, at most about 36.6 per second (2.4 MSPS / 65,536). The waterfall's vertical axis is **frames**, not a calibrated time axis.
- The DC bin and the 3 bins on each side are excluded from detection to avoid the LO leakage spike.
- Detected bins separated by up to 3 bins are merged into one signal, and at most 32 signals are reported.
- CA-CFAR's `ALPHA = 2` is an illustrative threshold multiplier, **not** a calibrated false alarm probability.
- Detection widths are CFAR cluster widths, not calibrated occupied bandwidths. The reported frequency is the strongest bin in each cluster.
- Tuning discards four blocks and resets Max Hold. The waterfall is **not** cleared, so rows from the previous frequency scroll out over the next 300 frames.
- The sidebar labels "SPAN 2.4 MHz | RF BW 2.0 MHz", "2.4 MSPS" and "30 dB" are fixed strings in `src/SpectrumRenderer.cpp`. If you change the sample rate, RF bandwidth or gain in `include/Config.hpp`, update them as well. CFAR settings and plot ranges are in `include/Config.hpp`; the palette is in `src/SpectrumRenderer.cpp`.

## Known limitations

- **Wide signals and CA-CFAR.** The CFAR window has 2 guard cells and 8 reference cells per side (about 23 kHz). An FM broadcast station is about 200 kHz wide (around 85 bins), so inside the station the reference cells contain the station itself and the threshold rises with it. For a flat signal, a bin can never exceed `ALPHA = 2` times its reference mean: at the edge, the mean is (S + N) / 2 and the threshold is S + N, which is greater than S. In practice, FM stations are missed or only marked where a local peak sticks out, and the reported frequency moves with the audio content.
- IQ reads block the event loop, so keyboard input is handled once per block.

## Next steps

- Estimate the noise floor from the whole spectrum (for example, the median bin) instead of local CA-CFAR reference cells, so wide signals are detected.
- Report the center of each detection cluster instead of its strongest bin.
- Replace `assert` in the CFAR test with explicit checks so it also fails in Release builds.
- Clear the waterfall when retuning.
- Generate the sidebar labels from `include/Config.hpp`.
- Sweep 88–108 MHz automatically and list the detected stations.
- Read IIO channel format descriptors and support non-default sample packing.
- Calibrate the spectrum in dBFS or dBFS/Hz and allow vertical scale adjustment.
- Benchmark and tune CFAR with recorded IQ data, and add more unit tests.
- Decouple blocking IQ reads from rendering to improve responsiveness.

## License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE) for the full text.
