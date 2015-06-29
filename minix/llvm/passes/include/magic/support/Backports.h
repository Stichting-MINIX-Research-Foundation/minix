#ifndef BACKPORTS_H
#define BACKPORTS_H

#include <pass.h>

using namespace llvm;

namespace llvm {

class Backports {
  public:

      //From DbgInfoPrinter.cpp (LLVM 2.9)
      static Value *findDbgGlobalDeclare(GlobalVariable *V);
      static Value *findDbgSubprogramDeclare(Function *V);
      static const DbgDeclareInst *findDbgDeclare(const Value *V);

      //From Local.cpp (LLVM 2.9)
      static DbgDeclareInst *FindAllocaDbgDeclare(Value *V);
};

}

#endif
