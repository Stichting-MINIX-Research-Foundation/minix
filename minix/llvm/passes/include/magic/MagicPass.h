#ifndef MAGIC_PASS_H

#define MAGIC_PASS_H

#include <pass.h>
#include <magic/magic.h>
#include <magic/support/MagicUtil.h>
#include <magic/support/SmartType.h>
#include <magic/support/TypeInfo.h>
#include <magic/support/MagicDebugFunction.h>
#include <magic/support/MagicMemFunction.h>
#include <magic/support/MagicMmapCtlFunction.h>

#if MAGIC_USE_QPROF_INSTRUMENTATION
#include <common/qprof_common.h>
#endif

using namespace llvm;

namespace llvm {

#define magicPassLog(M) DEBUG(dbgs() << "MagicPass: " << M << "\n")
#define magicPassErr(M) errs() << "MagicPass: " << M << "\n"

class MagicPass : public ModulePass {

  public:
      static char ID;

      MagicPass();

      std::vector<GlobalVariable*> getGlobalVariables() const;
      std::vector<int> getGlobalVariableSizes() const;
      std::vector<GlobalVariable*> getShadowGlobalVariables() const;
      std::vector<Function*> getFunctions() const;
      GlobalVariable* getMagicArray() const;
      GlobalVariable* getMagicTypeArray() const;
      GlobalVariable* getMagicFunctionArray() const;
      GlobalVariable* getMagicDsindexArray() const;

      virtual bool runOnModule(Module &M);

  private:
      std::vector<GlobalVariable*> globalVariables;
      std::set<GlobalVariable*> globalVariablesWithAddressTaken;
      std::vector<int> globalVariableSizes;
      std::vector<GlobalVariable*> shadowGlobalVariables;
      std::vector<Function*> functions;
      std::vector<TypeInfo*> globalTypeInfos;
      std::map<GlobalValue*, TypeInfo*> globalParentMap;
      std::map<GlobalValue*, TypeInfo*>::iterator parentMapIt;
      std::map<std::string, GlobalVariable*> stringOwnerMap;
      std::map<std::string, GlobalVariable*>::iterator stringOwnerMapIt;
      std::map<GlobalVariable*, std::string> stringOwnerInvertedMap;
      std::map<GlobalVariable*, std::string>::iterator stringOwnerInvertedMapIt;

      GlobalVariable* magicArray;
      GlobalVariable* magicTypeArray;
      GlobalVariable* magicFunctionArray;
      GlobalVariable* magicDsindexArray;

      std::vector<std::string> libPathRegexes;
      std::vector<std::string> voidTypeAliases;
      std::set<std::string> voidTypeAliasesSet;
      std::vector<std::string> mmFuncPrefixes;
      std::set<std::pair<std::string, std::string> > mmFuncPairs;
      std::vector<std::string> mmPoolFunctions;
      std::vector<std::string> mmapCtlFunctions;
      std::set<std::string>::iterator stringSetIt;
      std::set<Function*> brkFunctions;
      std::set<Function*> sbrkFunctions;
      std::vector<Regex*> magicDataSectionRegexes;
      std::vector<Regex*> magicFunctionSectionRegexes;
      std::vector<Regex*> extLibSectionRegexes;

#if MAGIC_USE_QPROF_INSTRUMENTATION
      QProfConf *qprofConf;
#endif

      void qprofInstrumentationInit(Module &M);
      void qprofInstrumentationApply(Module &M);
      bool checkPointerVariableIndexes(TYPECONST Type* type, std::vector<int> &ptrVarIndexes, unsigned offset=0);
      void findPointerVariables(Function* function, Value *value, std::vector<Value*> &ptrVars, std::vector<std::vector<int> > &ptrVarIndexes, Value *parent = NULL, bool isUser=false);
      TypeInfo* typeInfoFromPointerVariables(Module &M, TypeInfo *voidPtrTypeInfo, std::vector<Value*> &ptrVars, std::vector<std::vector<int> > &ptrVarIndexes, std::string &allocName);
      TypeInfo* getAllocTypeInfo(Module &M, TypeInfo *voidPtrTypeInfo, const CallSite &CS, std::string &allocName, std::string &allocParentName);
      TypeInfo* fillTypeInfos(TypeInfo &sourceTypeInfo, std::vector<TypeInfo*> &typeInfos);
      TypeInfo* fillExternalTypeInfos(TYPECONST Type* sourceType, GlobalValue *parent, std::vector<TypeInfo*> &typeInfos);
      void printInterestingTypes(TYPECONST TypeInfo *aTypeInfo);
      unsigned getMaxRecursiveSequenceLength(TYPECONST TypeInfo *aTypeInfo);
      FunctionType* getFunctionType(TYPECONST FunctionType *baseType, std::vector<unsigned> selectedArgs);
      bool isCompatibleMagicMemFuncType(TYPECONST FunctionType *type, TYPECONST FunctionType* magicType);
      Function* findWrapper(Module &M, std::string *magicMemPrefixes, Function *f, std::string fName);

      void indexCasts(Module &M, User *U, std::vector<TYPECONST Type*> &intCastTypes, std::vector<int> &intCastValues, std::map<TYPECONST Type*, std::set<TYPECONST Type*> > &bitcastMap);

      void fillStackInstrumentedFunctions(std::vector<Function*> &stackIntrumentedFuncs, Function *deepestLLFunction);
      void indexLocalTypeInfos(Module &M, Function *F, std::map<AllocaInst*, std::pair<TypeInfo*, std::string> > &localMap);
      void addMagicStackDsentryFuncCalls(Module &M, Function *insertCallsInFunc, Function *localsFromFunc, Function *dsentryCreateFunc, Function *dsentryDestroyFunc, TYPECONST StructType *dsentryStructType, std::map<AllocaInst*, std::pair<TypeInfo*, std::string> > localTypeInfoMap, std::map<TypeInfo*, Constant*> &magicArrayTypePtrMap, TypeInfo *voidPtrTypeInfo, std::vector<TypeInfo*> &typeInfoList, std::vector<std::pair<std::string, std::string> > &namesList, std::vector<int> &flagsList);
      bool isExtLibrary(GlobalValue *GV, DIDescriptor *DID);
      bool isMagicGV(Module &M, GlobalVariable *GV);
      bool isMagicFunction(Module &M, Function *F);
};

inline std::vector<GlobalVariable*> MagicPass::getGlobalVariables() const {
    return globalVariables;
}

inline std::vector<int> MagicPass::getGlobalVariableSizes() const {
    return globalVariableSizes;
}

inline std::vector<GlobalVariable*> MagicPass::getShadowGlobalVariables() const {
    return shadowGlobalVariables;
}

inline std::vector<Function*> MagicPass::getFunctions() const {
    return functions;
}

inline GlobalVariable* MagicPass::getMagicArray() const {
    return magicArray;
}

inline GlobalVariable* MagicPass::getMagicTypeArray() const {
    return magicTypeArray;
}

inline GlobalVariable* MagicPass::getMagicFunctionArray() const {
    return magicFunctionArray;
}

inline GlobalVariable* MagicPass::getMagicDsindexArray() const {
    return magicDsindexArray;
}

}

#endif
