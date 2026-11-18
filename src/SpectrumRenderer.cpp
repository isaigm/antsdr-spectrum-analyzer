#include "SpectrumRenderer.hpp"
#include "Config.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
// Graphite, warm ivory, muted ochre: same palette as the single-file version.
namespace ui {
const sf::Color BG(27, 28, 26);
const sf::Color PANEL(35, 36, 33);
const sf::Color CARD(46, 47, 43);
const sf::Color BORDER(71, 72, 66);
const sf::Color TEXT(230, 227, 217);
const sf::Color MUTED(166, 165, 153);
const sf::Color ACCENT(196, 156, 94);
const sf::Color SUCCESS(142, 164, 125);
}

float powerToY(double power) {
    const double db = 10.0 * std::log10(power + 1e-12);
    const double y = config::SPECTRUM_HEIGHT -
        (db - config::SPECTRUM_DB_MIN) /
        (config::SPECTRUM_DB_MAX - config::SPECTRUM_DB_MIN) * config::SPECTRUM_HEIGHT;
    return static_cast<float>(std::clamp(y, 0.0, double(config::SPECTRUM_HEIGHT - 1)));
}

sf::Color heat(double level) {
    const double s = std::clamp(level, 0.0, 1.0) * 4.0;
    const int seg = static_cast<int>(s);
    const double frac = s - seg;
    const auto up = static_cast<sf::Uint8>(255 * frac);
    const auto down = static_cast<sf::Uint8>(255 * (1.0 - frac));
    switch (seg) {
        case 0: return {0, 0, up};
        case 1: return {0, up, 255};
        case 2: return {up, 255, down};
        case 3: return {255, down, 0};
        default: return {255, 0, 0};
    }
}

bool loadFont(sf::Font& font) {
    const char* paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
    };
    for (const char* path : paths)
        if (font.loadFromFile(path)) return true;
    return false;
}

void rect(sf::RenderTarget& target, float x, float y, float w, float h,
          sf::Color fill, sf::Color outline = sf::Color::Transparent) {
    sf::RectangleShape r({w, h});
    r.setPosition(x, y);
    r.setFillColor(fill);
    if (outline != sf::Color::Transparent) {
        r.setOutlineColor(outline);
        r.setOutlineThickness(1.f);
    }
    target.draw(r);
}

void text(sf::RenderTarget& target, const sf::Font& font, const std::string& message,
          unsigned size, float x, float y, sf::Color color = ui::TEXT, bool bold = false) {
    sf::Text label(message, font, size);
    label.setPosition(x, y);
    label.setFillColor(color);
    if (bold) label.setStyle(sf::Text::Bold);
    target.draw(label);
}

void centeredText(sf::RenderTarget& target, const sf::Font& font,
                  const std::string& message, unsigned size, float center, float y,
                  sf::Color color) {
    sf::Text label(message, font, size);
    label.setFillColor(color);
    const auto bounds = label.getLocalBounds();
    label.setPosition(center - bounds.width / 2.f - bounds.left, y);
    target.draw(label);
}

std::string formatHop(std::int64_t hop) {
    char s[40];
    if (hop >= 1000000)
        std::snprintf(s, sizeof(s), "%lld MHz", static_cast<long long>(hop / 1000000));
    else
        std::snprintf(s, sizeof(s), "%lld kHz", static_cast<long long>(hop / 1000));
    return s;
}

void controlRow(sf::RenderTarget& target, const sf::Font& font, float base_x,
                float y, const char* keys, const char* action, const char* detail) {
    const float x = base_x + 18.f;
    rect(target, x, y, config::SIDEBAR_WIDTH - 36.f, 46.f, ui::CARD);
    rect(target, x + 10.f, y + 9.f, 91.f, 28.f, ui::BG, ui::BORDER);
    centeredText(target, font, keys, 12, x + 10.f + 91.f / 2.f, y + 12.f, ui::ACCENT);
    text(target, font, action, 13, x + 112.f, y + 6.f, ui::TEXT, true);
    text(target, font, detail, 11, x + 112.f, y + 24.f, ui::MUTED);
}
} // namespace

SpectrumRenderer::SpectrumRenderer()
    : window_(sf::VideoMode(config::WINDOW_WIDTH, config::WINDOW_HEIGHT),
              "AntSDR E200  |  Spectrum Analyzer", sf::Style::Titlebar | sf::Style::Close),
      waterfall_pixels_(config::PLOT_WIDTH * config::WATERFALL_HEIGHT * 4, 0) {
    window_.setVerticalSyncEnabled(true);
    window_.setKeyRepeatEnabled(false);
    font_present_ = loadFont(font_);
    if (!font_present_)
        std::fprintf(stderr, "No UI font found (try installing fonts-dejavu-core).\n");

    for (auto& v : spectrum_vertices_) v.color = sf::Color::Green;
    for (auto& v : hold_vertices_) v.color = sf::Color::Yellow;
    for (auto& v : threshold_vertices_) v.color = sf::Color(200, 0, 200);

    if (!spectrum_buffer_.create(config::FFT_SIZE) ||
        !hold_buffer_.create(config::FFT_SIZE) ||
        !threshold_buffer_.create(config::FFT_SIZE))
        throw std::runtime_error("Cannot allocate SFML vertex buffers");
    if (!waterfall_texture_.create(config::PLOT_WIDTH, config::WATERFALL_HEIGHT))
        throw std::runtime_error("Cannot create SFML waterfall texture");
    waterfall_sprite_.setTexture(waterfall_texture_);
    waterfall_sprite_.setPosition(0.f, static_cast<float>(config::SPECTRUM_HEIGHT));
    clearWaterfall();
}

std::vector<UserAction> SpectrumRenderer::pollActions() {
    std::vector<UserAction> actions;
    sf::Event ev;
    while (window_.pollEvent(ev)) {
        if (ev.type == sf::Event::Closed) {
            window_.close();
            break;
        }
        if (ev.type != sf::Event::KeyPressed) continue;
        switch (ev.key.code) {
            case sf::Keyboard::Left: actions.push_back(UserAction::TuneDown); break;
            case sf::Keyboard::Right: actions.push_back(UserAction::TuneUp); break;
            case sf::Keyboard::Down: actions.push_back(UserAction::StepDown); break;
            case sf::Keyboard::Up: actions.push_back(UserAction::StepUp); break;
            case sf::Keyboard::M: actions.push_back(UserAction::ToggleMaxHold); break;
            case sf::Keyboard::R: actions.push_back(UserAction::ResetMaxHold); break;
            case sf::Keyboard::T: actions.push_back(UserAction::ToggleThreshold); break;
            default: break;
        }
    }
    return actions;
}

void SpectrumRenderer::clearWaterfall() {
    std::fill(waterfall_pixels_.begin(), waterfall_pixels_.end(), 0);
    for (std::size_t i = 3; i < waterfall_pixels_.size(); i += 4)
        waterfall_pixels_[i] = 255;
    waterfall_texture_.update(waterfall_pixels_.data());
}

void SpectrumRenderer::drawWaterfall(const Spectrum& power) {
    const std::size_t row_size = config::PLOT_WIDTH * 4;
    std::memmove(waterfall_pixels_.data() + row_size, waterfall_pixels_.data(),
                 row_size * (config::WATERFALL_HEIGHT - 1));
    for (std::size_t j = 0; j < config::FFT_SIZE; ++j) {
        const double db = 10.0 * std::log10(power[fftIndex(j)] + 1e-12);
        const sf::Color color = heat((db - config::WATERFALL_DB_MIN) /
                                     (config::WATERFALL_DB_MAX - config::WATERFALL_DB_MIN));
        waterfall_pixels_[j * 4 + 0] = color.r;
        waterfall_pixels_[j * 4 + 1] = color.g;
        waterfall_pixels_[j * 4 + 2] = color.b;
        waterfall_pixels_[j * 4 + 3] = 255;
    }
    waterfall_texture_.update(waterfall_pixels_.data());
    window_.draw(waterfall_sprite_);
}

