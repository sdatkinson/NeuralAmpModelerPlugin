cmake_minimum_required(VERSION 3.15)
project(NeuralAmpModelerTests LANGUAGES CXX)

enable_testing()

set(PLUGIN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(CORE_ROOT "${PLUGIN_ROOT}/NeuralAmpModelerCore")
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS "${CORE_ROOT}/NAM/*.cpp" "${CORE_ROOT}/NAM/wavenet/*.cpp")

# Keep every model architecture's static registry entry, even when tests use only get_dsp().
add_library(test_core OBJECT ${CORE_SOURCES})
target_compile_features(test_core PUBLIC cxx_std_20)
target_compile_definitions(test_core PUBLIC NAM_ENABLE_A2_FAST NOMINMAX)
target_include_directories(test_core PUBLIC
  "${PLUGIN_ROOT}/NeuralAmpModeler"
  "${CORE_ROOT}"
  "${CORE_ROOT}/Dependencies/nlohmann"
)
# Match the Eigen headers used by the plugin, rather than Core's nested checkout.
target_include_directories(test_core SYSTEM PUBLIC "${PLUGIN_ROOT}/eigen")

add_executable(test_resampling_nam test_resampling_nam.cpp)
target_link_libraries(test_resampling_nam PRIVATE test_core)
add_executable(test_cabinet_model test_cabinet_model.cpp)
target_link_libraries(test_cabinet_model PRIVATE test_core)
# Compare WAV playback levels with the old processor; these sources are no longer in the plugin targets.
target_sources(test_cabinet_model PRIVATE
  "${PLUGIN_ROOT}/AudioDSPTools/dsp/dsp.cpp"
  "${PLUGIN_ROOT}/AudioDSPTools/dsp/ImpulseResponse.cpp"
  "${PLUGIN_ROOT}/AudioDSPTools/dsp/wav.cpp"
)
target_compile_definitions(test_cabinet_model PRIVATE NAM_TEST_MODELS_DIR="${CORE_ROOT}/example_models")

add_test(NAME resampling_nam COMMAND test_resampling_nam)
add_test(NAME cabinet_model COMMAND test_cabinet_model)
