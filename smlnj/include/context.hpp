/// \file context.hpp
///
/// \copyright 2023 The Fellowship of SML/NJ (https://smlnj.org)
/// All rights reserved.
///
/// \brief The Context class is an extension of the `LLVMContext` class
///        and wraps up the LLVM code generator state used to generate code.
///
/// \author John Reppy
///

#ifndef _CONTEXT_HPP_
#define _CONTEXT_HPP_

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/ADT/SmallVector.h"

/*DEBUG*/#include "llvm/Support/raw_os_ostream.h"
/*DEBUG*/#include "llvm/Support/Debug.h"

#include "lambda-var.hpp"
#include "cm-registers.hpp"
#include "code-object.hpp"
#include "objfile-pwrite-stream.hpp"

using Types_t = std::vector<llvm::Type *>;
using Args_t = std::vector<llvm::Value *>;

namespace llvm {
    class TargetMachine;
    namespace legacy {
        class FunctionPassManager;
    }
}

namespace CFG {
    class param;
    class frag;
    class attrs;
    class cluster;
    enum class frag_kind;
}

namespace smlnj {
namespace cfgcg {

struct TargetInfo;

// map from lvars to values of type T*
template <typename T>
using lvar_map_t = std::unordered_map<LambdaVar::lvar, T *>;

// the different kinds of fragments.  The first two are restricted
// to entry fragments for clusters; all others are `INTERNAL`
//
using frag_kind = CFG::frag_kind;

/// The Context class encapsulates the current state of code generation, as well
/// as information about the target architecture.  It is passed as an argument to
/// all of the `codegen` methods in the CFG representation.
//
class Context : public llvm::LLVMContext {
  public:

    /// static function for creating the code buffer for the given target
    /// \param target specifies the target architecture
    static Context *create (const TargetInfo * target);

    /// static function for creating the code buffer for the given target
    /// \param target specifies the target architecture
    static Context *create (std::string const & target);

    void optimize ();

    /// initialize the code buffer for a new module
    void beginModule (std::string const & src, int nClusters);

    /// finish up LLVM code generation for the module
    void completeModule ();

    /// delete the module after code generation
    void endModule ();

    /// return the current module as a `const` pointer
    llvm::Module const *module () const { return this->_module; }

    /// return the current module
    llvm::Module *module () { return this->_module; }

    /// set the current cluster (during preperation for code generation)
    void setCluster (CFG::cluster *cluster) { this->_curCluster = cluster; }

    /// mark the beginning of a cluster for code generation
    void beginCluster (CFG::cluster *cluster, llvm::Function *fn);
    /// mark the end of a cluster for code generation
    void endCluster ();

    /// initialize the code buffer for a new fragment
    void beginFrag ();

    /// get the IR builder
    llvm::IRBuilder<> & build () { return this->_builder; }

    /// define a new LLVM function for a cluster with the given type; the `isFirst` flag
    /// should be true for the entry function of the module.
    llvm::Function *newFunction (llvm::FunctionType *fnTy, std::string const &name, bool isFirst);

    /// create a function type from a vector of parameter types.  This function adds
    /// the extra types corresponding to the SML registers and for the unused
    /// argument registers for continuations
    llvm::FunctionType *createFnTy (frag_kind kind, Types_t const & tys) const;

    /// create a vector to hold the types of function paramaters (including for fragments),
    /// where `n` is the number of arguments to the call.  This method initialize the
    /// prefix of the vector with the types of the SML registers
    Types_t createParamTys (frag_kind kind, int n) const;

    /// create a vector to hold the arguments of a call (APPLY/THROW/GOTO), where
    /// `n` is the number of arguments to the call.  This method initialize the
    /// prefix of the vector with the values of the SML registers
    Args_t createArgs (frag_kind kind, int n);

    void setupStdEntry (CFG::attrs *attrs, CFG::frag *frag);

    /// setup the parameter lists for a fragment
    void setupFragEntry (CFG::frag *frag, std::vector<llvm::PHINode *> &phiNodes);

    /// get the LLVM value that represents the specified SML register
    llvm::Value *mlReg (CMRegId r)
    {
        llvm::Value *reg = this->_regState.get(r);
        if (reg == nullptr) {
            return this->_loadMemReg(r);
        } else {
            return reg;
        }
    }

