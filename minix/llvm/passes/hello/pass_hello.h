#ifndef _PASS_HELLO_H
#define _PASS_HELLO_H

#if LLVM_VERSION >= 33
#define ATTRIBUTE_SET_TY              AttributeSet
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>
#else /* LLVM_VERSION < 33 */
#define ATTRIBUTE_SET_TY              AttrListPtr
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Instructions.h>
#include <llvm/Type.h>
#include <llvm/Constants.h>
#include <llvm/Intrinsics.h>
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/IntrinsicInst.h>
#endif /* LLVM_VERSION >= 33 */

#if LLVM_VERSION >= 32
#define DATA_LAYOUT_TY 		      DataLayout
#define ATTRIBUTE_SET_RET_IDX         ATTRIBUTE_SET_TY::ReturnIndex
#define ATTRIBUTE_SET_FN_IDX          ATTRIBUTE_SET_TY::FunctionIndex
#include <llvm/DebugInfo.h>
#if LLVM_VERSION == 32
#include <llvm/DataLayout.h>
#include <llvm/IRBuilder.h>
#endif
#else /* LLVM_VERSION < 32 */
#define DATA_LAYOUT_TY 		      TargetData
#define ATTRIBUTE_SET_RET_IDX         0
#define ATTRIBUTE_SET_FN_IDX          (~0U)
#include <llvm/Target/TargetData.h>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/Support/IRBuilder.h>
#endif /* LLVM_VERSION >= 32 */

#if LLVM_VERSION >= 31
/* XXX Check. */
#define CONSTANT_ARRAY_INITIALIZER_TY ConstantDataArray

#else /* LLVM_VERSION < 31 */
#define CONSTANT_ARRAY_INITIALIZER_TY ConstantArray
#endif /* LLVM_VERSION >= 31 */

#if LLVM_VERSION >= 30
#define BASE_PARSER                   parser

#define TYPECONST
#else /* LLVM_VERSION < 30 */
#define BASE_PARSER                   basic_parser

#define TYPECONST const
#endif /* LLVM_VERSION >= 30 */

#if LLVM_VERSION >= 29
#define VALUE_TO_VALUE_MAP_TY ValueToValueMapTy
#else  /* LLVM_VERSION < 29 */
#define VALUE_TO_VALUE_MAP_TY ValueMap<const Value*, Value*>
#endif /* LLVM_VERSION >= 29 */

#define ZERO_CONSTANT_INT(M) ConstantInt::get((M).getContext(), APInt(32, 0, 10))
#define VOID_PTR_TY(M)       PointerType::get(IntegerType::get((M).getContext(), 8), 0)
#define VOID_PTR_PTR_TY(M)   PointerType::get(PointerType::get(IntegerType::get((M).getContext(), 8), 0), 0)

#define DEBUG_LLVM_DEBUG_API 0

typedef enum PassUtilLinkageTypeE {
    PASS_UTIL_LINKAGE_NONE = 0,
    PASS_UTIL_LINKAGE_WEAK,
    PASS_UTIL_LINKAGE_COMMON,
    PASS_UTIL_LINKAGE_EXTERNAL,
    PASS_UTIL_LINKAGE_EXTERNAL_WEAK,
    PASS_UTIL_LINKAGE_WEAK_POINTER,
    PASS_UTIL_LINKAGE_PRIVATE,
    __NUM_PASS_UTIL_LINKAGE_TYPES
    /* Values here should only be appended at the end, external components (e.g., scripts) may be relying on them.*/
} PassUtilLinkageType;

#define PASS_UTIL_LINKAGE_TYPE_STRINGS \
    "NONE", \
    "WEAK", \
    "COMMON", \
    "EXTERNAL", \
    "EXTERNAL_WEAK", \
    "WEAK_POINTER", \
    "PRIVATE"

typedef enum PassUtilPropE {
    PASS_UTIL_PROP_NONE,
    PASS_UTIL_PROP_NOINLINE,
    PASS_UTIL_PROP_USED,
    PASS_UTIL_PROP_PRESERVE,
    __NUM_PASS_UTIL_PROPS
} PassUtilProp;

#define PASS_UTIL_FLAG(F) (1 << F)

#define PASS_COMMON_INIT_ONCE() \
    Module *PassUtil::M = NULL; \

using namespace llvm;

