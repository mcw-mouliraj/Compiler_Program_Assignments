#ifndef LLVM_TRANSFORMS_UTILS_MOULICFSCCP_H
#define LLVM_TRANSFORMS_UTILS_MOULICFSCCP_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class MouliCFSCCPPass : public OptionalPassInfoMixin<MouliCFSCCPPass> {
public:
  LLVM_ABI PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif
