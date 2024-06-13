# CMake configuration for SML/NJ LLVM with JWA patches
#
# COPYRIGHT (c) 2024 The Fellowship of SML/NJ (http://www.smlnj.org)
# All rights reserved.
#

include_directories(include)
add_subdirectory(smlnj)

if (SMLNJ_CFGC_BUILD)
  message(STATUS "cfgc tool enabled.")
  add_subdirectory(cfgc)
endif()
