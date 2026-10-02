#include "llvm/Transforms/Utils/MouliCSE.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// hash -> instructions with that hash; isIdenticalTo does the real check
using ExprTable = DenseMap<hash_code, SmallVector<Instruction *, 4>>;

// skip loads, calls, allocas, phis, terminators - unsafe or pointless to dedupe
bool isCSECandidate(Instruction &I) {
  if (isa<PHINode>(I) || isa<AllocaInst>(I) || I.isTerminator())
    return false;
  return !I.mayReadFromMemory() && !I.mayHaveSideEffects();
}

hash_code hashInstr(Instruction *I) {
  return hash_combine(I->getOpcode(), I->getType(),
                       hash_combine_range(I->value_op_begin(),
                                           I->value_op_end()));
}

// scoped hash table over the dominator tree: add on the way down,
// remove on the way back up
void visit(DomTreeNode *Node, ExprTable &Table, bool &Modified) {
  SmallVector<std::pair<hash_code, Instruction *>, 8> AddedHere;

  for (Instruction &Inst : make_early_inc_range(*Node->getBlock())) {
    if (!isCSECandidate(Inst))
      continue;

    hash_code H = hashInstr(&Inst);
    SmallVectorImpl<Instruction *> &Bucket = Table[H];

    Instruction *Match = nullptr;
    for (Instruction *Cand : Bucket) {
      if (Cand->isIdenticalTo(&Inst)) {
        Match = Cand;
        break;
      }
    }

    if (Match) {
      errs() << "mouli-cse: " << Inst << "   ->   reuses   " << *Match
             << "\n";
      Inst.replaceAllUsesWith(Match);
      Inst.eraseFromParent();
      Modified = true;
      continue;
    }

    Bucket.push_back(&Inst);
    AddedHere.emplace_back(H, &Inst);
  }

  for (DomTreeNode *Child : *Node)
    visit(Child, Table, Modified);

  // leaving the subtree, so these go out of scope again
  for (auto &[H, I] : AddedHere)
    llvm::erase(Table[H], I);
}

} // namespace

PreservedAnalyses MouliCSEPass::run(Function &F, FunctionAnalysisManager &AM) {
  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

  ExprTable Table;
  bool Modified = false;
  visit(DT.getRootNode(), Table, Modified);

  if (!Modified)
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  PA.preserve<DominatorTreeAnalysis>();
  return PA;
}
