cmake_minimum_required(VERSION 3.16)

project(spectral_noise_reducer_dsp_example
    VERSION 1.0.0
    DESCRIPTION "CV_DSP SpectralNoiseReducer standalone smoke example"
    LANGUAGES CXX
)

add_executable(spectral_noise_reducer_smoke
    spectral_noise_reducer_smoke.cpp
)

add_executable(realtime_noise_reducer_benchmark
    realtime_noise_reducer_benchmark.cpp
)

target_compile_features(spectral_noise_reducer_smoke PRIVATE cxx_std_20)
target_compile_features(realtime_noise_reducer_benchmark PRIVATE cxx_std_20)
target_include_directories(spectral_noise_reducer_smoke PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../..
)
target_include_directories(realtime_noise_reducer_benchmark PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../..
)

if(MSVC)
    target_compile_options(spectral_noise_reducer_smoke PRIVATE /W4)
    target_compile_options(realtime_noise_reducer_benchmark PRIVATE /W4)
else()
    target_compile_options(spectral_noise_reducer_smoke PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(realtime_noise_reducer_benchmark PRIVATE -Wall -Wextra -Wpedantic)
endif()
