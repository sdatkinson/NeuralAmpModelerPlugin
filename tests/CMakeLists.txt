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

# Optional integration tests against a built plugin. The default tests still
# require no plugin SDK; supply a complete Steinberg SDK checkout to enable this.
set(NAM_VST3_SDK_ROOT "" CACHE PATH "Steinberg VST3 SDK checkout for plugin integration tests")
set(NAM_VST3_TEST_PLUGIN "" CACHE PATH "Built NeuralAmpModeler.vst3 bundle to test")
if(NAM_VST3_SDK_ROOT AND NAM_VST3_TEST_PLUGIN)
  if(APPLE)
    enable_language(OBJCXX)
  endif()
  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
  add_subdirectory("${NAM_VST3_SDK_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/vst3sdk" EXCLUDE_FROM_ALL)
  add_executable(test_stereo_plugin test_stereo_plugin.cpp
    "${NAM_VST3_SDK_ROOT}/public.sdk/source/vst/hosting/plugprovider.cpp"
    "${NAM_VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp"
  )
  # Match the SDK validator: its Windows loader expects u8string() to return std::string.
  set_target_properties(test_stereo_plugin PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
  target_include_directories(test_stereo_plugin PRIVATE "${CORE_ROOT}/Dependencies/nlohmann")
  target_link_libraries(test_stereo_plugin PRIVATE sdk_hosting)
  if(APPLE)
    set(MODULE_SOURCE "${NAM_VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm")
    set_source_files_properties("${MODULE_SOURCE}" PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(test_stereo_plugin PRIVATE "-framework Cocoa")
  elseif(WIN32)
    set(MODULE_SOURCE "${NAM_VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp")
  else()
    set(MODULE_SOURCE "${NAM_VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_linux.cpp")
  endif()
  target_sources(test_stereo_plugin PRIVATE "${MODULE_SOURCE}")
  add_test(NAME stereo_plugin COMMAND test_stereo_plugin "${NAM_VST3_TEST_PLUGIN}")
endif()