namespace llvm {

class PassUtil {
  public:
      static Constant* getGetElementPtrConstant(Constant *constant, std::vector<Value*> &indexes);
      static CallInst* createCallInstruction(Value *F, std::vector<Value*> &args, const Twine &NameStr="", Instruction *InsertBefore=0);
      static CallInst* createCallInstruction(Value *F, std::vector<Value*> &args, const Twine &NameStr="", BasicBlock *InsertAtEnd=0);
      static FunctionType* getFunctionType(TYPECONST Type* Result, std::vector<TYPECONST Type*> &argsTy, bool isVarArg=false);
      static Constant* getStringConstantArray(Module &M, const std::string &string);
      static GlobalVariable* getStringGlobalVariable(Module &M, const std::string &string, const std::string &varName, const std::string &varSection = "", Constant **getElementPtrExpr=NULL, bool cacheable=false);
  private:
      static Module *M;
};

inline Constant* PassUtil::getGetElementPtrConstant(Constant *constant, std::vector<Value*> &indexes) {
#if LLVM_VERSION >= 30
    ArrayRef<Value*> ref(indexes);
    return ConstantExpr::getGetElementPtr(constant, ref);
#else
    return ConstantExpr::getGetElementPtr(constant, &indexes[0], indexes.size());
#endif
}

inline CallInst* PassUtil::createCallInstruction(Value *F, std::vector<Value*> &args, const Twine &NameStr, Instruction *InsertBefore) {
#if LLVM_VERSION >= 30
    ArrayRef<Value*> ref(args);
    return CallInst::Create(F, ref, NameStr, InsertBefore);
#else
    return CallInst::Create(F, args.begin(), args.end(), NameStr, InsertBefore);
#endif
}

inline CallInst* PassUtil::createCallInstruction(Value *F, std::vector<Value*> &args, const Twine &NameStr, BasicBlock *InsertAtEnd) {
#if LLVM_VERSION >= 30
    ArrayRef<Value*> ref(args);
    return CallInst::Create(F, ref, NameStr, InsertAtEnd);
#else
    return CallInst::Create(F, args.begin(), args.end(), NameStr, InsertAtEnd);
#endif
}

inline FunctionType* PassUtil::getFunctionType(TYPECONST Type* Result, std::vector<TYPECONST Type*> &argsTy, bool isVarArg)
{
#if LLVM_VERSION >= 30
    ArrayRef<TYPECONST Type*> ref(argsTy);
    return FunctionType::get(Result, ref, isVarArg);
#else
    return FunctionType::get(Result, argsTy, isVarArg);
#endif
}


inline Constant* PassUtil::getStringConstantArray(Module &M, const std::string &string)
{
  std::vector<Constant*> elements;
  elements.reserve(string.size() + 1);
  for (unsigned i = 0; i < string.size(); ++i)
    elements.push_back(ConstantInt::get(Type::getInt8Ty(M.getContext()), string[i]));

  // Add a null terminator to the string...
  elements.push_back(ConstantInt::get(Type::getInt8Ty(M.getContext()), 0));

  ArrayType *ATy = ArrayType::get(Type::getInt8Ty(M.getContext()), elements.size());
  return ConstantArray::get(ATy, elements);
}

inline GlobalVariable* PassUtil::getStringGlobalVariable(Module &M, const std::string &string, const std::string &varName, const std::string &varSection, Constant **getElementPtrExpr, bool cacheable)
{
    static std::map<std::string, GlobalVariable*> stringCache;
    std::map<std::string, GlobalVariable*>::iterator stringCacheIt;
    std::string stringCacheKey;
    GlobalVariable *strGV = NULL;

    if (cacheable) {
    	stringCacheKey = string + "~!~!" + varName + "~!~!" + varSection;
        stringCacheIt = stringCache.find(stringCacheKey);
        if (stringCacheIt != stringCache.end()) {
            strGV = stringCacheIt->second;
            cacheable = false;
        }
    }

    if (!strGV) {
        //create a constant internal string reference
        Constant *stringValue = PassUtil::getStringConstantArray(M, string);

        //create the global variable, cache it, and record it in the module
        strGV = new GlobalVariable(M, stringValue->getType(), true,
            GlobalValue::InternalLinkage, stringValue, varName);
        if (varSection.compare("")) {
            strGV->setSection(varSection);
        }
    }
    if (getElementPtrExpr) {
    	    std::vector<Value*> strConstantIndices;
    	    strConstantIndices.push_back(ZERO_CONSTANT_INT(M));
    	    strConstantIndices.push_back(ZERO_CONSTANT_INT(M));
    	    *getElementPtrExpr = PassUtil::getGetElementPtrConstant(strGV, strConstantIndices);
    }

    if (cacheable) {
        stringCache.insert(std::pair<std::string, GlobalVariable*>(stringCacheKey, strGV));
    }

    return strGV;
}

}

#endif /* _PASS_HELLO_H */