    /// assign a value to an SML register
    void setMLReg (CMRegId r, llvm::Value *v)
    {
        llvm::Value *reg = this->_regState.get(r);
        if (reg == nullptr) {
            return this->_storeMemReg(r, v);
        } else {
            this->_regState.set(r, v);
        }
    }

    /// save and restore the CM register state to a cache object
    void saveSMLRegState (CMRegState & cache) { cache.copyFrom (this->_regState); }
    void restoreSMLRegState (CMRegState const & cache) { this->_regState.copyFrom (cache); }

    /// target parameters

    /// the size of a target machine word in bytes
    int wordSzInBytes () const { return this->_wordSzB; }

    /// round a size (in bytes) up to the nearest multiple of the word size.
    size_t roundToWordSzInBytes (size_t nb) const
    {
        return (nb + (this->_wordSzB - 1)) & ~(this->_wordSzB - 1);
    }

    /// round a size in bytes up to the number of words
    size_t roundToWordSz (size_t nb) const
    {
        return ((nb + (this->_wordSzB - 1)) & ~(this->_wordSzB - 1)) / this->_wordSzB;
    }

    /// is the target a 64-bit machine?
    bool is64Bit () const { return (this->_wordSzB == 8); }

    /// return a ponter to the target information struct
    TargetInfo const *targetInfo () const { return this->_target; }

    /// align the allocation pointer for 64 bits on 32-bit machines.  The resulting
    /// alloc pointer points to the location of the object descriptor, so adding
    /// wordSzInBytes() should produce an 8-byte aligned address
    llvm::Value *alignedAllocPtr ()
    {
        if (this->is64Bit()) {
            return this->mlReg (CMRegId::ALLOC_PTR);
        } else {
            return this->createIntToPtr(
                this->createOr(
                    this->createPtrToInt (this->mlReg (CMRegId::ALLOC_PTR)),
                    this->uConst (4)),
		this->objPtrTy);
        }
    }

  // cached types
    /// @{
    llvm::Type *voidTy;         ///< the "void" type
    llvm::IntegerType *i8Ty;    ///< 8-bit integer type
    llvm::IntegerType *i16Ty;   ///< 16-bit integer type
    llvm::IntegerType *i32Ty;   ///< 32-bit integer type
    llvm::IntegerType *i64Ty;   ///< 64-bit integer type
    llvm::Type *f32Ty;          ///< 32-bit floating-point type
    llvm::Type *f64Ty;          ///< 64-bit floating-point type
    llvm::IntegerType *intTy;   ///< the native integer type
    llvm::Type *mlValueTy;      ///< the uniform ML value type, which is a pointer to the intTy
    llvm::Type *objPtrTy;       ///< pointer into the heap (i.e., a pointer to an ML value)
    llvm::Type *bytePtrTy;      ///< "char *" type
    /// @}

    /// return the integer type of the specified bit size
    llvm::IntegerType *iType (int sz) const
    {
        if (sz == 64) return this->i64Ty;
        else if (sz == 32) return this->i32Ty;
        else if (sz == 16) return this->i16Ty;
        else return this->i8Ty;
    }

    /// return the floating-point type of the specified bit size
    llvm::Type *fType (int sz) const
    {
        if (sz == 64) return this->f64Ty;
        else return this->f32Ty;
    }

    /// ensure that a value has the `mlValue` type
    llvm::Value *asMLValue (llvm::Value *v)
    {
        auto ty = v->getType();
        if (ty == this->mlValueTy) {
            return v;
        } else if (ty->isPointerTy()) {
            return this->_builder.CreateBitCast (v, this->mlValueTy);
        } else {
            return this->_builder.CreateIntToPtr(v, this->mlValueTy);
        }
    }

    /// ensure that a value has the pointer to `mlValue` type
    llvm::Value *asObjPtr (llvm::Value *v)
    {
        auto ty = v->getType();
        if (! ty->isPointerTy()) {
            return this->_builder.CreateIntToPtr(v, this->objPtrTy);
        } else if (ty != this->objPtrTy) {
            return this->_builder.CreateBitCast(v, this->objPtrTy);
        } else {
            return v;
        }
    }

    /// ensure that a value has the LLVM type `i8*`
    llvm::Value *asBytePtr (llvm::Value *v)
    {
        auto ty = v->getType();
        if (! ty->isPointerTy()) {
            return this->_builder.CreateIntToPtr(v, this->bytePtrTy);
        } else if (ty != this->bytePtrTy) {
            return this->_builder.CreateBitCast(v, this->bytePtrTy);
        } else {
            return v;
        }
    }

