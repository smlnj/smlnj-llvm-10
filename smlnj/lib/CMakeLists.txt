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

target_compile_options(CFGCodeGen PRIVATE -fno-exceptions -fno-rtti)
target_compile_definitions(CFGCodeGen PRIVATE ${OPSYS} ${ARCH})
target_include_directories(CFGCodeGen PRIVATE ${CMAKE_BINARY_DIR}/llvm/include ${CMAKE_SOURCE_DIR}/llvm/include)

install(TARGETS CFGCodeGen)
