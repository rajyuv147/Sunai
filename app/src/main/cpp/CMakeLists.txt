cmake_minimum_required(VERSION 3.22.1)
project(sunai C CXX)

# Speed flags: full optimisation, hidden symbols, unused code removed at link time.
set(SPEED_FLAGS -O3 -fvisibility=hidden -ffunction-sections -fdata-sections)

# Oboe (Google's low-latency audio library), built from source as a static
# library so the app needs no extra .so files or C++ runtime.
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/oboe ${CMAKE_CURRENT_BINARY_DIR}/oboe)
target_compile_options(oboe PRIVATE ${SPEED_FLAGS})

# RNNoise (AI noise reduction), also static. Picks the 0.2 model when downloaded.
include(${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise.cmake)
target_compile_options(rnnoise PRIVATE ${SPEED_FLAGS} -w)

# The app's own native library: one small .so per processor type.
add_library(sunai SHARED
    jni_bridge.cpp
    audio/AudioEngine.cpp
    dsp/Processor.cpp)

target_include_directories(sunai PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/oboe/include)
target_compile_options(sunai PRIVATE ${SPEED_FLAGS} -Wall)
target_link_options(sunai PRIVATE
    -Wl,--gc-sections
    -Wl,--exclude-libs,ALL
    -Wl,-z,max-page-size=16384)   # 16 KB pages, required by newer Android versions
target_link_libraries(sunai PRIVATE oboe rnnoise log android OpenSLES m)
