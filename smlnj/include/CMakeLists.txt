# CMake configuration for the CFGCodeGen library interface
#
# COPYRIGHT (c) 2024 The Fellowship of SML/NJ (https://smlnj.org)
# All rights reserved.
#

set(SRCS
  cfg.hpp
  code-buffer.hpp
  code-object.hpp
  lambda-var.hpp
  objfile-pwrite-stream.hpp
  sml-registers.hpp
  target-info.hpp)

install(DIRECTORY asdl TYPE INCLUDE)
install(FILES ${SRCS} TYPE INCLUDE)
