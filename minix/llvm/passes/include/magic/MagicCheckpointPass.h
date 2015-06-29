#ifndef MAGIC_CHECKPOINT_PASS_H
#define MAGIC_CHECKPOINT_PASS_H

#include <magic/magic.h>
#include <magic/MagicPass.h>

using namespace llvm;

namespace llvm {

#define magicCheckpointPassLog(M) DEBUG(dbgs() << "MagicCheckpointPass: " << M << "\n")

class MagicCheckpointPass : public ModulePass {

  public:
      static char ID;

      MagicCheckpointPass();

      virtual void getAnalysisUsage(AnalysisUsage &AU) const;
      virtual bool runOnModule(Module &M);

  private:
      MagicPass *MP;
};

}

#endif
