#ifndef LLVM_TRANSFORMS_UTILS_MOULISR_H
#define LLVM_TRANSFORMS_UTILS_MOULISR_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class MouliSRPass : public OptionalPassInfoMixin<MouliSRPass> {
public:
  LLVM_ABI PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif
