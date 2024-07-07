# CMake configuration for SML/NJ LLVM with JWA patches
#
# COPYRIGHT (c) 2024 The Fellowship of SML/NJ (http://www.smlnj.org)
# All rights reserved.
#

# workaround for older versions of CMake
#
if (CMAKE_VERSION VERSION_LESS 3.25)
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX ON CACHE BOOL "Set to true when the target system is Linux" INTERNAL)
  endif()
endif()

# determine the operating system
#
if (APPLE)
  set(OPSYS "OPSYS_DARWIN")
elseif (LINUX)
  set(OPSYS "OPSYS_LINUX")
else ()
  message(FATAL_ERROR "unsupported operating system")
endif()

# determine the archicture
#
if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  set(ARCH "ARCH_AMD64")
elseif (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "arm64")
  set(ARCH "ARCH_ARM64")
else ()
  message(FATAL_ERROR "unsupported architecture ${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

include_directories(include)
add_subdirectory(include)
add_subdirectory(lib)

if (SMLNJ_CFGC_BUILD)
  message(STATUS "cfgc tool enabled.")
  add_subdirectory(cfgc)
endif()
