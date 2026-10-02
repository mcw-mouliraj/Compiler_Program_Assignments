#ifndef LLVM_TRANSFORMS_UTILS_MOULICSE_H
#define LLVM_TRANSFORMS_UTILS_MOULICSE_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class MouliCSEPass : public OptionalPassInfoMixin<MouliCSEPass> {
public:
  LLVM_ABI PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif
