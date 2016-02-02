#ifndef VARIABLE_REFS_H
#define VARIABLE_REFS_H

#include <pass.h>

using namespace llvm;

namespace llvm {

class VariableRefs {
  public:
      VariableRefs();

      bool isUnnecessaryInstruction(Instruction* inst) const;
      Instruction* getInstruction() const;
      bool isInstructionInEntryBlock() const;

      void addInstruction(Instruction* inst);
      void clear();

  private:
      Instruction* instruction;
      bool instructionInEntryBlock;

      void setFunctionEntryInstruction(Function* function);
};

}

#endif
