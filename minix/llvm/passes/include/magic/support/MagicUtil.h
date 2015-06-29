#ifndef MAGIC_UTIL_H
#define MAGIC_UTIL_H

#include <magic/magic.h>
#include <magic/support/SmartType.h>
#include <cxxabi.h>

using namespace llvm;

namespace llvm {

#define magicUtilLog(M) DEBUG(dbgs() << "MagicUtil: " << M << "\n")

class MagicUtil {
  public:
      static StringRef getGVSourceName(Module &M, GlobalVariable *GV, DIGlobalVariable **DIGVP=NULL, const std::string &baseDir="");
      static StringRef getLVSourceName(Module &M, AllocaInst *V, DIVariable **DIVP=NULL);
      static StringRef getFunctionSourceName(Module &M, Function *F, DISubprogram **DISP=NULL, const std::string &baseDir="");
      static void putStringRefCache(Module &M, const std::string &str, GlobalVariable *GV);
      static Constant* getGetElementPtrConstant(Constant *constant, std::vector<Value*> &indexes);
      static GetElementPtrInst* createGetElementPtrInstruction(Value *ptr, std::vector<Value*> &indexes, const Twine &NameStr="", Instruction *InsertBefore=0);
      static GetElementPtrInst* createGetElementPtrInstruction(Value *ptr, std::vector<Value*> &indexes, const Twine &NameStr="", BasicBlock *InsertAtEnd=0);
      static CallInst* createCallInstruction(Value *F, std::vector<Value*> &args, const Twine &NameStr="", Instruction *InsertBefore=0);
      static CallInst* createCallInstruction(Value *F, std::vector<Value*> &args, const Twine &NameStr="", BasicBlock *InsertAtEnd=0);
      static Function* getIntrinsicFunction(Module &M, Intrinsic::ID id, TYPECONST Type** types=NULL, unsigned size=0);
      static GlobalVariable *getStringRef(Module &M, const std::string &str);
      static GlobalVariable *getIntArrayRef(Module &M, unsigned arrSize, std::vector<int> *arr, bool isConstant=true);
      static GlobalVariable *getStringArrayRef(Module &M, unsigned arrSize, std::vector<std::string> *arr, bool isConstant=true);
      static GlobalVariable *getGenericArrayRef(Module &M, std::vector<Constant*> &arrayElems, bool isConstant=true);
      static GlobalVariable *getMagicTypePtrArrayRef(Module &M, Instruction *InsertBefore, std::vector<Value*> &globalTypeIndexes, GlobalVariable *magicTypeArray);
      static GlobalVariable* getExportedIntGlobalVar(Module &M, std::string name, int value, bool isConstant=true);
      static GlobalVariable* getShadowRef(Module &M, GlobalVariable *GV);
      static Value* getMagicStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* array, Value* arrayIndex, const std::string &structFieldName, std::string *structFieldNames);
      static Value* getMagicSStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* magicArray, Value* magicArrayIndex, const std::string &structFieldName);
      static Value* getMagicTStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* magicTypeArray, Value* magicTypeArrayIndex, const std::string &structFieldName);
      static Value* getMagicFStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* magicFunctionArray, Value* magicFunctionArrayIndex, const std::string &structFieldName);
      static Value* getMagicRStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* magicVar, const std::string &structFieldName);
      static Value* getMagicDStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* magicDsindexArray, Value* magicDsindexArrayIndex, const std::string &structFieldName);
      static Constant* getArrayPtr(Module &M, GlobalVariable* array);
      static void insertMemcpyInst(Module &M, Instruction *InsertBefore, Value *Dst, Value *Src, Value *Len, unsigned Align);
      static void insertCopyInst(Module &M, Instruction *InsertBefore, GlobalVariable *GV, GlobalVariable *SGV, int GVSize, bool forceMemcpy);
      static Function* getCalledFunctionFromCS(const CallSite &CS);
      static void replaceCallInst(Instruction *originalInst, CallInst *newInst, int argOffset=0, bool removeUnusedFunction=true);
      static std::vector<Function*> getGlobalVariablesShadowFunctions(Module &M, std::vector<GlobalVariable*> globalVariables, std::vector<GlobalVariable*> shadowGlobalVariables, std::vector<int> globalVariableSizes, GlobalVariable* magicArray, int magicArraySize, bool forceShadow, bool setDirtyFlag);
      static Function* getGlobalVariableShadowFunction(Module &M, GlobalVariable* GV, GlobalVariable* SGV, int GVSize, GlobalVariable* magicArray, int magicArrayIndex, bool forceShadow, bool setDirtyFlag);
      static void insertGlobalVariableCleanDirtyFlag(Module &M, GlobalVariable* GV, GlobalVariable* magicArray, int magicArrayIndex, Instruction *InsertBefore);
      static void insertShadowTag(Module &M, GlobalVariable *GV, Instruction *InsertBefore);
      static bool isShadowTag(Instruction *inst);
      static GlobalVariable* getGlobalVariableFromShadowTag(Instruction *inst, std::vector<Instruction*> &instructionsToRemove);
      static void cleanupShadowTag(Module &M, std::vector<Instruction*> &instructionsToRemove);
      static bool hasAddressTaken(const GlobalValue *GV, bool includeMembers=true);
      static bool lookupValueSet(const GlobalVariable *GV, std::vector<int> &valueSet);
      static Value* getStringOwner(GlobalVariable *GV);
      static Instruction* getFirstNonAllocaInst(Function* F, bool skipAllocaPoint=true);
      static void setGlobalVariableSection(GlobalVariable *GV, const std::string &section);
      static bool getCallAnnotation(Module &M, const CallSite &CS, int *annotation);
      static bool getVarAnnotation(Module &M, const GlobalVariable *GV, int *annotation);
      static CallSite getCallSiteFromInstruction(Instruction *I);
      static AllocaInst* getAllocaInstFromArgument(Argument *argument);
      static Function* getMangledFunction(Module &M, StringRef functionName);
      static Function* getFunction(Module &M, StringRef functionName);
      static bool isCompatibleType(const Type* type1, const Type* type2);
      static void inlinePreHookForwardingCall(Function* function, Function* preHookFunction, std::vector<unsigned> argsMapping, std::vector<Value*> trailingArgs);
      static void inlinePostHookForwardingCall(Function* function, Function* postHookFunction, std::vector<unsigned> mapping, std::vector<Value*> trailingArgs);
      static int getPointerIndirectionLevel(const Type* type);
      static Value* getFunctionParam(Function* function, unsigned index);
      static bool isLocalConstant(Module &M, GlobalVariable *GV);
};

}

#endif
