project(UnitTesting)
Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)
include_directories(
	${CMAKE_SOURCE_DIR}/.. 
	${CMAKE_SOURCE_DIR}/../../iPlug2/Dependencies/Extras/nlohmann
	${CMAKE_SOURCE_DIR}/../../iPlug2/WDL
)

add_executable(tests 
	TestMain.cpp 
	TestNeuralAmpModelerPreset.cpp 
	../NeuralAmpModelerPreset.h
	../NeuralAmpModelerPreset.cpp
)
set_property(TARGET tests PROPERTY CXX_STANDARD 17)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
