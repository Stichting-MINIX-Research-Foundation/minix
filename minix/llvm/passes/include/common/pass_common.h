#ifndef _PASS_COMMON_H
#define _PASS_COMMON_H

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
#include <llvm/IR/DebugInfo.h>
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

#define FOREACH_FUNC(M, F, B) do { \
    Module::FunctionListType &__FL = (M).getFunctionList(); \
    for (Module::iterator __MI = __FL.begin(); __MI != __FL.end(); ++__MI) { \
        const Function *F = __MI; \
        if (F->isIntrinsic()) \
            continue; \
        B \
    } \
} while(0)

#define FOREACH_FUNC_INS(F, I, B) do { \
    for (Function::const_iterator __FI = F->begin(), __FE = F->end(); __FI != __FE; ++__FI) { \
        for (BasicBlock::const_iterator __BI = __FI->begin(), BE = __FI->end(); __BI != BE; ++__BI) { \
            Instruction *I = (Instruction*) ((unsigned long) &(*__BI)); \
            B \
        } \
    } \
} while(0)

#define FOREACH_FUNC_CS(F, CS, B) do { \
    FOREACH_FUNC_INS(F, I, \
        CallSite CS = PassUtil::getCallSiteFromInstruction(I); \
        if (!CS.getInstruction()) \
            continue; \
        B \
    ); \
} while(0)

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
      static void writeTypeSymbolic(raw_string_ostream &OS, TYPECONST Type *type, const Module *M);
      static const std::string getTypeDescription(TYPECONST Type* type);
      static MDNode *findDbgGlobalDeclare(GlobalVariable *V);
      static MDNode *findDbgSubprogramDeclare(const Function *F);
      static void getDbgLocationInfoRelPath(const std::string &baseDir, const std::string &filename, const std::string &directory, std::string &relPath);
      static void getDbgLocationInfo(DIDescriptor &DID, const std::string &baseDir, std::string *filename, std::string *directory, std::string *relPath);
      static bool getInstrDbgLocationInfo(Instruction *I, const std::string &baseDir, std::string *filename, std::string *directory, std::string *relPath, unsigned int *lineNum, bool expand=true);
      static unsigned getDbgSubrangeNumElements(const DISubrange &subrange);
      static bool isDbgVectorTy(const DIType &type);
      static DIType getDITypeDerivedFrom(const DIDerivedType &type);
      static DIType getDITypeFromRef(const DITypeRef &ref);
      static bool isOpaqueTy(TYPECONST Type *type);
      static bool isPrimitiveTy(TYPECONST Type *type);
      static Constant* getGetElementPtrConstant(Constant *constant, std::vector<Value*> &indexes);
      static GetElementPtrInst* createGetElementPtrInstruction(Value *ptr, std::vector<Value*> &indexes, const Twine &NameStr="", Instruction *InsertBefore=0);
      static GetElementPtrInst* createGetElementPtrInstruction(Value *ptr, std::vector<Value*> &indexes, const Twine &NameStr="", BasicBlock *InsertAtEnd=0);
      static CallInst* createCallInstruction(Value *F, std::vector<Value*> &args, const Twine &NameStr="", Instruction *InsertBefore=0);
      static CallInst* createCallInstruction(Value *F, std::vector<Value*> &args, const Twine &NameStr="", BasicBlock *InsertAtEnd=0);
      static Function* getIntrinsicFunction(Module &M, Intrinsic::ID id, TYPECONST Type** types=NULL, unsigned size=0);
      static FunctionType* getFunctionType(TYPECONST Type* Result, std::vector<TYPECONST Type*> &argsTy, bool isVarArg=false);
      static Function* setFunctionProperties(Function *F, unsigned long properties);
      static Function* createFunctionWeakPtrWrapper(Module &M, StringRef Name, FunctionType *Ty);
      static Function* getOrInsertFunction(Module &M, StringRef Name, FunctionType *Ty, PassUtilLinkageType insertLinkage, unsigned long properties);
      static PassUtilLinkageType getFunctionPassUtilLinkageType(Function *F);
      static std::string getPassUtilLinkageTypeString(PassUtilLinkageType linkageType);
      static void getFunctionEntryExits(Function *F, BasicBlock **entryBlock, std::vector<BasicBlock*> *exitBlocks);
      static bool isReturnedValue(Function *F, Value *V);
      static CallSite getCallSiteFromInstruction(Instruction *I);
      static CallSite getCallSiteFromUser(User *U);
      static void getFunctionsInDirectBUCallgraph(Function* F, std::set<Function*> &funcs);
      static void getAllocaInfo(Function *F, Instruction **allocaInsertionPoint, Instruction **firstNonAllocaInst);
      static Constant* getStringConstantArray(Module &M, const std::string &string);
      static GlobalVariable* getStringGlobalVariable(Module &M, const std::string &string, const std::string &varName = ".str.pu", const std::string &varSection = "", Constant **getElementPtrExpr=NULL, bool cacheable=false);
      static ATTRIBUTE_SET_TY remapCallSiteAttributes(CallSite &CS, int argOffset);
      static void parseStringListOpt(std::vector<std::string> &vector, const std::string &string, const std::string &separator = ":");
      static void parseStringPairListOpt(std::set<std::pair<std::string, std::string> > &set, const std::string &string, const std::string &listSeparator = ":", const std::string &pairSeparator = ";");
      static void parseRegexListOpt(std::vector<Regex*> &list, const std::string &string);
      static bool matchRegexes(std::string string, std::vector<Regex*> &regexes);
      static void setModule(Module *M);
      static void getModuleName(Module &M, std::string *fullName, std::string *dirName, std::string *baseName);
      static unsigned long getTypeHash(TYPECONST Type* type, unsigned maxLevel=11);
  private:
      static Module *M;
};

inline void PassUtil::writeTypeSymbolic(raw_string_ostream &OS, TYPECONST Type *type, const Module *M) {
#if LLVM_VERSION >= 30
    /* XXX Check. */
    type->print(OS);
    return;
#else
    return WriteTypeSymbolic(OS, type, M);
#endif
}

inline const std::string PassUtil::getTypeDescription(TYPECONST Type* type) {
    std::string string;
#if LLVM_VERSION >= 30
    /* XXX Check. */
    raw_string_ostream ostream(string);
    type->print(ostream);
    ostream.flush();
#else
    string = type->getDescription();
#endif

    return string;
}

inline MDNode *PassUtil::findDbgGlobalDeclare(GlobalVariable *V) {
#if LLVM_VERSION >= 30
  const Module *M = V->getParent();
  NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.cu");
  if (!NMD)
    return 0;

  for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
      DICompileUnit CU(NMD->getOperand(i));
      DIArray GVs = CU.getGlobalVariables();
      for (unsigned i = 0, e = GVs.getNumElements(); i != e; ++i) {
        DIDescriptor DIG(GVs.getElement(i));
        if (DIGlobalVariable(DIG).getGlobal() == V)
          return DIG;
      }
  }
  return 0;
#else
  const Module *M = V->getParent();
  NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.gv");
  if (!NMD)
    return 0;

  for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
    DIDescriptor DIG(cast<MDNode>(NMD->getOperand(i)));
    if (!DIG.isGlobalVariable())
      continue;
    if (DIGlobalVariable(DIG).getGlobal() == V)
      return DIG;
  }
  return 0;
#endif
}

inline MDNode *PassUtil::findDbgSubprogramDeclare(const Function *V) {
#if LLVM_VERSION >= 30
  const Module *M = V->getParent();
  NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.cu");
  if (!NMD)
    return 0;

  for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
      DICompileUnit CU(NMD->getOperand(i));
      DIArray SPs = CU.getSubprograms();
      for (unsigned i = 0, e = SPs.getNumElements(); i != e; ++i) {
           DISubprogram DIS(SPs.getElement(i));
           if (DIS.getFunction() == V) {
           	return DIS;
           }
      }
  }
  return 0;
#else
  const Module *M = V->getParent();
  NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.sp");
  if (!NMD)
    return 0;

  for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
    DIDescriptor DIG(cast<MDNode>(NMD->getOperand(i)));
    if (!DIG.isSubprogram())
      continue;
    if (DISubprogram(DIG).getFunction() == V)
      return DIG;
  }
  return 0;
#endif
}

inline void PassUtil::getDbgLocationInfoRelPath(const std::string &baseDir, const std::string &filename, const std::string &directory, std::string &relPath) {
    StringRef directoryRef(directory);
    std::pair<StringRef, StringRef> stringPair = directoryRef.split(baseDir);
    relPath = (stringPair.second.compare("") ? stringPair.second.str() : stringPair.first.str()) + "/" + filename;
#if DEBUG_LLVM_DEBUG_API
    errs() << " - getDbgLocationInfoRelPath: Location Info is: " << directory << " | " << filename << " | " << relPath << "\n";
#endif
}

