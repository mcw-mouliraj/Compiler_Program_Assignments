#ifndef LLVM_TRANSFORMS_UTILS_MOULIDCE_H
#define LLVM_TRANSFORMS_UTILS_MOULIDCE_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class MouliDCEPass : public OptionalPassInfoMixin<MouliDCEPass> {
public:
  LLVM_ABI PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif
