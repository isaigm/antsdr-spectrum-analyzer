#pragma once

#include "Types.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <cstdint>
#include <vector>

enum class UserAction {
    TuneDown, TuneUp, StepDown, StepUp,
    ToggleMaxHold, ResetMaxHold, ToggleThreshold
};

// SFML-only component: handles keyboard events and draws spectrum/waterfall/sidebar.
class SpectrumRenderer {
public:
    SpectrumRenderer();
    bool isOpen() const noexcept { return window_.isOpen(); }
    std::vector<UserAction> pollActions();
    void clearWaterfall();
    void draw(const Spectrum& power, const Spectrum& hold,
              const DetectionResult& detections, std::int64_t center_hz,
              const DisplayState& state);

private:
    sf::RenderWindow window_;
    sf::Font font_;
    bool font_present_ = false;
    sf::VertexBuffer spectrum_buffer_{sf::LineStrip};
    sf::VertexBuffer hold_buffer_{sf::LineStrip};
    sf::VertexBuffer threshold_buffer_{sf::LineStrip};
    sf::Texture waterfall_texture_;
    sf::Sprite waterfall_sprite_;
    std::vector<sf::Uint8> waterfall_pixels_;
    std::array<sf::Vertex, config::FFT_SIZE> spectrum_vertices_{};
    std::array<sf::Vertex, config::FFT_SIZE> hold_vertices_{};
    std::array<sf::Vertex, config::FFT_SIZE> threshold_vertices_{};
    sf::VertexArray markers_{sf::Lines};

    void drawWaterfall(const Spectrum& power);
    void drawSpectrum(const Spectrum& power, const Spectrum& hold,
                      const DetectionResult& detections, const DisplayState& state);
    void drawLabels(const DetectionResult& detections, std::int64_t center_hz,
                    std::int64_t hop_hz);
    void drawSidebar(std::int64_t center_hz, const DisplayState& state);
};
