cmake_minimum_required(VERSION 3.16)
project(MidiCombo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)

# Music theory core: no Qt, no ALSA, so it can be unit tested on its own.
add_library(midicombo_core STATIC
    src/Chord.cpp
    src/ChordDetector.cpp
    src/Progression.cpp
    src/Style.cpp
)
target_include_directories(midicombo_core PUBLIC src)
target_compile_options(midicombo_core PRIVATE -Wall -Wextra)

add_executable(midicombo
    src/main.cpp
    src/MidiEngine.cpp
    src/BandEngine.cpp
    src/MainWindow.cpp
    src/PresetFile.cpp
    src/StyleFile.cpp
    src/Widgets.cpp
)
target_compile_options(midicombo PRIVATE -Wall -Wextra)
target_link_libraries(midicombo PRIVATE midicombo_core Qt6::Widgets PkgConfig::ALSA Threads::Threads)

enable_testing()
add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE midicombo_core)
add_test(NAME core COMMAND test_core)

add_executable(test_presets tests/test_presets.cpp src/PresetFile.cpp)
target_include_directories(test_presets PRIVATE src)
target_link_libraries(test_presets PRIVATE Qt6::Core)
add_test(NAME presets COMMAND test_presets)

add_executable(test_styles tests/test_styles.cpp src/StyleFile.cpp)
target_compile_definitions(test_styles PRIVATE EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
target_link_libraries(test_styles PRIVATE midicombo_core Qt6::Core)
add_test(NAME styles COMMAND test_styles)

install(TARGETS midicombo RUNTIME DESTINATION bin)
install(DIRECTORY examples/styles/ DESTINATION share/midicombo/styles)
