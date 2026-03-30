cmake_minimum_required(VERSION 3.16)

project(oceanography_waves_lib
  VERSION 0.1.0
  DESCRIPTION "Header-first C++ library for ocean surface wave simulation"
  LANGUAGES CXX)

include(GNUInstallDirs)

option(OWLIB_BUILD_TESTS "Build tests executable" ON)
option(OWLIB_BUILD_DATA_SIM "Build data simulation executable" ON)
option(OWLIB_BUILD_PLOTS "Build C++ plotting executables" OFF)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)

add_library(oceanography_waves INTERFACE)
add_library(oceanography_waves::oceanography_waves ALIAS oceanography_waves)

target_compile_features(oceanography_waves INTERFACE cxx_std_20)
target_include_directories(oceanography_waves
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_link_libraries(oceanography_waves INTERFACE Eigen3::Eigen)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(oceanography_waves INTERFACE
    -Wall -Wextra -Wpedantic -funroll-loops -fno-finite-math-only)
endif()

if(OWLIB_BUILD_TESTS)
  enable_testing()
  add_executable(owlib_tests tests/tests.cpp)
  target_link_libraries(owlib_tests PRIVATE oceanography_waves)
  add_test(NAME owlib_tests COMMAND owlib_tests)
endif()

if(OWLIB_BUILD_DATA_SIM)
  add_executable(waves_sim data-sim/waves_sim.cpp)
  target_link_libraries(waves_sim PRIVATE oceanography_waves)
endif()

if(OWLIB_BUILD_PLOTS)
  add_executable(fenton_plots_cpp plots/fenton_plots.cpp)
  target_link_libraries(fenton_plots_cpp PRIVATE oceanography_waves)
endif()

install(TARGETS oceanography_waves EXPORT oceanography_waves_targets)
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT oceanography_waves_targets
  FILE oceanography_waves_targets.cmake
  NAMESPACE oceanography_waves::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oceanography_waves)