    /// ensure that a value is a machine-sized int type (assume that it is
    /// either a intTy or mlValueTy value)
    llvm::Value *asInt (llvm::Value *v)
    {
        if (v->getType()->isPointerTy()) {
            return this->_builder.CreatePtrToInt(v, this->intTy);
        } else {
            return v;
        }
    }

    /// helper function to ensure that arguments to arithmetic operations
    /// have an LLVM integer type, since we use i64* (or i32*) as the type
    /// of ML values
    llvm::Value *asInt (unsigned sz, llvm::Value *v)
    {
        if (v->getType() == this->mlValueTy) {
            return this->_builder.CreatePtrToInt(v, this->iType(sz));
        } else {
            return v;
        }
    }

    /// cast an argument type to match the expected target type.  We assume that the
    /// types are _not_ equal!
    llvm::Value *castTy (llvm::Type *srcTy, llvm::Type *tgtTy, llvm::Value *v);

    /// SML unit value with ML value type
    llvm::Value *unitValue ()
    {
        return this->_builder.CreateIntToPtr(
            llvm::ConstantInt::getSigned (this->intTy, 1),
            this->mlValueTy);
    }

/** NOTE: we may be able to avoid needing the signed constants */
    /// signed integer constant of specified bit size
    llvm::ConstantInt *iConst (int sz, int64_t c) const
    {
        return llvm::ConstantInt::getSigned (this->iType (sz), c);
    }
    /// signed constant of native size
    llvm::ConstantInt *iConst (int64_t c) const
    {
        return llvm::ConstantInt::getSigned (this->intTy, c);
    }
    llvm::ConstantInt *i32Const (int32_t n) const
    {
        return llvm::ConstantInt::getSigned (this->i32Ty, n);
    }

    /// unsigned integer constant of specified bit size
    llvm::ConstantInt *uConst (int sz, uint64_t c) const
    {
        return llvm::ConstantInt::get (this->iType (sz), c);
    }
    /// unsigned constant of native size
    llvm::ConstantInt *uConst (uint64_t c) const
    {
        return llvm::ConstantInt::get (this->intTy, c);
    }
    llvm::ConstantInt *u32Const (uint32_t n) const
    {
        return llvm::ConstantInt::get (this->i32Ty, n);
    }

    /// insert a binding into the label-to-cluster map
    void insertCluster (LambdaVar::lvar lab, CFG::cluster *cluster)
    {
        std::pair<LambdaVar::lvar,CFG::cluster *> pair(lab, cluster);
        this->_clusterMap.insert (pair);
    }

    /// lookup a binding in the label-to-cluster map
    CFG::cluster *lookupCluster (LambdaVar::lvar lab)
    {
        lvar_map_t<CFG::cluster>::const_iterator got = this->_clusterMap.find(lab);
        if (got == this->_clusterMap.end()) {
            return nullptr;
        } else {
            return got->second;
        }
    }

    /// create an alias for the expression `f1 - f2`, where `f1` and `f2` are
    /// the labels of the two functions.
    llvm::Constant *labelDiff (llvm::Function *f1, llvm::Function *f2);

    /// create an alias for the expression `lab - entry`, where `lab` is the
    /// block label of `bb` and `entry` is the label of the current function.
    llvm::Constant *blockDiff (llvm::BasicBlock *bb);

    /// evaluate a LABEL (which maps to the given function) to an absolute address
    llvm::Value *evalLabel (llvm::Function *fn);

    /// insert a binding into the label-to-fragment map
    void insertFrag (LambdaVar::lvar lab, CFG::frag *frag)
    {
        std::pair<LambdaVar::lvar,CFG::frag *> pair(lab, frag);
        this->_fragMap.insert (pair);
    }

    /// lookup a binding in the label-to-fragment map
    CFG::frag *lookupFrag (LambdaVar::lvar lab)
    {
        lvar_map_t<CFG::frag>::const_iterator got = this->_fragMap.find(lab);
        if (got == this->_fragMap.end()) {
            return nullptr;
        } else {
            return got->second;
        }
    }

    /// insert a binding into the lvar-to-value map
    void insertVal (LambdaVar::lvar lv, llvm::Value *v)
    {
        std::pair<LambdaVar::lvar,llvm::Value *> pair(lv, v);
        this->_vMap.insert (pair);
    }

