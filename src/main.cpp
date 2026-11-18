#include "Application.hpp"
#include "Config.hpp"
#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
    const char* uri = argc > 1 ? argv[1] : config::DEFAULT_URI;
    try {
        Application app(uri);
        app.run();
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return 1;
    }
}