inline void PassUtil::getDbgLocationInfo(DIDescriptor &DID, const std::string &baseDir, std::string *filename, std::string *directory, std::string *relPath) {
  StringRef _directory;
  StringRef _filename;
  if (DID.isGlobalVariable()) {
#if LLVM_VERSION >= 30
    _directory = ((DIGlobalVariable*)&DID)->getDirectory();
    _filename = ((DIGlobalVariable*)&DID)->getFilename();
#else
    _directory = ((DIGlobalVariable*)&DID)->getCompileUnit().getDirectory();
    _filename = ((DIGlobalVariable*)&DID)->getCompileUnit().getFilename();
#endif
#if DEBUG_LLVM_DEBUG_API
    errs() << "DIGlobalVariable name is: " << ((DIGlobalVariable*)&DID)->getName() << "\n";
#endif
  }
  else if (DID.isSubprogram()) {
    _directory = ((DISubprogram*)&DID)->getDirectory();
    _filename = ((DISubprogram*)&DID)->getFilename();
#if DEBUG_LLVM_DEBUG_API
    errs() << "DISubprogram name is: " << ((DISubprogram*)&DID)->getName() << "\n";
#endif
  }
  else {
    DIScope DIS;
    assert (DID.isVariable());
    DIS = ((DIVariable*)&DID)->getContext();
    if (DIS.isSubprogram()) {
        _directory = ((DISubprogram*)&DIS)->getDirectory();
        _filename = ((DISubprogram*)&DIS)->getFilename();
#if DEBUG_LLVM_DEBUG_API
        errs() << "DIVariable (SP) name is: " << ((DIVariable*)&DID)->getName() << "\n";
#endif
    }
    else if (DIS.isLexicalBlock()) {
        _directory = ((DILexicalBlock*)&DIS)->getDirectory();
        _filename = ((DILexicalBlock*)&DIS)->getFilename();
#if DEBUG_LLVM_DEBUG_API
        errs() << "DIVariable (LB) name is: " << ((DIVariable*)&DID)->getName() << "\n";
#endif
    }
    else {
#if LLVM_VERSION >= 30
        assert(DIS.isLexicalBlockFile());
        _directory = ((DILexicalBlockFile*)&DIS)->getDirectory();
        _filename = ((DILexicalBlockFile*)&DIS)->getFilename();
#if DEBUG_LLVM_DEBUG_API
        errs() << "DIVariable (LBF) name is: " << ((DIVariable*)&DID)->getName() << "\n";
#endif
#else
	assert(0 && "Unexpected DIScope instance!");
#endif
    }
  }
  if (filename) {
    *filename = _filename;
  }
  if (directory) {
    *directory = _directory;
  }
  if (relPath) {
    getDbgLocationInfoRelPath(baseDir, _filename, _directory, *relPath);
  }
}

inline bool PassUtil::getInstrDbgLocationInfo(Instruction *I, const std::string &baseDir, std::string *filename, std::string *directory, std::string *relPath, unsigned int *lineNum, bool expand) {
    BasicBlock::iterator BI = I;
    MDNode *N = BI->getMetadata("dbg");
    if (!N && !expand) {
        return false;
    }
    while(!N) {
        if (BI->isTerminator()) {
            BranchInst *BInst = dyn_cast<BranchInst>(BI);
            if (BInst && BInst->isUnconditional()) {
                BI = BInst->getSuccessor(0)->front();
                N = BI->getMetadata("dbg");
                continue;
            }
            return false;
        }
        BI++;
        N = BI->getMetadata("dbg");
    }

    DILocation DIL(N);
    StringRef _directory = DIL.getDirectory();
    StringRef _filename = DIL.getFilename();
    if (filename) {
        *filename = _filename;
    }
    if (directory) {
        *directory = _directory;
    }
    if (relPath) {
      getDbgLocationInfoRelPath(baseDir, _filename, _directory, *relPath);
    }
    if (lineNum) {
        *lineNum = DIL.getLineNumber();
    }

    return true;
}

inline unsigned PassUtil::getDbgSubrangeNumElements(const DISubrange &subrange) {
#if LLVM_VERSION >= 33
    const unsigned numElements = (unsigned) subrange.getCount();
#else
    const unsigned low = (unsigned) subrange.getLo();
    const unsigned high = (unsigned) subrange.getHi();
    const unsigned numElements = high - low + 1;
#endif

    return numElements;
}

