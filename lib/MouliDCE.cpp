#include "llvm/Transforms/Utils/MouliDCE.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// no uses, not a terminator, no side effects -> safe to remove
bool isDead(Instruction &I) {
  if (I.isTerminator())
    return false;
  if (!I.use_empty())
    return false;
  if (I.mayHaveSideEffects())
    return false;
  return true;
}

} // namespace

PreservedAnalyses MouliDCEPass::run(Function &F, FunctionAnalysisManager &AM) {
  bool Modified = false;

  // removing one dead inst can make its operands dead too, so loop
  bool Changed = true;
  while (Changed) {
    Changed = false;

    for (BasicBlock &BB : F) {
      for (Instruction &I : make_early_inc_range(BB)) {
        if (!isDead(I))
          continue;

        errs() << "mouli-dce: removing dead instruction: " << I << "\n";
        I.eraseFromParent();
        Changed = Modified = true;
      }
    }
  }

  return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
