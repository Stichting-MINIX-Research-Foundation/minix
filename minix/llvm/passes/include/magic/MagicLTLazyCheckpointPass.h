#ifndef MAGIC_LTLAZY_CHECKPOINT_PASS_H
#define MAGIC_LTLAZY_CHECKPOINT_PASS_H

#include <magic/magic.h>
#include <magic/MagicPass.h>

using namespace llvm;

namespace llvm {

#define magicLTLazyCheckpointPassLog(M) DEBUG(dbgs() << "MagicLTLazyCheckpointPass: " << M << "\n")

class MagicLTLazyCheckpointPass : public FunctionPass {

  public:
      static char ID;

      MagicLTLazyCheckpointPass();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnFunction(Function &F);

  private:
      MagicPass *MP;
};

}

#endif
