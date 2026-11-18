cmake_minimum_required(VERSION 3.16)
project(antsdr_spectrum_analyzer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBIIO REQUIRED IMPORTED_TARGET libiio)
pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
if(SFML_VERSION VERSION_GREATER_EQUAL "3.0")
    message(FATAL_ERROR "This project targets SFML 2.x (tested against 2.6), not SFML 3")
endif()

add_executable(antsdr_spectrum
    src/main.cpp
    src/Application.cpp
    src/SdrReceiver.cpp
    src/SpectrumProcessor.cpp
    src/CfarDetector.cpp
    src/SpectrumRenderer.cpp
)
target_include_directories(antsdr_spectrum PRIVATE include)
target_link_libraries(antsdr_spectrum PRIVATE
    PkgConfig::LIBIIO PkgConfig::FFTW3 sfml-graphics sfml-window sfml-system
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(antsdr_spectrum PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(cfar_tests tests/test_cfar.cpp src/CfarDetector.cpp)
    target_include_directories(cfar_tests PRIVATE include)
    add_test(NAME cfar_tests COMMAND cfar_tests)
endif()
