cmake_minimum_required(VERSION 3.14)

project(
  PicoOSC
  VERSION 0.1.0
  DESCRIPTION "A simple OSC communication library for the Raspberry Pi Pico"
  HOMEPAGE_URL " "
  LANGUAGES NONE)

# ---- Declare library ----

add_library(PicoOSC_PicoOSC INTERFACE)
add_library(PicoOSC::PicoOSC ALIAS
            PicoOSC_PicoOSC)

set_property(TARGET PicoOSC_PicoOSC
             PROPERTY EXPORT_NAME PicoOSC)

target_include_directories(
  PicoOSC_PicoOSC ${warning_guard}
  INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

target_compile_features(PicoOSC_PicoOSC
                        INTERFACE cxx_std_20)