    /// lookup a binding in the lvar-to-value map
    llvm::Value *lookupVal (LambdaVar::lvar lv)
    {
        lvar_map_t<llvm::Value>::const_iterator got = this->_vMap.find(lv);
        if (got == this->_vMap.end()) {
            return nullptr;
        } else {
            return got->second;
        }
    }

    /// create a fresh basic block in the current function
    llvm::BasicBlock *newBB (const llvm::Twine &name="")
    {
        return llvm::BasicBlock::Create (*this, name, this->_curFn);
    }

    /// return the block address for a basic block in the current function
    llvm::BlockAddress *blockAddr (llvm::BasicBlock *bb)
    {
        return llvm::BlockAddress::get (this->_curFn, bb);
    }

    /// set the current block to insert instructions in
    void setInsertPoint (llvm::BasicBlock *bb)
    {
        this->_builder.SetInsertPoint (bb);
    }

    /// get the current function
    llvm::Function *getCurFn () const { return this->_curFn; }

    /// get the current basic block
    llvm::BasicBlock *getCurBB ()
    {
        return this->_builder.GetInsertBlock ();
    }

    /// get the current value of the base pointer as an integer value
    llvm::Value *basePtr () const
    {
        assert ((this->_regState.getBasePtr() != nullptr)
            && "no base pointer for current cluster");
        return this->_regState.getBasePtr();
    }

    /// utility function for allocating a record of ML values (pointers or
    /// tagged ints).
    llvm::Value *allocRecord (llvm::Value *desc, Args_t const & args);

    /// utility function for allocating a record of ML values (pointers or
    /// tagged ints), where the descriptor is a known constant value.
    llvm::Value *allocRecord (uint64_t desc, Args_t const & args)
    {
        return allocRecord (this->asMLValue(this->uConst(desc)), args);
    }

    /// call the garbage collector.
    void callGC (Args_t const & roots, std::vector<LambdaVar::lvar> const & newRoots);

    /// return the basic-block that contains the Overflow trap generator
    llvm::BasicBlock *getOverflowBB ();

    /// return branch-weight meta data, where `prob` represents the probability of
    /// the true branch and is in the range 1..999.
    llvm::MDNode *branchProb (int prob);

    /// get the branch-weight meta data for overflow-trap branches
    llvm::MDNode *overflowWeights ();

    /// return an address in the stack with the given `ptrTy`.
    llvm::Value *stkAddr (llvm::Type *ptrTy, int offset)
    {
        if (this->_readReg == nullptr) {
            this->_initSPAccess();
        }

        return this->createIntToPtr (
            this->createAdd(
                this->_builder.CreateCall(
                    this->_readReg->getFunctionType(),
                    this->_readReg,
                    { llvm::MetadataAsValue::get(*this, this->_spRegMD) }),
                this->iConst(offset)),
            ptrTy);

    }

  // get intinsics; these are cached for the current module
    /// @{
    llvm::Function *sadd32WOvflw () const
    {
        if (this->_sadd32WO == nullptr) {
            this->_sadd32WO =
                _getIntrinsic (llvm::Intrinsic::sadd_with_overflow, this->i32Ty);
        }
        return this->_sadd32WO;
    }
    llvm::Function *ssub32WOvflw () const
    {
        if (this->_ssub32WO == nullptr) {
            this->_ssub32WO =
                _getIntrinsic (llvm::Intrinsic::ssub_with_overflow, this->i32Ty);
        }
        return this->_ssub32WO;
    }
    llvm::Function *smul32WOvflw () const
    {
        if (this->_smul32WO == nullptr) {
            this->_smul32WO =
                _getIntrinsic (llvm::Intrinsic::smul_with_overflow, this->i32Ty);
        }
        return this->_smul32WO;
    }
    llvm::Function *sadd64WOvflw () const
    {
        if (this->_sadd64WO == nullptr) {
            this->_sadd64WO =
                _getIntrinsic (llvm::Intrinsic::sadd_with_overflow, this->i64Ty);
        }
        return this->_sadd64WO;
    }
    llvm::Function *ssub64WOvflw () const
    {
        if (this->_ssub64WO == nullptr) {
            this->_ssub64WO =
                _getIntrinsic (llvm::Intrinsic::ssub_with_overflow, this->i64Ty);
        }
        return this->_ssub64WO;
    }
    llvm::Function *smul64WOvflw () const
    {
        if (this->_smul64WO == nullptr) {
            this->_smul64WO =
                _getIntrinsic (llvm::Intrinsic::smul_with_overflow, this->i64Ty);
        }
        return this->_smul64WO;
    }
    llvm::Function *fabs32 () const
    {
        if (this->_fabs32 == nullptr) {
            this->_fabs32 = _getIntrinsic (llvm::Intrinsic::fabs, this->f32Ty);
        }
        return this->_fabs32;
    }
    llvm::Function *fabs64 () const
    {
        if (this->_fabs64 == nullptr) {
            this->_fabs64 = _getIntrinsic (llvm::Intrinsic::fabs, this->f64Ty);
        }
        return this->_fabs64;
    }
    llvm::Function *sqrt32 () const
    {
        if (this->_sqrt32 == nullptr) {
            this->_sqrt32 = _getIntrinsic (llvm::Intrinsic::sqrt, this->f32Ty);
        }
        return this->_sqrt32;
    }
    llvm::Function *sqrt64 () const
    {
        if (this->_sqrt64 == nullptr) {
            this->_sqrt64 = _getIntrinsic (llvm::Intrinsic::sqrt, this->f64Ty);
        }
        return this->_sqrt64;
    }
    llvm::Function *copysign32 () const
    {
        if (this->_copysign32 == nullptr) {
            this->_copysign32 = _getIntrinsic (llvm::Intrinsic::copysign, this->f32Ty);
        }
        return this->_copysign32;
    }
    llvm::Function *copysign64 () const
    {
        if (this->_copysign64 == nullptr) {
            this->_copysign64 = _getIntrinsic (llvm::Intrinsic::copysign, this->f64Ty);
        }
        return this->_copysign64;
    }
    /// @}

