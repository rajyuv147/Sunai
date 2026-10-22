# Chooses the noise-removal AI:
#   RNNoise 0.2 (stronger, bigger) when its model has been downloaded
#     (scripts/fetch_rnnoise_model.sh, done automatically in the cloud build),
#   otherwise RNNoise 0.1.1 (smaller, model included in the repository).
# Both use 10 ms frames and add the same 10 ms delay.
set(RNN2 ${CMAKE_CURRENT_LIST_DIR}/rnnoise2)
set(RNN1 ${CMAKE_CURRENT_LIST_DIR}/rnnoise)

if(EXISTS ${RNN2}/src/rnnoise_data.c)
    message(STATUS "Sunai: using RNNoise 0.2 (stronger model)")
    set(RNNOISE_SRC
        ${RNN2}/src/denoise.c ${RNN2}/src/rnn.c ${RNN2}/src/pitch.c ${RNN2}/src/kiss_fft.c
        ${RNN2}/src/celt_lpc.c ${RNN2}/src/nnet.c ${RNN2}/src/nnet_default.c
        ${RNN2}/src/parse_lpcnet_weights.c ${RNN2}/src/rnnoise_tables.c ${RNN2}/src/rnnoise_data.c)
    add_library(rnnoise STATIC ${RNNOISE_SRC})
    target_include_directories(rnnoise PUBLIC ${RNN2}/include PRIVATE ${RNN2}/src)
    target_compile_definitions(rnnoise PUBLIC SUNAI_RNNOISE_V2=1)
    # Leave out the full-precision debug copy of the weights (the phone uses the
    # compact 8-bit copy), which keeps the app small.
    target_compile_definitions(rnnoise PRIVATE DISABLE_DEBUG_FLOAT=1)
else()
    message(STATUS "Sunai: using RNNoise 0.1.1 (run scripts/fetch_rnnoise_model.sh for the stronger model)")
    file(GLOB RNNOISE_SRC ${RNN1}/src/*.c)
    add_library(rnnoise STATIC ${RNNOISE_SRC})
    target_include_directories(rnnoise PUBLIC ${RNN1}/include)
endif()
