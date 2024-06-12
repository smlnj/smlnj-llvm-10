#!/bin/sh
#
# COPYRIGHT (c) 2024 The Fellowship of SML/NJ (http://www.smlnj.org)
# All rights reserved.
#
# This script handles building the LLVM code generator library as part
# of the SML/NJ installation process.  It will use available parallelism
# and normally builds a code generator that supports just a single target.
#
# usage: build-llvm.sh [options]
#
# options:
#       -h | -help      -- print help message
#       -all-targets    -- build a version of LLVM that supports all hardware targets
#                          that are known to SML/NJ
#       -build-cfgc     -- build the cfgc (CFG compiler) tool
#       -debug          -- build a debug release of LLVM (WARNING: debug releases take
#                          much longer to build and are signficantly slower than the
#                          default release builds)
#       -docs           -- build LLVM API documentation
#       -config         -- configure, but do not compile
#       -install <dir>  -- specify installation directory; default is this directory
#       -ninja          -- generate build.ninja files (instead of Unix makefiles)
#

# get location of this script as an absolute path
#
CMDDIR=`dirname "$0"`
LLVMDIR=$(cd $CMDDIR; pwd)

usage() {
  echo "usage: build-llvm.sh [ options ]"
  echo "options:"
  echo "    -h, -help       print this message and exit"
  echo "    -all-targets    build a version of LLVM that supports all hardware targets"
  echo "                    that are known to SML/NJ"
  echo "    -build-cfgc     build the cfgc (CFG compiler) tool."
  echo "    -debug          build a debug version of the LLVM libraries"
  echo "    -docs           build LLVM API documentation"
  echo "    -config         configure, but do not compile"
  echo "    -install <dir>  specify installation directory (default: $CMDDIR)"
  echo "    -ninja          generate build.ninja files (instead of Unix makefiles)"
  exit $1
}

CONFIG_ONLY=no
LLVM_BUILD_TYPE=Release
USE_GOLD_LD=no
NPROCS=2
GENERATOR="Unix Makefiles"

# default place to put
INSTALL_PREFIX="$LLVMDIR"

# system specific defaults
#
case `uname -s` in
  Darwin)
    case `uname -p` in
      arm) # on arm processors, we only use the performance cores
        NPROCS=$(sysctl -n hw.perflevel0.physicalcpu)
        ;;
      *) # otherwise use the physical core count
        NPROCS=$(sysctl -n hw.physicalcpu)
        ;;
    esac
  ;;
  Linux)
    USE_GOLD_LD=yes
    if [ -x /bin/nproc ] ; then
      # NPROCS reports the number of hardware threads, which is usually twice the
      # number of actual cores, so we will divide by two.
      NPROCS=$(/bin/nproc --all)
      if [ $NPROCS -gt 4 ] ; then
         NPROCS=$(($NPROCS / 2))
      fi
    fi
  ;;
  *)
    echo "build-llvm.sh: unsupported system"
    exit 1
  ;;
esac

ALL_TARGETS="AArch64;X86"
case $(uname -m) in
  x86_64) TARGETS="X86" ;;
  arm64) TARGETS="AArch64" ;;
  *) echo "unknown hardware platform"
    exit 1
    ;;
esac

# process command-line arguments
#
BUILD_CFGC=NO
while [ "$#" != "0" ] ; do
  arg=$1; shift
  case $arg in
    -h|-help)
      usage 0
      ;;
    -all-targets)
      TARGETS=$ALL_TARGETS
      ;;
    -build-cfgc)
      BUILD_CFGC=YES
      ;;
    -debug)
      LLVM_BUILD_TYPE=Debug
      ;;
    -docs)
      BUILD_DOCS=yes
      ;;
    -config)
      CONFIG_ONLY=yes
      ;;
    -install)
      if [ "$#" != "0" ] ; then
        INSTALL_PREFIX=$1; shift
      else
        echo "build-llvm.sh: missing installation path"
        usage 1
      fi
      ;;
    -ninja)
      GENERATOR="Ninja"
      ;;
    *)
      echo "build-llvm.sh: invalid option '$arg'"
      usage 1
      ;;
  esac
done

if [ $LLVM_BUILD_TYPE = "Debug" ] ; then
  PRESET=smlnj-llvm-debug
else
  PRESET=smlnj-llvm-release
fi

# check that we have a version of CMake that understands presets
#
cmake llvm --list-presets > /dev/null 2>&1
if [ $? != 0 ] ; then
  echo "Installation of SML/NJ requires CMake version 3.19 or later"
  exit 1
fi

# most of the definitions are specified in the CMakePresets.json file,
# but we define a few here based on the command-line options
#
CMAKE_DEFS="\
  -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
  -DLLVM_TARGETS_TO_BUILD=$TARGETS \
  -DLLVM_ENABLE_DUMP=ON \
"

if [ x"$USE_GOLD_LD" = xyes ] ; then
  CMAKE_DEFS="$CMAKE_DEFS -DLLVM_USE_LINKER=gold"
fi

if [ x"$BUILD_DOCS" = xyes ] ; then
  CMAKE_DEFS="$CMAKE_DEFS -DLLVM_BUILD_DOCS=ON -DLLVM_INCLUDE_DOCS=ON -DLLVM_ENABLE_DOXYGEN=ON"
fi

# remove the build directory if it exists
#
if [ -d build ] ; then
  echo  "$0: removing existing build etc. directories"
  rm -rf build bin lib include
fi

echo "build-llvm.sh: mkdir build"
mkdir build
cd build

echo "build-llvm.sh: configuring build"
echo "  cmake --preset=$PRESET -G \"$GENERATOR\" $CMAKE_DEFS ../src"
cmake --preset=$PRESET -G "$GENERATOR" $CMAKE_DEFS ../llvm || exit 1

if [ x"$CONFIG_ONLY" = xno ] ; then

  echo "build-llvm.sh: building on $NPROCS cores"
  echo "  cmake --build . -j $NPROCS -t install"
  time cmake --build . -j $NPROCS -t install

  # if requested, build the CFG compiler
  #
  if [ x"$BUILD_CFGC" = xYES ] ; then
    cd ../cfgc/src
    make install
  fi

fi
