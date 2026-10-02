#include "llvm/Transforms/Utils/MouliSR.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include <cmath>

using namespace llvm;

// Returns log2(n) if n is a positive power of two, else -1.
static int getPowerOfTwoExponent(uint64_t n) {
  if (n == 0 || (n & (n - 1)) != 0)
    return -1; // not a power of two
  return static_cast<int>(std::log2(static_cast<double>(n)));
}

PreservedAnalyses MouliSRPass::run(Function &F, FunctionAnalysisManager &AM) {
  bool Changed = false;

  // don't erase while iterating, collect first
  SmallVector<Instruction *, 8> ToErase;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      auto *Mul = dyn_cast<BinaryOperator>(&I);
      if (!Mul || Mul->getOpcode() != Instruction::Mul)
        continue;

      Value *Op0 = Mul->getOperand(0);
      Value *Op1 = Mul->getOperand(1);

      // one side has to be a constant power of two
      ConstantInt *C = dyn_cast<ConstantInt>(Op1);
      Value *Other = Op0;
      if (!C) {
        C = dyn_cast<ConstantInt>(Op0);
        Other = Op1;
      }
      if (!C)
        continue; // neither side constant, e.g. x*x

      int logb2 = getPowerOfTwoExponent(C->getZExtValue());
      if (logb2 <= 0)
        continue; // not power of two, or just 1

      errs() << "mouli-sr: rewriting " << *Mul
             << "  ->  shl by " << logb2 << "\n";

      IRBuilder<> Builder(Mul);
      Value *Shift = Builder.CreateShl(
          Other, ConstantInt::get(C->getType(), logb2), "sr");

      Mul->replaceAllUsesWith(Shift);
      ToErase.push_back(Mul);
      Changed = true;
    }
  }

  for (Instruction *I : ToErase)
    I->eraseFromParent();

  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
