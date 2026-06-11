cmake_minimum_required(VERSION 3.16)

project(spectral_noise_reducer_dsp_example
    VERSION 1.0.0
    DESCRIPTION "CV_DSP SpectralNoiseReducer standalone smoke example"
    LANGUAGES CXX
)

add_executable(spectral_noise_reducer_smoke
    spectral_noise_reducer_smoke.cpp
)

target_compile_features(spectral_noise_reducer_smoke PRIVATE cxx_std_20)
target_include_directories(spectral_noise_reducer_smoke PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../..
)

if(MSVC)
    target_compile_options(spectral_noise_reducer_smoke PRIVATE /W4)
else()
    target_compile_options(spectral_noise_reducer_smoke PRIVATE -Wall -Wextra -Wpedantic)
endif()
