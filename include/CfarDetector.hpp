#pragma once

#include "Types.hpp"

// CA-CFAR in linear power, followed by merging nearby detected bins.
class CfarDetector {
public:
    DetectionResult detect(const Spectrum& power) const;
};
