cmake_minimum_required(VERSION 3.14)
set(LIBNAME PicoOSC)

add_library(${LIBNAME} INTERFACE)

project(
	${LIBNAME}
  VERSION 0.1.0
  DESCRIPTION "A simple OSC communication library for the Raspberry Pi Pico"
  HOMEPAGE_URL " "
  LANGUAGES NONE)

target_include_directories(${LIBNAME} INTERFACE include)

# Add target sources
target_sources(${LIBNAME} INTERFACE include/PicoOSC/PicoOSC.hpp)
