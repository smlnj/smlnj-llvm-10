# CMake configuration for the cfgc tool
#
# COPYRIGHT (c) 2024 The Fellowship of SML/NJ (https://smlnj.org)
# All rights reserved.
#

# determine the LLVM libraries
llvm_map_components_to_libnames(LLVM_LIBS ${LLVM_TARGETS_TO_BUILD})

set(SRCS
  main.cpp)

add_executable(cfgc ${SRCS})
add_dependencies(cfgc CFGCodeGen)

target_compile_options(cfgc PRIVATE -fno-exceptions -fno-rtti)
target_compile_definitions(cfgc PRIVATE ${OPSYS} ${ARCH})
target_include_directories(cfgc PRIVATE
  ${CMAKE_BINARY_DIR}/smlnj/include
  ${CMAKE_BINARY_DIR}/llvm/include ${CMAKE_SOURCE_DIR}/llvm/include)
target_link_libraries(cfgc CFGCodeGen ${LLVM_LIBS})

install(TARGETS cfgc)