void SpectrumRenderer::drawSpectrum(const Spectrum& power, const Spectrum& hold,
                                    const DetectionResult& detections,
                                    const DisplayState& state) {
    markers_.clear();
    for (std::size_t j = 0; j < config::FFT_SIZE; ++j) {
        const auto k = fftIndex(j);
        const float x = static_cast<float>(j);
        spectrum_vertices_[j].position = {x, powerToY(power[k])};
        spectrum_vertices_[j].color = detections.detected[j] ? sf::Color::Red : sf::Color::Green;
        hold_vertices_[j].position = {x, powerToY(hold[k])};
        threshold_vertices_[j].position = {x, powerToY(detections.threshold[j])};
    }
    for (const auto& signal : detections.signals) {
        const float x = static_cast<float>(signal.peak_bin);
        markers_.append(sf::Vertex({x, 28.f}, sf::Color::Red));
        markers_.append(sf::Vertex({x, static_cast<float>(config::SPECTRUM_HEIGHT)}, sf::Color::Red));
    }
    spectrum_buffer_.update(spectrum_vertices_.data());
    hold_buffer_.update(hold_vertices_.data());
    threshold_buffer_.update(threshold_vertices_.data());
    if (state.show_max_hold) window_.draw(hold_buffer_);
    if (state.show_threshold) window_.draw(threshold_buffer_);
    window_.draw(spectrum_buffer_);
    window_.draw(markers_);
}

void SpectrumRenderer::drawLabels(const DetectionResult& detections,
                                  std::int64_t center_hz, std::int64_t hop_hz) {
    if (!font_present_) return;
    char buf[64];
    auto frequency = [&](double hz, float x) {
        std::snprintf(buf, sizeof(buf), "%.3f MHz", hz / 1e6);
        centeredText(window_, font_, buf, 16, x, 5.f, sf::Color::White);
    };
    frequency(center_hz - config::SAMPLE_RATE_HZ / 2.0, 60.f);
    frequency(center_hz, config::PLOT_WIDTH / 2.f);
    frequency(center_hz + config::SAMPLE_RATE_HZ / 2.0, config::PLOT_WIDTH - 60.f);
    text(window_, font_, "hop: " + formatHop(hop_hz), 16,
         5.f, config::SPECTRUM_HEIGHT - 25.f, sf::Color::White);

    int last_x = -1000;
    for (const auto& signal : detections.signals) {
        if (signal.peak_bin - last_x < 70) continue;
        const double hz = center_hz +
            (signal.peak_bin - static_cast<int>(config::FFT_SIZE) / 2) *
            static_cast<double>(config::SAMPLE_RATE_HZ) / config::FFT_SIZE;
        std::snprintf(buf, sizeof(buf), "%.3f", hz / 1e6);
        text(window_, font_, buf, 16, signal.peak_bin + 3.f, 28.f, sf::Color::Red);
        last_x = signal.peak_bin;
    }
}

