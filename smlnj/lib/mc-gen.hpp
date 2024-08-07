/// \file mc-gen.hpp
///
/// \copyright 2023 The Fellowship of SML/NJ (https://smlnj.org)
/// All rights reserved.
///
/// \brief Wrapper class for the low-level machine-specific parts of the code generator
///
/// \author John Reppy
///

#ifndef _MC_GEN_HPP_
#define _MC_GEN_HPP_

#include "code-object.hpp"

#include "llvm/IR/Module.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/IR/LegacyPassManager.h"

namespace smlnj {
namespace cfgcg {

class MCGen {
  public:

    MCGen (llvm::LLVMContext &context, const TargetInfo *target);

    /// per-module initialization
    void beginModule (llvm::Module *module);

    /// per-module finalization
    void endModule ();

    /// run the per-function optimizations over the functions of the module
    void optimize (llvm::Module *module);

    /// dump the code to an output file
    void dumpCode (llvm::Module *module, std::string const & stem, bool asmCode = true) const;

    /// compile the code into the code buffer's object-file backing store.
    void compile (class Context *codeBuf);

  private:
    const TargetInfo *_tgtInfo;
    std::unique_ptr<llvm::TargetMachine> _tgtMachine;
    std::unique_ptr<llvm::legacy::FunctionPassManager> _passMngr;

};

} // namespace cfgcg
} // namespace smlnj

#endif // !_MC_GEN_HPP_
