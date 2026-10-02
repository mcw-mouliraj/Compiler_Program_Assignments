#include "llvm/Transforms/Utils/MouliCFSCCP.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// literal constant, or something we already proved constant
Constant *getConst(Value *V, DenseMap<Value *, Constant *> &Known) {
  if (auto *C = dyn_cast<Constant>(V))
    return C;
  auto It = Known.find(V);
  if (It == Known.end())
    return nullptr;
  return It->second;
}

// plain integer math, done by hand. floats/vectors/pointers not handled
Constant *foldBinOp(Instruction::BinaryOps Op, ConstantInt *L,
                     ConstantInt *R) {
  const APInt &A = L->getValue();
  const APInt &B = R->getValue();
  Type *Ty = L->getType();

  switch (Op) {
  case Instruction::Add:  return ConstantInt::get(Ty, A + B);
  case Instruction::Sub:  return ConstantInt::get(Ty, A - B);
  case Instruction::Mul:  return ConstantInt::get(Ty, A * B);
  case Instruction::And:  return ConstantInt::get(Ty, A & B);
  case Instruction::Or:   return ConstantInt::get(Ty, A | B);
  case Instruction::Xor:  return ConstantInt::get(Ty, A ^ B);
  case Instruction::Shl:  return ConstantInt::get(Ty, A.shl(B));
  case Instruction::LShr: return ConstantInt::get(Ty, A.lshr(B));
  case Instruction::AShr: return ConstantInt::get(Ty, A.ashr(B));
  case Instruction::SDiv:
    return B.isZero() ? nullptr : ConstantInt::get(Ty, A.sdiv(B));
  case Instruction::UDiv:
    return B.isZero() ? nullptr : ConstantInt::get(Ty, A.udiv(B));
  case Instruction::SRem:
    return B.isZero() ? nullptr : ConstantInt::get(Ty, A.srem(B));
  case Instruction::URem:
    return B.isZero() ? nullptr : ConstantInt::get(Ty, A.urem(B));
  default:
    return nullptr;
  }
}

Constant *foldICmp(ICmpInst::Predicate Pred, ConstantInt *L, ConstantInt *R) {
  const APInt &A = L->getValue();
  const APInt &B = R->getValue();
  bool Result;

  switch (Pred) {
  case ICmpInst::ICMP_EQ:  Result = A == B; break;
  case ICmpInst::ICMP_NE:  Result = A != B; break;
  case ICmpInst::ICMP_SLT: Result = A.slt(B); break;
  case ICmpInst::ICMP_SLE: Result = A.sle(B); break;
  case ICmpInst::ICMP_SGT: Result = A.sgt(B); break;
  case ICmpInst::ICMP_SGE: Result = A.sge(B); break;
  case ICmpInst::ICMP_ULT: Result = A.ult(B); break;
  case ICmpInst::ICMP_ULE: Result = A.ule(B); break;
  case ICmpInst::ICMP_UGT: Result = A.ugt(B); break;
  case ICmpInst::ICMP_UGE: Result = A.uge(B); break;
  default:
    return nullptr;
  }
  return ConstantInt::get(Type::getInt1Ty(L->getContext()), Result);
}

Constant *tryFold(Instruction *I, DenseMap<Value *, Constant *> &Known) {
  if (auto *Phi = dyn_cast<PHINode>(I)) {
    // constant only if all incoming values agree
    Constant *Agreed = nullptr;
    for (unsigned i = 0, e = Phi->getNumIncomingValues(); i != e; ++i) {
      Constant *C = getConst(Phi->getIncomingValue(i), Known);
      if (!C)
        return nullptr;
      if (!Agreed)
        Agreed = C;
      else if (Agreed != C)
        return nullptr;
    }
    return Agreed;
  }

  if (auto *BO = dyn_cast<BinaryOperator>(I)) {
    auto *LHS = dyn_cast_or_null<ConstantInt>(getConst(BO->getOperand(0), Known));
    auto *RHS = dyn_cast_or_null<ConstantInt>(getConst(BO->getOperand(1), Known));
    if (!LHS || !RHS)
      return nullptr;
    return foldBinOp(BO->getOpcode(), LHS, RHS);
  }

  if (auto *Cmp = dyn_cast<ICmpInst>(I)) {
    auto *LHS = dyn_cast_or_null<ConstantInt>(getConst(Cmp->getOperand(0), Known));
    auto *RHS = dyn_cast_or_null<ConstantInt>(getConst(Cmp->getOperand(1), Known));
    if (!LHS || !RHS)
      return nullptr;
    return foldICmp(Cmp->getPredicate(), LHS, RHS);
  }

  // calls, loads etc - can't say
  return nullptr;
}

} // namespace

