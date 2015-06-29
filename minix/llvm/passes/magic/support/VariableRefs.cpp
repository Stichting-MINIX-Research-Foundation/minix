#include <magic/support/VariableRefs.h>

using namespace llvm;

namespace llvm {

//===----------------------------------------------------------------------===//
// Constructors, destructor, and operators
//===----------------------------------------------------------------------===//

VariableRefs::VariableRefs() {
    clear();
}

//===----------------------------------------------------------------------===//
// Getters
//===----------------------------------------------------------------------===//

bool VariableRefs::isUnnecessaryInstruction(Instruction* inst) const {
    //have already instruction in the entry block, skip
    if(instructionInEntryBlock) {
        return true;
    }
    //have already instruction in the same block, skip
    if(instruction && inst->getParent() == instruction->getParent()) {
        return true;
    }

    return false;
}

Instruction* VariableRefs::getInstruction() const {
    return instruction;
}

bool VariableRefs::isInstructionInEntryBlock() const {
    return instructionInEntryBlock;
}

//===----------------------------------------------------------------------===//
// Other public methods
//===----------------------------------------------------------------------===//

void VariableRefs::addInstruction(Instruction* inst) {
    //no instruction yet, update instruction
    if(!instruction) {
        instruction = inst;
        return;
    }
    //have already instruction in another block, give up and resort to a single instruction in the entry block
    setFunctionEntryInstruction(inst->getParent()->getParent());
}

void VariableRefs::clear() {
    instruction = NULL;
    instructionInEntryBlock = false;
}

//===----------------------------------------------------------------------===//
// Private methods
//===----------------------------------------------------------------------===//

void VariableRefs::setFunctionEntryInstruction(Function* function) {
    this->instruction = function->front().getFirstNonPHI();
    this->instructionInEntryBlock = true;
}

}
