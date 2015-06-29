#ifndef MAGIC_CTLAZY_CHECKPOINT_PASS_H
#define MAGIC_CTLAZY_CHECKPOINT_PASS_H

#include <magic/magic.h>
#include <magic/MagicPass.h>
#include <magic/support/VariableRefs.h>

using namespace llvm;

namespace llvm {

#define magicCTLazyCheckpointPassLog(M) DEBUG(dbgs() << "MagicCTLazyCheckpointPass: " << M << "\n")

class MagicCTLazyCheckpointPass : public FunctionPass {

  public:
      static char ID;

      MagicCTLazyCheckpointPass();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnFunction(Function &F);

  private:
      AliasAnalysis *AA;

      bool instructionModifiesVar(Module &M, Instruction *inst, GlobalVariable* var);
};

}

#endif