PreservedAnalyses MouliCFSCCPPass::run(Function &F,
                                        FunctionAnalysisManager &AM) {
  DenseMap<Value *, Constant *> Known;

  SmallVector<Instruction *, 32> Insts;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      Insts.push_back(&I);

  // loop till a full pass finds nothing new
  bool Changed = true;
  while (Changed) {
    Changed = false;
    for (Instruction *I : Insts) {
      if (Known.count(I))
        continue;
      if (Constant *C = tryFold(I, Known)) {
        Known[I] = C;
        Changed = true;
      }
    }
  }

  if (!Known.empty())
    errs() << "mouli-cf-sccp: proved " << Known.size()
           << " instruction(s) constant in @" << F.getName() << "\n";

  bool Modified = false;

  // replace the constant instructions with their values
  SmallVector<Instruction *, 16> Dead;
  for (auto &Entry : Known) {
    Instruction *I = cast<Instruction>(Entry.first);
    errs() << "  " << *I << "   ->   " << *Entry.second << "\n";
    I->replaceAllUsesWith(Entry.second);
    Dead.push_back(I);
  }
  for (Instruction *I : Dead)
    I->eraseFromParent();
  Modified |= !Dead.empty();

  // known branch condition -> just jump to that side
  for (BasicBlock &BB : F) {
    auto *Br = dyn_cast_or_null<CondBrInst>(BB.getTerminator());
    if (!Br)
      continue;

    auto *Cond = dyn_cast<ConstantInt>(Br->getCondition());
    if (!Cond)
      continue;

    BasicBlock *Target = Cond->isOne() ? Br->getSuccessor(0)
                                        : Br->getSuccessor(1);
    errs() << "  branch in %" << BB.getName() << " folds to %"
           << Target->getName() << "\n";

    IRBuilder<> Builder(Br);
    Builder.CreateBr(Target);
    Br->eraseFromParent();
    Modified = true;
  }

  // remove blocks not reachable from entry anymore
  SmallPtrSet<BasicBlock *, 16> Live;
  SmallVector<BasicBlock *, 16> Worklist{&F.getEntryBlock()};
  Live.insert(&F.getEntryBlock());
  while (!Worklist.empty()) {
    BasicBlock *BB = Worklist.pop_back_val();
    for (BasicBlock *Succ : successors(BB))
      if (Live.insert(Succ).second)
        Worklist.push_back(Succ);
  }

  SmallVector<BasicBlock *, 8> Unreachable;
  for (BasicBlock &BB : F)
    if (!Live.count(&BB))
      Unreachable.push_back(&BB);

  for (BasicBlock *BB : Unreachable) {
    errs() << "  removing unreachable block %" << BB->getName() << "\n";
    for (BasicBlock *Succ : successors(BB)) {
      if (!Live.count(Succ))
        continue; // also dead, gets erased below
      for (PHINode &Phi : Succ->phis())
        Phi.removeIncomingValue(BB, false);
    }
  }
  for (BasicBlock *BB : Unreachable)
    BB->eraseFromParent();
  Modified |= !Unreachable.empty();

  // phi with only one input left is just that input
  bool ChangedPhi = true;
  while (ChangedPhi) {
    ChangedPhi = false;
    for (BasicBlock &BB : F) {
      for (PHINode &Phi : make_early_inc_range(BB.phis())) {
        if (Phi.getNumIncomingValues() != 1)
          continue;
        Value *V = Phi.getIncomingValue(0);
        Phi.replaceAllUsesWith(V);
        Phi.eraseFromParent();
        ChangedPhi = Modified = true;
      }
    }
  }

  return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