inline bool PassUtil::isDbgVectorTy(const DIType &type) {
#if LLVM_VERSION >= 33
    return type.isVector();
#else
    return type.getTag() == dwarf::DW_TAG_vector_type;
#endif
}

inline DIType PassUtil::getDITypeDerivedFrom(const DIDerivedType &type) {
#if LLVM_VERSION >= 34
    static DITypeIdentifierMap TypeIdentifierMap;
    static bool TypeMapInitialized = false;
    if (!TypeMapInitialized) {
        assert(PassUtil::M && "Set module first!");
        if (NamedMDNode *CU_Nodes = PassUtil::M->getNamedMetadata("llvm.dbg.cu")) {
          TypeIdentifierMap = generateDITypeIdentifierMap(CU_Nodes);
          TypeMapInitialized = true;
        }
    }
    return type.getTypeDerivedFrom().resolve(TypeIdentifierMap);
#else
    return type.getTypeDerivedFrom();
#endif
}

inline DIType PassUtil::getDITypeFromRef(const DITypeRef &ref) {
    static DITypeIdentifierMap TypeIdentifierMap;
    static bool TypeMapInitialized = false;
    if (!TypeMapInitialized) {
        /* TODO: generate the type identifier map only once! */
        assert(PassUtil::M && "Set module first!");
        if (NamedMDNode *CU_Nodes = PassUtil::M->getNamedMetadata("llvm.dbg.cu")) {
          TypeIdentifierMap = generateDITypeIdentifierMap(CU_Nodes);
          TypeMapInitialized = true;
        }
    }
    return ref.resolve(TypeIdentifierMap);
}

inline bool PassUtil::isOpaqueTy(TYPECONST Type *type) {
#if LLVM_VERSION >= 30
    return type->isStructTy() && (((TYPECONST StructType*)type)->isOpaque() || type->getNumContainedTypes() == 0);
#else
    return type->isOpaqueTy();
#endif
}

inline bool PassUtil::isPrimitiveTy(TYPECONST Type *type) {
    return type->isVoidTy() || type->isFloatingPointTy() || type->isLabelTy() || type->isMetadataTy() || type->isX86_MMXTy();
}

inline Constant* PassUtil::getGetElementPtrConstant(Constant *constant, std::vector<Value*> &indexes) {
#if LLVM_VERSION >= 30
    ArrayRef<Value*> ref(indexes);
    return ConstantExpr::getGetElementPtr(constant, ref);
#else
    return ConstantExpr::getGetElementPtr(constant, &indexes[0], indexes.size());
#endif
}

inline GetElementPtrInst* PassUtil::createGetElementPtrInstruction(Value *ptr, std::vector<Value*> &indexes, const Twine &NameStr, Instruction *InsertBefore) {
#if LLVM_VERSION >= 30
    ArrayRef<Value*> ref(indexes);
    return GetElementPtrInst::Create(ptr, ref, NameStr, InsertBefore);
#else
    return GetElementPtrInst::Create(ptr, indexes.begin(), indexes.end(), NameStr, InsertBefore);
#endif
}