  /***** shorthand for LLVM integer instructions (with argument coercions) *****/
/* FIXME: Note that for now, we assume that all arithmetic is in the native integer size! */
    /// @{
    llvm::Value *createAdd (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateAdd (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createAnd (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateAnd (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createAShr (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateAShr (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createICmp (llvm::CmpInst::Predicate cmp, llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateICmp (cmp, this->asInt(a), this->asInt(b));
    }
    llvm::Value *createICmpEQ (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateICmpEQ (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createICmpNE (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateICmpNE (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createICmpSLT (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateICmpSLT (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createLShr (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateLShr (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createMul (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateMul (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createOr (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateOr (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createSDiv (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateSDiv (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createShl (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateShl (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createSIToFP (llvm::Value *v, llvm::Type *ty)
    {
        return this->_builder.CreateSIToFP (v, ty);
    }
    llvm::Value *createSRem (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateSRem (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createSub (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateSub (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createUDiv (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateUDiv (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createURem (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateURem (this->asInt(a), this->asInt(b));
    }
    llvm::Value *createXor (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateXor (this->asInt(a), this->asInt(b));
    }

    llvm::Value *createSExt (llvm::Value *v, llvm::Type *ty)
    {
        return this->_builder.CreateSExt (v, ty);
    }
    llvm::Value *createZExt (llvm::Value *v, llvm::Type *ty)
    {
        return this->_builder.CreateZExt (v, ty);
    }
    llvm::Value *createTrunc (llvm::Value *v, llvm::Type *ty)
    {
        return this->_builder.CreateTrunc(v, ty);
    }
    /// @}

  /***** shorthand for LLVM floating-point instructions *****/
    /// @{
    llvm::Value *createFAdd (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateFAdd (a, b);
    }
    llvm::Value *createFCmp (llvm::CmpInst::Predicate cmp, llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateFCmp (cmp, a, b);
    }
    llvm::Value *createFDiv (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateFDiv (a, b);
    }
    llvm::Value *createFMul (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateFMul (a, b);
    }
    llvm::Value *createFNeg (llvm::Value *v)
    {
        return this->_builder.CreateFNeg (v);
    }
    llvm::Value *createFPToSI (llvm::Value *v, llvm::Type *ty)
    {
        return this->_builder.CreateFPToSI (v, ty);
    }
    llvm::Value *createFSub (llvm::Value *a, llvm::Value *b)
    {
        return this->_builder.CreateFSub (a, b);
    }
    /// @}

  /***** shorthand for load/store instructions *****/
    /// @{
    llvm::Value *createLoad (llvm::Type *ty, llvm::Value *adr, unsigned align)
    {
      // NOTE: our loads are always aligned to the ABI alignment requirement
        return this->_builder.CreateAlignedLoad (ty, adr, llvm::MaybeAlign(align));
    }
    llvm::Value *createLoad (llvm::Type *ty, llvm::Value *adr)
    {
      // NOTE: our loads are always aligned to the ABI alignment requirement
        return this->_builder.CreateAlignedLoad (ty, adr, llvm::MaybeAlign(0));
    }

    /// create a store of a ML value
    void createStoreML (llvm::Value *v, llvm::Value *adr)
    {
        this->_builder.CreateAlignedStore (
            this->asMLValue(v),
            this->asObjPtr(adr),
            llvm::MaybeAlign(this->_wordSzB));
    }
    void createStore (llvm::Value *v, llvm::Value *adr, unsigned align)
    {
        this->_builder.CreateAlignedStore (
            v,
            adr,
            llvm::MaybeAlign(align));
    }
    /// @}

  /***** shorthand for type cast instructions *****/
    /// @{
    llvm::Value *createIntToPtr (llvm::Value *v, llvm::Type *ty)
    {
        return this->_builder.CreateIntToPtr (v, ty);
    }
    llvm::Value *createPtrToInt (llvm::Value *v)
    {
        return this->_builder.CreatePtrToInt(v, this->intTy);
    }
    llvm::Value *createBitCast (llvm::Value *v, llvm::Type *ty)
    {
        return this->_builder.CreateBitCast (v, ty);
    }
    llvm::Value *createPointerCast (llvm::Value *v, llvm::Type *ty)
    {
        return this->_builder.CreatePointerCast (v, ty);
    }
    /// @}

  /***** shorthand for other instructions *****/

    /// create a tail JWA function call
    llvm::CallInst *createJWACall (llvm::FunctionType *fnTy, llvm::Value *fn, Args_t const &args)
    {
        llvm::CallInst *call = this->_builder.CreateCall(fnTy, fn, args);
        call->setCallingConv (llvm::CallingConv::JWA);
        call->setTailCallKind (llvm::CallInst::TCK_Tail);
        return call;
    }

    llvm::Value *createExtractValue (llvm::Value *v, int i)
    {
        return this->_builder.CreateExtractValue (v, i);
    }
    llvm::BranchInst *createBr (llvm::BasicBlock *bb)
    {
        return this->_builder.CreateBr (bb);
    }

    /// create a GEP instruction for accessing a ML value
    llvm::Value *createGEP (llvm::Value *base, llvm::Value *idx)
    {
        return this->_builder.CreateInBoundsGEP (base, { idx });
    }
    llvm::Value *createGEP (llvm::Type *ty, llvm::Value *base, llvm::Value *idx)
    {
        return this->_builder.CreateInBoundsGEP (
            this->createBitCast (base, ty),
            { idx });
    }

    /// create a GEP instruction for accessing a ML value using a constant index
    llvm::Value *createGEP (llvm::Value *base, int32_t idx)
    {
        return this->_builder.CreateInBoundsGEP (base, { this->i32Const(idx) });
    }

    /// create a GEP instruction for accessing a value of the specified type
    /// using a constant index
    llvm::Value *createGEP (llvm::Type *ty, llvm::Value *base, int32_t idx)
    {
        return this->_builder.CreateInBoundsGEP (
            this->createBitCast (base, ty),
            { this->i32Const(idx) });
    }

    /// create an unamed global alias
    llvm::Constant *createGlobalAlias (
        llvm::Type *ty,
        llvm::Twine const &name,
        llvm::Constant *v);

  /***** Code generation *****/

    /// access to the backing storage for generating in-memory object files
    ObjfilePWriteStream & objectFileOS () { return this->_objFileOS; }

    /// compile to an in-memory code object
    std::unique_ptr<CodeObject> compile ();

    /// dump assembly code to stdout
    void dumpAsm () const;

    /// dump assembly code to a file
    void dumpAsm (std::string const &stem) const;

    /// dump machine code to an object file
    void dumpObj (std::string const &stem) const;

  /***** Debugging support *****/

    /// dump the current module to stderr
    void dump () const;

    /// run the LLVM verifier on the module
    bool verify () const;

  private:
    struct TargetInfo const     *_target;
    llvm::IRBuilder<>           _builder;
    class MCGen                 *_gen;
    llvm::Module                *_module;       // current module
    llvm::Function              *_curFn;        // current LLVM function
    CFG::cluster                *_curCluster;   // current CFG cluster
    lvar_map_t<CFG::cluster>    _clusterMap;    // per-module mapping from labels to clusters
    lvar_map_t<CFG::frag>       _fragMap;       // pre-cluster map from labels to fragments
    lvar_map_t<llvm::Value>     _vMap;          // per-fragment map from lvars to values

    // more cached types (these are internal to the Context class)
    llvm::FunctionType *_gcFnTy;                // type of call-gc function
    llvm::FunctionType *_raiseOverflowFnTy;     // type of raise_overflow function

    // a basic block for the current cluster that will raise the Overflow exception
    llvm::BasicBlock            *_overflowBB;
    std::vector<llvm::PHINode *> _overflowPhiNodes;

    // tracking the state of the SML registers
    CMRegs               _regInfo;       // target-specific register info
    CMRegState                   _regState;      // current register values

    /// target-machine properties
    int64_t _wordSzB;

    // cached intrinsic functions
    mutable llvm::Function *_sadd32WO;          // @llvm.sadd.with.overflow.i32
    mutable llvm::Function *_ssub32WO;          // @llvm.ssub.with.overflow.i32
    mutable llvm::Function *_smul32WO;          // @llvm.smul.with.overflow.i32
    mutable llvm::Function *_sadd64WO;          // @llvm.sadd.with.overflow.i64
    mutable llvm::Function *_ssub64WO;          // @llvm.ssub.with.overflow.i64
    mutable llvm::Function *_smul64WO;          // @llvm.smul.with.overflow.i64
    mutable llvm::Function *_fabs32;            // @llvm.fabs.f32
    mutable llvm::Function *_fabs64;            // @llvm.fabs.f64
    mutable llvm::Function *_sqrt32;            // @llvm.sqrt.f32
    mutable llvm::Function *_sqrt64;            // @llvm.sqrt.f64
    mutable llvm::Function *_copysign32;        // @llvm.copysign.f32
    mutable llvm::Function *_copysign64;        // @llvm.copysign.f64

    /// cached @llvm.read_register + meta data to access stack
    llvm::Function *_readReg;
    llvm::MDNode *_spRegMD;

    /// helper function for getting an intrinsic when it has not yet
    /// been loaded for the current module.
    //
    llvm::Function *_getIntrinsic (llvm::Intrinsic::ID id, llvm::Type *ty) const;

    /// initialize the metadata needed to support reading the stack pointer
    void _initSPAccess ();

    /// utility function for loading a value from the stack
    llvm::Value *_loadFromStack (int offset, std::string const &name)
    {
        return this->build().CreateAlignedLoad (
            this->stkAddr (this->objPtrTy, offset),
            llvm::MaybeAlign (this->wordSzInBytes()),
            name);
    }

    /// function for loading a special register from memory
    llvm::Value *_loadMemReg (CMRegId r);

    /// function for setting a special memory register
    void _storeMemReg (CMRegId r, llvm::Value *v);

    /// information about JWA arguments
    struct arg_info {
        int nExtra;     ///< number of extra args for special CMachine registers
                        ///  that are mapped to machine registers
        int basePtr;    ///< == 1 if there is a base-pointer arg, 0 otherwise
        int nUnused;    ///< unused args (for STD_CONT convention)

        int numArgs (int n) { return n + this->nExtra + this->basePtr + this->nUnused; }

    };

    /// get information about JWA arguments for a fragment in the current cluster
    arg_info _getArgInfo (frag_kind kind) const;

    /// add the types for the "extra" parameters (plus optional base pointer) to
    /// a vector of types.
    void _addExtraParamTys (Types_t &tys, arg_info const &info) const;

    /// add the "extra" arguments (plus optional base pointer) to an argument vector
    void _addExtraArgs (Args_t &args, arg_info const &info) const;

    /// private constructor
    Context (struct TargetInfo const *target);

    //! backing storage for the generated object file.  We put this object it the
    //! code buffer so that we do not have to worry about its lifetime.
    ObjfilePWriteStream _objFileOS;
};

} // namespace cfgcg
} // namespace smlnj

#endif // !_CONTEXT_HPP_
