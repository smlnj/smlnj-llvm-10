# `cfgc` -- a Tool for Compiling CFG Pickles

This directory contains the source for a command-line tool for compiling
the CFG IR pickle files to target code.  It's purpose is to help debug
the SML/NJ backend (the generated code is not useful, since it lacks
the metadata needed to support SML/NJ linking).

**WARNING**: this source tree includes the unpickler code generated by
**asdlgen**.  If the CFG IR (or ASDL specification) changes, then these
files (`include/cfg.hpp` and `src/cfg.cpp`) must be updated.

## Usage

``` bash
usage: cfgc [ -o | -S | -c ] [ --emit-llvm ] [ --bits ] [ --target <target> ] <pkl-file>
```

In default mode, this tool prints the assembly code for the given CFG pickle
file.  A number of options affect the output:

* **-o** -- produce a target object (*i.e.*, `.o`) file

* **-S** -- produce a target assembly (*i.e.*, `.s`) file

* **-c** -- print summary information about the sections in the generated
  code.

* **--emit-llvm** -- emit LLVM assembly code

* **--bits** -- when combined with the "**-c**" flag, this also prints the binary
  code (after relocation patching)

* **--target** *<target>* -- generate code for the specified target architecture
  (either "aarch64" or "x86_64").