void SpectrumRenderer::drawSidebar(std::int64_t center_hz, const DisplayState& state) {
    const float x = static_cast<float>(config::PLOT_WIDTH);
    rect(window_, x, 0, config::SIDEBAR_WIDTH, config::WINDOW_HEIGHT, ui::PANEL);
    rect(window_, x, 0, 2.f, config::WINDOW_HEIGHT, ui::BORDER);
    rect(window_, x + 20.f, 20.f, 4.f, 45.f, ui::ACCENT);
    rect(window_, x + 19.f, 108.f, config::SIDEBAR_WIDTH - 38.f, 1.f, ui::BORDER);
    rect(window_, x + 19.f, 207.f, config::SIDEBAR_WIDTH - 38.f, 1.f, ui::BORDER);
    rect(window_, x + 19.f, 624.f, config::SIDEBAR_WIDTH - 38.f, 1.f, ui::BORDER);
    rect(window_, x + 18.f, 222.f, config::SIDEBAR_WIDTH - 36.f, 80.f, ui::CARD);
    rect(window_, x + 34.f, 285.f, 8.f, 8.f,
         state.show_threshold ? ui::SUCCESS : ui::MUTED);
    if (!font_present_) return;

    text(window_, font_, "AntSDR E200", 22, x + 33.f, 18.f, ui::TEXT, true);
    text(window_, font_, "SPECTRUM  /  RX", 11, x + 34.f, 56.f, ui::ACCENT);
    sf::CircleShape live_dot(4.f);
    live_dot.setPosition(x + 23.f, 84.f);
    live_dot.setFillColor(ui::SUCCESS);
    window_.draw(live_dot);
    text(window_, font_, "AD9361  -  RX ACTIVE", 11, x + 36.f, 78.f, ui::MUTED);

    char freq[64];
    std::snprintf(freq, sizeof(freq), "%.3f MHz", center_hz / 1e6);
    text(window_, font_, "CENTER FREQUENCY", 11, x + 20.f, 122.f, ui::MUTED);
    text(window_, font_, freq, 27, x + 20.f, 143.f, ui::TEXT, true);
    text(window_, font_, "SPAN 2.4 MHz   |   RF BW 2.0 MHz", 11,
         x + 21.f, 183.f, ui::MUTED);

    text(window_, font_, "TUNING STEP", 11, x + 31.f, 232.f, ui::MUTED);
    text(window_, font_, formatHop(state.hopHz()), 18, x + 31.f, 250.f, ui::ACCENT, true);
    text(window_, font_, "MAX HOLD", 11, x + 185.f, 232.f, ui::MUTED);
    text(window_, font_, state.show_max_hold ? "VISIBLE" : "HIDDEN", 13,
         x + 185.f, 254.f, state.show_max_hold ? ui::SUCCESS : ui::MUTED, true);
    text(window_, font_, std::string("CFAR THRESHOLD: ") +
         (state.show_threshold ? "VISIBLE" : "HIDDEN"), 11,
         x + 49.f, 279.f, ui::MUTED);

    text(window_, font_, "CONTROLS", 13, x + 20.f, 318.f, ui::TEXT, true);
    text(window_, font_, "KEYBOARD SHORTCUTS", 10, x + 122.f, 322.f, ui::MUTED);
    controlRow(window_, font_, x, 351.f, "LEFT/RIGHT", "Tune", "Shift LO frequency");
    controlRow(window_, font_, x, 404.f, "UP/DOWN", "Step size", "10 kHz to 100 MHz");
    controlRow(window_, font_, x, 457.f, "M", "Max Hold", "Show / hide trace");
    controlRow(window_, font_, x, 510.f, "R", "Reset", "Clear Max Hold");
    controlRow(window_, font_, x, 563.f, "T", "CFAR Threshold", "Show / hide line");

    text(window_, font_, "RECEIVER SETTINGS", 11, x + 20.f, 632.f, ui::MUTED, true);
    text(window_, font_, "Sample rate", 11, x + 20.f, 653.f, ui::MUTED);
    text(window_, font_, "2.4 MSPS", 11, x + 200.f, 653.f, ui::TEXT);
    text(window_, font_, "RX gain", 11, x + 20.f, 675.f, ui::MUTED);
    text(window_, font_, "30 dB", 11, x + 200.f, 675.f, ui::TEXT);
}

void SpectrumRenderer::draw(const Spectrum& power, const Spectrum& hold,
                            const DetectionResult& detections,
                            std::int64_t center_hz, const DisplayState& state) {
    window_.clear(ui::BG);
    drawWaterfall(power);
    drawSpectrum(power, hold, detections, state);
    drawLabels(detections, center_hz, state.hopHz());
    drawSidebar(center_hz, state);
    window_.display();
}
