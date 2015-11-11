#ifndef MAGIC_MMAP_CTL_FUNCTION_H
#define MAGIC_MMAP_CTL_FUNCTION_H

#include <pass.h>
#include <magic/support/TypeInfo.h>

using namespace llvm;

namespace llvm {

class MagicMmapCtlFunction {
  public:
      MagicMmapCtlFunction(Function *function, PointerType *voidPointerType, std::string &ptrArgName, std::string &lenArgName);

      Function* getFunction() const;
      void fixCalls(Module &M, Function *magicGetPageSizeFunc) const;

      void print(raw_ostream &OS) const;
      void printDescription(raw_ostream &OS) const;
      const std::string getDescription() const;

  private:
      Function *function;
      int ptrArg;
      int lenArg;
};

inline raw_ostream &operator<<(raw_ostream &OS, const MagicMmapCtlFunction &aMagicMmapCtlFunction) {
    aMagicMmapCtlFunction.print(OS);
    return OS;
}

inline void MagicMmapCtlFunction::print(raw_ostream &OS) const {
     OS << getDescription();
}

inline void MagicMmapCtlFunction::printDescription(raw_ostream &OS) const {
    OS << "[ function = "; OS << function->getName() << "(" << TypeUtil::getDescription(function->getFunctionType()) << ")"
       << ", ptr arg = "; OS << ptrArg
       << ", len arg = "; OS << lenArg
       << "]";
}

inline const std::string MagicMmapCtlFunction::getDescription() const {
    std::string string;
    raw_string_ostream ostream(string);
    printDescription(ostream);
    ostream.flush();
    return string;
}

inline MagicMmapCtlFunction::MagicMmapCtlFunction(Function *function, PointerType *voidPointerType, std::string &ptrArgName, std::string &lenArgName) {
    this->function = function;
    this->ptrArg = -1;
    this->lenArg = -1;
    bool lookupPtrArg = ptrArgName.compare("");
    bool lookupLenArg = lenArgName.compare("");
    assert((lookupPtrArg || lookupLenArg) && "No valid argument name specified!");
    unsigned i=0;
    for (Function::arg_iterator it = function->arg_begin(), E = function->arg_end();
        it != E; ++it) {
        std::string argName = it->getName();
        if(lookupPtrArg && !argName.compare(ptrArgName)) {
            this->ptrArg = i;
        }
        else if(lookupLenArg && !argName.compare(lenArgName)) {
            this->lenArg = i;
        }
        i++;
    }
    if(this->ptrArg >= 0) {
        assert(function->getFunctionType()->getContainedType(this->ptrArg+1) == voidPointerType && "Invalid ptr argument specified!");
    }
    else {
        assert(!lookupPtrArg && "Invalid ptr argument name specified!");
    }
    if(this->lenArg >= 0) {
        assert(isa<IntegerType>(function->getFunctionType()->getContainedType(this->lenArg+1)) && "Invalid len argument specified!");
    }
    else {
        assert(!lookupLenArg && "Invalid len argument name specified!");
    }
}

inline Function* MagicMmapCtlFunction::getFunction() const {
    return function;
}

/* This assumes in-band metadata of 1 page before every mmapped region. */
inline void MagicMmapCtlFunction::fixCalls(Module &M, Function *magicGetPageSizeFunc) const {
    std::vector<User*> Users(function->user_begin(), function->user_end());
    while (!Users.empty()) {
        User *U = Users.back();
        Users.pop_back();

        if (Instruction *I = dyn_cast<Instruction>(U)) {
            Function *parent = I->getParent()->getParent();
            if(parent->getName().startswith("magic") || parent->getName().startswith("_magic")) {
                continue;
            }
            CallSite CS = MagicUtil::getCallSiteFromInstruction(I);

            std::vector<Value*> args;
            CallInst* magicGetPageSizeCall = MagicUtil::createCallInstruction(magicGetPageSizeFunc, args, "", I);
            magicGetPageSizeCall->setCallingConv(CallingConv::C);
            magicGetPageSizeCall->setTailCall(false);
            TYPECONST IntegerType *type = dyn_cast<IntegerType>(magicGetPageSizeCall->getType());
            assert(type);

            if(this->ptrArg >= 0) {
                Value *ptrValue = CS.getArgument(this->ptrArg);
                BinaryOperator* negativePageSize = BinaryOperator::Create(Instruction::Sub, ConstantInt::get(M.getContext(), APInt(type->getBitWidth(), 0)), magicGetPageSizeCall, "", I);
                GetElementPtrInst* ptrValueWithOffset = GetElementPtrInst::Create(ptrValue, negativePageSize, "", I);

                CS.setArgument(this->ptrArg, ptrValueWithOffset);
            }
            if(this->lenArg >= 0) {
                Value *lenValue = CS.getArgument(this->lenArg);
                BinaryOperator* lenValuePlusPageSize = BinaryOperator::Create(Instruction::Add, lenValue, magicGetPageSizeCall, "", I);

                CS.setArgument(this->lenArg, lenValuePlusPageSize);
            }
        }
    }
}

}

#endif

