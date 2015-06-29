#include <pass.h>

#include <magic/support/Backports.h>
#include <magic/support/EDIType.h>
#include <magic/support/SmartType.h>

using namespace llvm;

namespace llvm {

//===----------------------------------------------------------------------===//
// Public static methods
//===----------------------------------------------------------------------===//

/// Find the debug info descriptor corresponding to this global variable.
Value *Backports::findDbgGlobalDeclare(GlobalVariable *V) {
   return PassUtil::findDbgGlobalDeclare(V);
}

/// Find the debug info descriptor corresponding to this function.
Value *Backports::findDbgSubprogramDeclare(Function *V) {
   return PassUtil::findDbgSubprogramDeclare(V);
}

/// Finds the llvm.dbg.declare intrinsic corresponding to this value if any.
/// It looks through pointer casts too.
const DbgDeclareInst *Backports::findDbgDeclare(const Value *V) {
  V = V->stripPointerCasts();

  if (!isa<Instruction>(V) && !isa<Argument>(V))
    return 0;

  const Function *F = NULL;
  if (const Instruction *I = dyn_cast<Instruction>(V))
    F = I->getParent()->getParent();
  else if (const Argument *A = dyn_cast<Argument>(V))
    F = A->getParent();

  for (Function::const_iterator FI = F->begin(), FE = F->end(); FI != FE; ++FI)
    for (BasicBlock::const_iterator BI = (*FI).begin(), BE = (*FI).end();
         BI != BE; ++BI)
      if (const DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(BI))
        if (DDI->getAddress() == V)
          return DDI;

  return 0;
}

/// FindAllocaDbgDeclare - Finds the llvm.dbg.declare intrinsic describing the
/// alloca 'V', if any.
DbgDeclareInst *Backports::FindAllocaDbgDeclare(Value *V) {
#if LLVM_VERSION >= 33
  if (MDNode *DebugNode = MDNode::getIfExists(V->getContext(), V))
    for (Value::use_iterator UI = DebugNode->use_begin(),
         E = DebugNode->use_end(); UI != E; ++UI)
      if (DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(*UI))
        return DDI;

  return 0;
#else

///
/// There is an unfixed bug (http://llvm.org/bugs/show_bug.cgi?id=10887)
/// that drops debug information references for local variables at linking time.
/// This is way the method FindAllocaDbgDeclare() will always return NULL and
/// we need to resort to some ugly workaround.
///
    AllocaInst *AI = dyn_cast<AllocaInst>(V);
    if(!AI) {
        return NULL;
    }
    BasicBlock *varParent = AI->getParent();
    Function *varFunc = varParent->getParent();
    if(!AI->hasName() || !AI->getName().compare("retval")) {
        return NULL;
    }
    StringRef varName = AI->getName();
    bool isInlinedVar = false;
    bool isScopedVar = false;
    DIType aDIType, aAddrDIType, tmpDIType;
    DbgDeclareInst *DDI = NULL, *addrDDI = NULL;
    SmallVector< StringRef, 8 > vector;
    varName.split(vector, ".");
    if(vector.size() >= 2 && vector[1][0] == 'i') {
        isInlinedVar = true;
        varName = vector[0];
    }
    if(!isInlinedVar) {
        //for non-inlined variables we have to look in the first bb first.
        for(BasicBlock::iterator i=varParent->begin(), e=varParent->end();i!=e;i++) {
            if(DbgDeclareInst *DI = dyn_cast<DbgDeclareInst>(i)) {
                MDNode *node = DI->getVariable();
                assert(node);
                DIVariable DIV(node);
                if(!DIV.getName().compare(varName)) {
                    aDIType = DIV.getType();
                    DDI = DI;
                    break;
                }
                StringRef addrVarName(DIV.getName().str() + "_addr");
                if(!addrVarName.compare(varName)) {
                    aAddrDIType = DIV.getType();
                    addrDDI = DI;
                    break;
                }
            }
        }
        if(!DDI && !addrDDI) {
            //not found? probably a scoped variable, look anywhere else.
            isScopedVar = true;
        }
    }
    if(isInlinedVar || isScopedVar) {
        //for inlined/scoped variables we have to look everywhere in the function and name clashing could also occur.
        bool isDuplicate = false;
        for (inst_iterator it = inst_begin(varFunc), et = inst_end(varFunc); it != et; ++it) {
            Instruction *inst = &(*it);
            if(DbgDeclareInst *DI = dyn_cast<DbgDeclareInst>(inst)) {
                MDNode *node = DI->getVariable();
                assert(node);
                DIVariable DIV(node);
                StringRef addrVarName(DIV.getName().str() + "_addr");
                if(!DIV.getName().compare(varName) || !addrVarName.compare(varName)) {
                    tmpDIType = DIV.getType();
                    EDIType tmpEDIType(tmpDIType);
                    if(!SmartType::isTypeConsistent(AI->getAllocatedType(), &tmpEDIType)) {
                        continue;
                    }
                    bool skipDIType = false;
                    if(DDI) {
                        EDIType aEDIType(aDIType);
                        skipDIType = true;
                        if(!aEDIType.equals(&tmpEDIType)) {
                            isDuplicate = true;
                        }
                    }
                    if(addrDDI) {
                        EDIType aAddrEDIType(aAddrDIType);
                        skipDIType = true;
                        if(!aAddrEDIType.equals(&tmpEDIType)) {
                            isDuplicate = true;
                        }
                    }
                    if(!skipDIType && !DIV.getName().compare(varName)) {
                        aDIType = tmpDIType;
                        DDI = DI;
                        continue;
                    }
                    if(!skipDIType && !addrVarName.compare(varName)) {
                        aAddrDIType = tmpDIType;
                        addrDDI = DI;
                        continue;
                    }
                }
            }
        }
        if(isDuplicate) {
            //name clashing problem with inline/scoped variables, pretend nothing was found
            DDI = NULL;
            addrDDI = NULL;
        }
    }
    if(!DDI && !addrDDI) {
        return (DbgDeclareInst*)-1;
    }
    assert((DDI && !addrDDI) || (!DDI && addrDDI));
    DDI = DDI ? DDI : addrDDI;
    return DDI;
#endif
}

}
