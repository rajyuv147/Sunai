# Builds the sound engine on a normal computer (no phone needed) and runs checks.
cmake_minimum_required(VERSION 3.16)
project(sunai_native_tests C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CPP ${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp)
file(GLOB RNNOISE_SRC ${CPP}/third_party/rnnoise/src/*.c)

add_library(rnnoise STATIC ${RNNOISE_SRC})
target_include_directories(rnnoise PUBLIC ${CPP}/third_party/rnnoise/include)
target_compile_options(rnnoise PRIVATE -O2 -w)
target_link_libraries(rnnoise PUBLIC m)

add_executable(sunai_tests tests.cpp ${CPP}/dsp/Processor.cpp)
target_include_directories(sunai_tests PRIVATE ${CPP}/dsp)
target_compile_options(sunai_tests PRIVATE -O2 -Wall -Wextra)
target_link_libraries(sunai_tests PRIVATE rnnoise)
