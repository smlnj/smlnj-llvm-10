# CMake configuration for the Codegen Library
#
# COPYRIGHT (c) 2024 The Fellowship of SML/NJ (https://smlnj.org)
# All rights reserved.
#

set(SRCS
  asdl-integer.cpp
  asdl.cpp
  cfg-codegen.cpp
  cfg-init.cpp
  cfg-prim-codegen.cpp
  cfg.cpp
  code-buffer.cpp
  code-object.cpp
  lambda-var.cpp
  mc-gen.cpp
  objfile-pwrite-stream.cpp
  overflow.cpp
  sml-registers.cpp
  target-info.cpp)

add_library(CFGCodeGen STATIC ${SRCS})

add_dependencies(CFGCodeGen llvm-headers)

target_compile_options(CFGCodeGen PRIVATE -fno-exceptions -fno-rtti)
target_compile_definitions(CFGCodeGen PRIVATE ${OPSYS} ${ARCH})
target_include_directories(CFGCodeGen PRIVATE
  ${CMAKE_BINARY_DIR}/smlnj/include
  ${CMAKE_BINARY_DIR}/llvm/include ${CMAKE_SOURCE_DIR}/llvm/include)

# enable targets in the code generator library
#
foreach(T IN LISTS LLVM_TARGETS_TO_BUILD)
  if(${T} STREQUAL "X86")
    message(STATUS "enable X86 target")
    target_compile_definitions(CFGCodeGen PRIVATE ENABLE_X86)
  elseif(${T} STREQUAL "AArch64")
    message(STATUS "enable ARM64 target")
    target_compile_definitions(CFGCodeGen PRIVATE ENABLE_ARM64)
  else()
    message(FATAL_ERROR "unknown target architecture ${T}")
  endif()
endforeach()

install(TARGETS CFGCodeGen)