inline GetElementPtrInst* PassUtil::createGetElementPtrInstruction(Value *ptr, std::vector<Value*> &indexes, const Twine &NameStr, BasicBlock *InsertAtEnd) {
#if LLVM_VERSION >= 30
    ArrayRef<Value*> ref(indexes);
    return GetElementPtrInst::Create(ptr, ref, NameStr, InsertAtEnd);
#else
    return GetElementPtrInst::Create(ptr, indexes.begin(), indexes.end(), NameStr, InsertAtEnd);
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

inline Function* PassUtil::getIntrinsicFunction(Module &M, Intrinsic::ID id, TYPECONST Type** types, unsigned size) {
#if LLVM_VERSION >= 30
    std::vector<TYPECONST Type*> typeVector;
    for(unsigned i=0;i<size;i++) {
        typeVector.push_back(types[i]);
    }
    ArrayRef<TYPECONST Type*> ref(typeVector);
    return Intrinsic::getDeclaration(&M, id, ref);
#else
    return Intrinsic::getDeclaration(&M, id, types, size);
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

inline Function* PassUtil::setFunctionProperties(Function *F, unsigned long props)
{
    assert(F);
    bool preserve = props & (PASS_UTIL_FLAG(PASS_UTIL_PROP_NOINLINE)|PASS_UTIL_FLAG(PASS_UTIL_PROP_USED)|PASS_UTIL_FLAG(PASS_UTIL_PROP_PRESERVE));

    if (F->isDeclaration()) {
        return F;
    }
    if (preserve) {
        Instruction *I;
        getAllocaInfo(F, NULL, &I);
        assert(I);

        /* Add a volatile store to a new global variable to preserve it. */
        PointerType* voidPointerTy = PointerType::get(IntegerType::get(F->getContext(), 8), 0);
        GlobalVariable* volatileVar = new GlobalVariable(*F->getParent(),
            voidPointerTy, false, GlobalValue::CommonLinkage,
            0, F->getName() + "_llvm_propvar");
        volatileVar->setInitializer(ConstantPointerNull::get(voidPointerTy));
        new StoreInst(ConstantExpr::getCast(Instruction::BitCast, F, voidPointerTy), volatileVar, true, I);
    }
    return F;
}

inline Function* PassUtil::createFunctionWeakPtrWrapper(Module &M, StringRef Name, FunctionType *Ty)
{
    unsigned i;
    Function *F = getOrInsertFunction(M, Name.str() + "_llvm_weakptrwrapper" , Ty, PASS_UTIL_LINKAGE_COMMON, 0);
    TYPECONST Type *RetTy = Ty->getReturnType();
    PointerType *FPtrTy = PointerType::get(Ty, 0);
    Constant *FPtrNull = Constant::getNullValue(FPtrTy);

    /* Create the global function pointer variable. */
    GlobalVariable* weakPtrVar = new GlobalVariable(M, FPtrTy, false,
        GlobalValue::CommonLinkage, 0, Name);
    weakPtrVar->setInitializer(FPtrNull);

    /* Create the wrapper function body. */
    F->dropAllReferences();
    BasicBlock* entryBB = BasicBlock::Create(M.getContext(), "entry",F,0);
    BasicBlock* weakPtrOverridenBB = BasicBlock::Create(M.getContext(), "have." + Name.str(),F,0);
    BasicBlock* endBB = BasicBlock::Create(M.getContext(), "end",F,0);
    AllocaInst* retval = NULL;

    /* Parse arguments. */
    std::vector<AllocaInst*> argsAllocaInsts;
    for (Function::arg_iterator args = F->arg_begin(); args != F->arg_end(); args++) {
        Value *argValue = args;
        AllocaInst *allocaInst = new AllocaInst(argValue->getType(), ".llvm.pu.args", entryBB);
        argsAllocaInsts.push_back(allocaInst);
    }
    if (!RetTy->isVoidTy()) {
        retval = new AllocaInst(RetTy, "retval", entryBB);
    }
    i=0;
    for (Function::arg_iterator args = F->arg_begin(); args != F->arg_end(); args++, i++) {
        Value *argValue = args;
        AllocaInst *allocaInst = argsAllocaInsts[i];
        new StoreInst(argValue, allocaInst, true, entryBB);
    }
    if (retval) {
        new StoreInst(Constant::getNullValue(RetTy), retval, true, entryBB);
    }

    /* Build entry block. */
    LoadInst* weakPtr = new LoadInst(weakPtrVar, "", true, entryBB);
    ICmpInst* cmpInst = new ICmpInst(*entryBB, ICmpInst::ICMP_NE, weakPtr, FPtrNull, "");
    BranchInst::Create(weakPtrOverridenBB, endBB, cmpInst, entryBB);

    /* Build weakPtrOverriden block, only executed with a non-NULL weakPtr */
    std::vector<Value*> weakPtrCallParams;
    i=0;
    for (Function::arg_iterator args = F->arg_begin(); args != F->arg_end(); args++, i++) {
        AllocaInst *allocaInst = argsAllocaInsts[i];
        weakPtrCallParams.push_back(new LoadInst(allocaInst, "", true, weakPtrOverridenBB));
    }
    weakPtr = new LoadInst(weakPtrVar, "", true, weakPtrOverridenBB);
    CallInst* weakPtrCall = createCallInstruction(weakPtr, weakPtrCallParams, "", weakPtrOverridenBB);
    weakPtrCall->setCallingConv(CallingConv::C);

    if (retval) {
        new StoreInst(weakPtrCall, retval, false, weakPtrOverridenBB);
    }
    BranchInst::Create(endBB, weakPtrOverridenBB);

    /* Build end block. */
    if (!retval) {
        ReturnInst::Create(M.getContext(), endBB);
    }
    else {
        LoadInst* retvalValue = new LoadInst(retval, "", false, endBB);
        ReturnInst::Create(M.getContext(), retvalValue, endBB);
    }
    return F;
}

inline Function* PassUtil::getOrInsertFunction(Module &M, StringRef Name, FunctionType *Ty, PassUtilLinkageType insertLinkage, unsigned long properties)
{
    static std::map<std::string, Function *> functionMap;
    std::map<std::string, Function *>::iterator functionMapIt;
    Function *F = NULL;
    bool needsEmptyBody = true;
    bool needsProperties = true;
    bool needsIncludsion = true;

    functionMapIt = functionMap.find(Name);
    if (functionMapIt != functionMap.end()) {
        return functionMapIt->second;
    }
    F = M.getFunction(Name);

    if (F) {
        /* If the function exists, check the type and return it. */
        if (F->getFunctionType() != Ty) {
            return NULL;
        }
        functionMap.insert(std::pair<std::string, Function *>(Name, F));
        setFunctionProperties(F, properties);
        return F;
    }

    /* Has the user requested creation of the function otherwise? */
    if (insertLinkage == PASS_UTIL_LINKAGE_NONE) {
        return NULL;
    }
    switch(insertLinkage) {
    case PASS_UTIL_LINKAGE_WEAK:
        /* Create empty function that can optionally be overriden at link time*/
        F = Function::Create(Ty, GlobalVariable::WeakAnyLinkage, Name);
    break;
    case PASS_UTIL_LINKAGE_COMMON:
        /* Creates empty function, non overridable. */
        F = Function::Create(Ty, GlobalVariable::InternalLinkage, Name);
    break;
    case PASS_UTIL_LINKAGE_EXTERNAL:
        /* Creates function declaration that must be defined at link time. */
        F = Function::Create(Ty, GlobalVariable::ExternalLinkage, Name);
        needsEmptyBody = false;
    break;
    case PASS_UTIL_LINKAGE_EXTERNAL_WEAK:
        /* Creates weak function declaration that can optionally be defined
         * at link time (if undefined the linker will emit a NULL symbol).
         */
        F = Function::Create(Ty, GlobalVariable::ExternalWeakLinkage, Name);
        needsEmptyBody = false;
    break;
    case PASS_UTIL_LINKAGE_WEAK_POINTER:
        /* Creates function pointer initialized to NULL that can optionally
         * be initialized at runtime. Invocations are wrapped to ensure that
         * indirect call is performed on a NULL pointer. This is to emulate
         * Mac OS' weak_pointer attribute, which allows weak symbols to be
         * overriden in LD_PRELOADED libraries at runtime.
         */
        F = PassUtil::createFunctionWeakPtrWrapper(M, Name, Ty);
        needsProperties = false;
        needsIncludsion = false;
    break;
    default:
        return NULL;
    break;
    }
    if (needsIncludsion) {
        M.getFunctionList().push_back(F);
    }
    if (needsEmptyBody) {
        BasicBlock* block = BasicBlock::Create(M.getContext(), "entry", F);
        IRBuilder<> builder(block);
        TYPECONST Type *RetTy = Ty->getReturnType();
        if (RetTy->isVoidTy()) {
            builder.CreateRetVoid();
        }
        else {
            builder.CreateRet(Constant::getNullValue(RetTy));
        }
    }
    functionMap.insert(std::pair<std::string, Function *>(Name, F));
    if (needsProperties) {
        setFunctionProperties(F, properties);
    }
    return F;
}

inline PassUtilLinkageType PassUtil::getFunctionPassUtilLinkageType(Function *F)
{
    if (F->isDeclaration()) {
        return PASS_UTIL_LINKAGE_EXTERNAL;
    }
    if (F->hasInternalLinkage()) {
        return PASS_UTIL_LINKAGE_PRIVATE;
    }
    return PASS_UTIL_LINKAGE_COMMON;
}

inline std::string PassUtil::getPassUtilLinkageTypeString(PassUtilLinkageType linkageType)
{
    const char *strArray[] = { PASS_UTIL_LINKAGE_TYPE_STRINGS };
    std::string str(strArray[linkageType]);
    return str;
}

inline void PassUtil::getFunctionEntryExits(Function *F, BasicBlock **entryBlock, std::vector<BasicBlock*> *exitBlocks)
{
    if (entryBlock) {
        *entryBlock = &F->front();
    }
    if (exitBlocks) {
        for(Function::iterator I = F->begin(), E = F->end(); I != E; ++I) {
            if (isa<ReturnInst>(I->getTerminator()) || isa<UnreachableInst>(I->getTerminator()))
                exitBlocks->push_back(I);
        }
    }
}

inline bool PassUtil::isReturnedValue(Function *F, Value *V)
{
    std::vector<BasicBlock*> exitBlocks;
    PassUtil::getFunctionEntryExits(F, NULL, &exitBlocks);
    for (unsigned i=0;i<exitBlocks.size();i++) {
        Instruction *I = exitBlocks[i]->getTerminator();
        ReturnInst *RI = dyn_cast<ReturnInst>(I);
        if (RI && RI->getReturnValue()) {
            Value *RV = RI->getReturnValue();
            if (RV == V) {
                return true;
            }
            if (LoadInst *LI = dyn_cast<LoadInst>(RV)) {
                if (LI->getPointerOperand() == V) {
                    return true;
                }
            }
        }
    }
    return false;
}

inline CallSite PassUtil::getCallSiteFromInstruction(Instruction *I)
{
  return getCallSiteFromUser(I);
}

inline CallSite PassUtil::getCallSiteFromUser(User *U)
{
  CallSite CS(U->stripPointerCasts());
  CallSite emptyCS;
  Instruction *I = CS.getInstruction();
  if (!I)
      return emptyCS;
  if (isa<CallInst>(I) && dyn_cast<CallInst>(I)->isInlineAsm())
      return emptyCS;
  Function *F = CS.getCalledFunction();
  if (F && F->isIntrinsic())
      return emptyCS;
  return CS;
}

inline void PassUtil::getFunctionsInDirectBUCallgraph(Function* F, std::set<Function*> &funcs) {
  if (funcs.find(F) != funcs.end())
      return;
  funcs.insert(F);
  FOREACH_FUNC_CS(F, CS,
      if (!CS.getCalledFunction())
          continue;
      getFunctionsInDirectBUCallgraph(CS.getCalledFunction(), funcs);
  );
}

inline void PassUtil::getAllocaInfo(Function *F, Instruction **allocaInsertionPoint, Instruction **firstNonAllocaInst)
{
    assert(!F->isDeclaration());
    BasicBlock::iterator allocaIP = F->front().begin();
    while (isa<AllocaInst>(allocaIP)) ++allocaIP;
    BasicBlock::iterator firstNonAI = allocaIP;
    if (firstNonAI->getName().equals("alloca point")) {
    	firstNonAI++;
    }
    if(allocaInsertionPoint) {
    	*allocaInsertionPoint = allocaIP;
    }
    if(firstNonAllocaInst) {
    	*firstNonAllocaInst = firstNonAI;
    }
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

inline ATTRIBUTE_SET_TY PassUtil::remapCallSiteAttributes(CallSite &CS, int argOffset)
{
    ATTRIBUTE_SET_TY Attrs = CS.getAttributes();
    ATTRIBUTE_SET_TY NewAttrs;
#if LLVM_VERSION >= 33
    Instruction *I = CS.getInstruction();
    NewAttrs.addAttributes(I->getContext(), ATTRIBUTE_SET_RET_IDX, Attrs.getRetAttributes());
    NewAttrs.addAttributes(I->getContext(), ATTRIBUTE_SET_FN_IDX, Attrs.getFnAttributes());
    for (unsigned i=1;i<=CS.arg_size();i++) {
        NewAttrs.addAttributes(I->getContext(), i+argOffset, Attrs.getParamAttributes(i));
    } 
#elif LLVM_VERSION == 32
    Instruction *I = CS.getInstruction();
    NewAttrs.addAttr(I->getContext(), ATTRIBUTE_SET_RET_IDX, Attrs.getRetAttributes());
    NewAttrs.addAttr(I->getContext(), ATTRIBUTE_SET_FN_IDX, Attrs.getFnAttributes());
    for (unsigned i=1;i<=CS.arg_size();i++) {
        NewAttrs.addAttr(I->getContext(), i+argOffset, Attrs.getParamAttributes(i));
    } 
#else
    NewAttrs.addAttr(ATTRIBUTE_SET_RET_IDX, Attrs.getRetAttributes());
    NewAttrs.addAttr(ATTRIBUTE_SET_FN_IDX, Attrs.getFnAttributes());
    for (unsigned i=1;i<=CS.arg_size();i++) {
        NewAttrs.addAttr(i+argOffset, Attrs.getParamAttributes(i));
    }
#endif

    return NewAttrs;
}

inline void PassUtil::parseStringListOpt(std::vector<std::string> &list, const std::string &string, const std::string &separator)
{
    if(string.compare("")) {
        SmallVector< StringRef, 8 > vector;
        StringRef sref(string);
        sref.split(vector, separator, -1, false);
        list.insert(list.end(), vector.begin(), vector.end());
    }
}

inline void PassUtil::parseStringPairListOpt(std::set<std::pair<std::string, std::string> > &set, const std::string &string, const std::string &listSeparator, const std::string &pairSeparator)
{
	if(string.compare("")) {
		SmallVector< StringRef, 8 > vector;
		StringRef sref(string);
		sref.split(vector, listSeparator, -1, false);
		SmallVector< StringRef, 8 > parts;
		while(!vector.empty()) {
			StringRef token = vector.pop_back_val();
			parts.clear();
			token.split(parts, pairSeparator, -1, false);
			assert(parts.size() == 2 && "Two tokens were expected.");
			set.insert(std::pair<std::string, std::string>(parts.front(), parts.back()));
		}
	}
}

inline void PassUtil::parseRegexListOpt(std::vector<Regex*> &list, const std::string &string)
{
    std::vector<std::string> stringList;
    std::vector<std::string>::iterator it;
    PassUtil::parseStringListOpt(stringList, string);

    for (it = stringList.begin(); it != stringList.end(); ++it) {
        Regex* regex = new Regex(*it, 0);
        std::string error;
        assert(regex->isValid(error));
        list.push_back(regex);
    }
}

inline bool PassUtil::matchRegexes(std::string string, std::vector<Regex*> &regexes)
{
    for (std::vector<Regex*>::iterator it = regexes.begin(); it != regexes.end(); ++it) {
    	Regex *regex = *it;
    	if(regex->match(string, NULL)) {
    	    return true;
    	}
    }

    return false;
}

inline void PassUtil::setModule(Module *M)
{
    PassUtil::M = M;
}

inline void PassUtil::getModuleName(Module &M, std::string *fullName, std::string *dirName, std::string *baseName)
{
    std::string _fullName, _dirName, _baseName;
    _fullName = M.getModuleIdentifier();
    SmallVector< StringRef, 8 > vector;
    StringRef fullNameRef(_fullName);
    fullNameRef.split(vector, "/", -1, false);
    if (vector.size() > 1) {
        _baseName = vector.pop_back_val();
        for (unsigned i=0;i<vector.size();i++) {
            _dirName.append("/");
            _dirName.append(vector[i]);
        }
    }
    else {
        _baseName = _fullName;
        _dirName = "/";
    }
    vector.clear();
    StringRef baseNameRef(_baseName);
    baseNameRef.split(vector, ".", -1, false);
    if (vector.size() > 1) {
        _baseName = vector[0];
    }
    if (fullName)
        *fullName = _fullName;
    if (dirName)
        *dirName = _dirName;
    if (baseName)
        *baseName = _baseName;
}

inline unsigned long PassUtil::getTypeHash(TYPECONST Type* type, unsigned maxLevel)
{
    static std::vector<TYPECONST Type*> nestedTypes;
    static unsigned level = 0;
    static unsigned counter;
    unsigned long hash = 7;
    if(level == 0) {
        counter = 0;
    }
    unsigned numContainedTypes = type->getNumContainedTypes();
    unsigned nestedIndex = 0;
    for(unsigned i=0;i<nestedTypes.size();i++) {
        if(type == nestedTypes[i]) {
            nestedIndex = i+1;
            break;
        }
    }
    hash = (13*hash) ^ level;
    hash = (13*hash) ^ counter++;
    hash = (13*hash) ^ type->getTypeID();
    hash = (13*hash) ^ nestedIndex;
    if(TYPECONST IntegerType *intType = dyn_cast<IntegerType>(type)) {
        hash = (13*hash) ^ intType->getBitWidth();
    }
    else if(TYPECONST PointerType *ptrType = dyn_cast<PointerType>(type)) {
        hash = (13*hash) ^ ptrType->getElementType()->getTypeID();
    }
    if(nestedIndex > 0 || level >= maxLevel) {
        return hash;
    }
    if(numContainedTypes == 0) {
        return hash;
    }
    level++;
    nestedTypes.push_back(type);
    hash = (13*hash) ^ numContainedTypes;
    for(unsigned i=0;i<numContainedTypes;i++) {
        hash = (13*hash) ^ getTypeHash(type->getContainedType(i), maxLevel);
    }
    nestedTypes.pop_back();
    level--;

    return hash;
}

}

#endif /* _PASS_COMMON_H */
