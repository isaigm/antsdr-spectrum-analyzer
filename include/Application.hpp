#pragma once

#include "SdrReceiver.hpp"
#include "SpectrumProcessor.hpp"
#include "CfarDetector.hpp"
#include "SpectrumRenderer.hpp"
#include <vector>

// Coordinates acquisition, DSP, detection and presentation.
class Application {
public:
    explicit Application(const std::string& uri);
    void run();

private:
    SdrReceiver m_receiver;
    SpectrumProcessor m_processor;
    CfarDetector m_detector;
    SpectrumRenderer m_renderer;
    DisplayState m_display;
    std::vector<IQSample> iq;
    bool first_frame = true;
    long detected_bins_sum = 0;
    int frames_since_log = 0;

    void handle(UserAction action);
    void printStatistics(const DetectionResult& results);
};
