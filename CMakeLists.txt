# CMake configuration for SML/NJ LLVM with JWA patches
#
# COPYRIGHT (c) 2024 The Fellowship of SML/NJ (http://www.smlnj.org)
# All rights reserved.
#

# LLVM 10.0 only requires 3.4.3, but we'll require a more recent version
#
cmake_minimum_required(VERSION 3.20.0)

# Disable builds in the source tree to prevent corruption.
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "
ERROR: builds in the source tree are not allowed.
       Create a separate directory for build files.
")
endif()

# check the build type
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, default to Release")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (default Release)" FORCE)
endif()

# LLVM 10.0 uses C++14
#
set(CMAKE_CXX_STANDARD 14 CACHE STRING "C++ standard to conform to")
set(CMAKE_CXX_STANDARD_REQUIRED YES)

project(smlnj-llvm
  VERSION 10.0.1
  DESCRIPTION "LLVM 10.0.1 with modifications to support SML/NJ"
  HOMEPAGE_URL https://smlnj.org
  LANGUAGES C CXX)

# build options
#
option (SMLNJ_CFGC_BUILD
  "Enable building the cfgc command-line tool" OFF)

## Begin LLVM Configuration ##
message(STATUS "** START LLVM CONFIGURATION **")
add_subdirectory(llvm)
message(STATUS "** LLVM CONFIGURATION DONE **")
## End LLVM Configuration ##

# enable verbose makefiles during development
#
set(CMAKE_VERBOSE_MAKEFILE ON)

# The CFG Code Generation Library
#
include_directories(include)
add_subdirectory(smlnj)
