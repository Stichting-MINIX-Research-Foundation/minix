#include <magic/MagicPass.h>

using namespace llvm;

PASS_COMMON_INIT_ONCE();

// command-line arguments
static cl::opt<std::string>
DLLFName("magic-dll-function",
    cl::desc("Specify the name of the deepest long-lived function whose stack "
        "needs to be instrumented"),
    cl::init(MAGIC_ENTRY_POINT), cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
LibPathRegex("magic-lib-path-regex",
    cl::desc("Specify all the colon-separated path regexes that identify directories containing "
        "libraries. Deprecated. Use -magic-ext-lib-sections instead."),
    cl::init(""), cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
VoidTypeAlias("magic-void-alias",
    cl::desc("Specify all the colon-separated type names that are to be treated as void, typically "
        "used in custom memory management implementations"),
    cl::init(""), cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
MMFuncPrefix("magic-mmfunc-prefix",
    cl::desc("Specify all the colon-separated prefixes that are to be used when extracting "
        "memory management functions used in custom memory management implementations"),
    cl::init(""), cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
MMFuncPair("magic-mmfunc-pair",
    cl::desc("Specify all the colon-separated pairs of malloc/free style memory management functions "
        "used in custom memory management implementations. Each function is to be listed together "
	"with a number indicating which of the input parameters is the one corresponding to its "
	"malloc(size)/free(pointer) counterpart. Example: "
	"\"my_smart_alloc/3;my_smart_free/3:my_custom_alloc/2;my_custom_free/1\". "
	"The counter for arguments starts from 1."),
    cl::init(""), cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
MMPoolFunc("magic-mm-poolfunc",
    cl::desc("Specify a pool memory management set of functions for creating pools, destroying pools, "
		"managing the pool buffers, reseting (reusing) pools and allocating memory blocks from the pool. "
		"All the functions are to be listed together with a number indicating which of the input parameters"
		"(numbering starts at 1) corresponds to the pool object. For the creation function, the pool object "
		"can be the return value (specify 0 for return value). The block allocation function additionally "
		"requires the number of the parameter denoting the size. Separate sets of functions using ':' and "
		"separate multiple functions of the same type using ';'. "
		"Example: \"my_pool_block_alloc/1/2:my_pool_create/0:my_pool_destroy/1:my_pool_alloc/1;"
		"another_pool_alloc/1;my_pool_free/1:my_pool_reset/1\"."
		"If there are no additional management functions, skip them. "
		"Example: \"pool_block_alloc/1/2:pool_create:pool_destroy/1\"."),
    cl::init(""), cl::NotHidden, cl::ValueRequired);

static cl::opt<bool>
EnablePoolMemReuse("magic-mpool-enable-reuse",
    cl::desc("Enable memory reuse across pools."),
    cl::init(false), cl::NotHidden);

static cl::opt<std::string>
MMAPCtlFunction("magic-mmap-ctlfunc",
    cl::desc("Specify all the colon-separated mmap control functions that change low-level properties"
        "of memory-mapped memory regions taking the start address as an argument"),
    cl::init(""), cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
MagicDataSections("magic-data-sections",
    cl::desc("Specify all the colon-separated magic data section regexes not to instrument"),
    cl::init("^" MAGIC_STATIC_VARS_SECTION_PREFIX ".*$:^" UNBL_SECTION_PREFIX ".*$"), cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
MagicFunctionSections("magic-function-sections",
    cl::desc("Specify all the colon-separated magic function section regexes not to instrument"),
    cl::init("^" MAGIC_STATIC_FUNCTIONS_SECTION ".*$:^" UNBL_SECTION_PREFIX ".*$"), cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
ExtLibSections("magic-ext-lib-sections",
    cl::desc("Specify all the colon-separated external lib section regexes"),
    cl::init(MAGIC_DEFAULT_EXT_LIB_SECTION_REGEX), cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
baseBuildDir("magic-base-build-dir",
    cl::desc("Specify the base build directory from which the pass derives relative directories for debug symbols"),
    cl::init(""), cl::NotHidden, cl::ValueRequired);

static cl::opt<bool>
EnableShadowing("magic-enable-shadowing",
    cl::desc("Enable state shadowing"),
    cl::init(false), cl::NotHidden);

static cl::opt<bool>
DisableMemFunctions("magic-disable-mem-functions",
    cl::desc("Disable hooking of memory functions"),
    cl::init(false), cl::NotHidden);

static cl::opt<bool>
DisableMallocSkip("magic-disable-malloc-skip",
    cl::desc("Disable ignoring malloc data variables"),
    cl::init(false), cl::NotHidden);

static cl::opt<bool>
SkipAll("magic-skip-all",
    cl::desc("Exit immediately"),
    cl::init(false), cl::NotHidden);

#if MAGIC_USE_QPROF_INSTRUMENTATION
QPROF_DECLARE_ALL_OPTS(magic,
    magicLLSitestacks,
    magicDeepestLLLoops,
    magicDeepestLLLibs,
    magicTaskClasses
);
#endif

#define DEBUG_TYPE_INFOS            0
#define DEBUG_FILL_TYPE_INFOS       0
#define DEBUG_FILL_EXT_TYPE_INFOS   0
#define DEBUG_ALLOC_LEVEL           0
#define DEBUG_ALLOC_BAD_TYPES       0
#define DEBUG_CASTS                 0
#define DEBUG_DUPLICATED_TYPE_INFOS 0
#define DEBUG_VALUE_SET             0
#define DEBUG_QPROF                 0

namespace llvm {

//===----------------------------------------------------------------------===//
// Constructors, destructor, and operators
//===----------------------------------------------------------------------===//

MagicPass::MagicPass() : ModulePass(ID) {}

unsigned TypeInfo::maxNameLength = 0;
unsigned TypeInfo::maxTypeStringLength = 0;
std::map<TYPECONST Type*, std::set<int> > TypeInfo::intCastTypes;
std::map<TYPECONST Type*, std::set<TYPECONST Type*> > TypeInfo::bitCastTypes;
std::map<TYPECONST Type*, std::set<TypeInfo*> > TypeInfo::typeMap;

bool SmartType::forceRawUnions = MAGIC_FORCE_RAW_UNIONS;
bool SmartType::forceRawBitfields = MAGIC_FORCE_RAW_BITFIELDS;

Function *MagicMemFunction::lastAllocWrapper = NULL;
std::map<std::string, Function*> MagicMemFunction::allocWrapperCache;
std::set<Function*> MagicMemFunction::customWrapperSet;

//===----------------------------------------------------------------------===//
// Public methods
//===----------------------------------------------------------------------===//

bool MagicPass::runOnModule(Module &M) {
    unsigned i;

    if (SkipAll) {
	return false;
    }

    magicPassLog("Running...");
    EDIType::setModule(&M);
    PassUtil::setModule(&M);

    // initialize qprof instrumentation
    qprofInstrumentationInit(M);

    //look up magic entry point function
    Function *magicEntryPointFunc = M.getFunction(MAGIC_ENTRY_POINT);
    if( !magicEntryPointFunc ){
        //if no valid entry point, we are not compiling a valid program, skip pass
        magicPassLog("Error: no " << MAGIC_ENTRY_POINT << "() found");
        return false;
    }

    //look up magic enabled variable
    GlobalVariable* magicEnabled = M.getNamedGlobal(MAGIC_ENABLED);
    if(!magicEnabled) {
        magicPassErr("Error: no " << MAGIC_ENABLED << " variable found");
        exit(1);
    }

    //look up magic root variable
    GlobalVariable* magicRootVar = M.getNamedGlobal(MAGIC_ROOT_VAR_NAME);
    if(!magicRootVar) {
        magicPassErr("Error: no " << MAGIC_ROOT_VAR_NAME << " variable found");
        exit(1);
    }

    //look up magic data init function and get the last instruction to add stuff in it
    Function *magicDataInitFunc = M.getFunction(MAGIC_DATA_INIT_FUNC_NAME);
    if(!magicDataInitFunc){
        magicPassErr("Error: no " << MAGIC_DATA_INIT_FUNC_NAME << "() found");
        exit(1);
    }
    Instruction *magicArrayBuildFuncInst = magicDataInitFunc->back().getTerminator();

    //look up pointer to magic memory instrumentation flag
    Value* magicNoMemInst = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_NO_MEM_INST);
    if(!magicNoMemInst) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_NO_MEM_INST << " field found");
        exit(1);
    }

    //look up pointer to magic array and magic struct type
    Value* magicArrayPtr = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_SENTRIES);
    if(!magicArrayPtr) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_SENTRIES << " field found");
        exit(1);
    }
    TYPECONST StructType* magicStructType = (TYPECONST StructType*) ((TYPECONST PointerType*)((TYPECONST PointerType*)magicArrayPtr->getType())->getElementType())->getElementType();

    //look up pointer to magic array size
    Value *magicArraySize = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_SENTRIES_NUM);
    if(!magicArraySize) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_SENTRIES_NUM << " field found");
        exit(1);
    }

    //look up pointer to magic array string size
    Value *magicArrayStrSize = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_SENTRIES_STR_NUM);
    if(!magicArrayStrSize) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_SENTRIES_STR_NUM << " field found");
        exit(1);
    }

    //look up pointer to magic next id
    Value *magicNextId = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_SENTRIES_NEXT_ID);
    if(!magicNextId) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_SENTRIES_NEXT_ID << " field found");
        exit(1);
    }

    //look up pointer to magic dsindex array and magic dsindex struct type
    Value* magicDsindexArrayPtr = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_DSINDEXES);
    if(!magicDsindexArrayPtr) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_DSINDEXES << " field found");
        exit(1);
    }
    TYPECONST StructType* magicDsindexStructType = (TYPECONST StructType*) ((TYPECONST PointerType*)((TYPECONST PointerType*)magicDsindexArrayPtr->getType())->getElementType())->getElementType();

    //look up pointer to magic dsindex array size
    Value *magicDsindexArraySize = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_DSINDEXES_NUM);
    if(!magicDsindexArraySize) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_DSINDEXES_NUM << " field found");
        exit(1);
    }

    //look up pointer to magic type array and magic type struct type
    Value *magicTypeArrayPtr = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_TYPES);
    if(!magicTypeArrayPtr) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_TYPES << " field found");
        exit(1);
    }
    TYPECONST StructType* magicTypeStructType = (TYPECONST StructType*) ((TYPECONST PointerType*)((TYPECONST PointerType*)magicTypeArrayPtr->getType())->getElementType())->getElementType();

    //look up pointer to magic type array size
    Value *magicTypeArraySize = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_TYPES_NUM);
    if(!magicTypeArraySize) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_TYPES_NUM << " field found");
        exit(1);
    }

    //look up pointer to magic type next id
    Value *magicTypeNextId = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_TYPES_NEXT_ID);
    if(!magicTypeNextId) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_TYPES_NEXT_ID << " field found");
        exit(1);
    }

    //look up pointer to magic function array and magic function struct type
    Value *magicFunctionArrayPtr = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_FUNCTIONS);
    if(!magicFunctionArrayPtr) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_FUNCTIONS << " field found");
        exit(1);
    }
    TYPECONST StructType* magicFunctionStructType = (TYPECONST StructType*) ((TYPECONST PointerType*)((TYPECONST PointerType*)magicFunctionArrayPtr->getType())->getElementType())->getElementType();

    //look up pointer to magic function array size
    Value *magicFunctionArraySize = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_FUNCTIONS_NUM);
    if(!magicFunctionArraySize) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_FUNCTIONS_NUM << " field found");
        exit(1);
    }

    //look up pointer to magic function next id
    Value *magicFunctionNextId = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_FUNCTIONS_NEXT_ID);
    if(!magicFunctionNextId) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_FUNCTIONS_NEXT_ID << " field found");
        exit(1);
    }

    //look up magic dsentry struct type 
    Value *magicFirstDsentyPtr = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_FIRST_DSENTRY);
    if(!magicFirstDsentyPtr) {
        magicPassErr("Error: no " << MAGIC_RSTRUCT_FIELD_FIRST_DSENTRY << " field found");
        exit(1);
    }
    TYPECONST StructType* magicDsentryStructType = (TYPECONST StructType*) ((TYPECONST PointerType*)((TYPECONST PointerType*)magicFirstDsentyPtr->getType())->getElementType())->getElementType();

    //look up magic init function
    Function *magicInitFunc = M.getFunction(MAGIC_INIT_FUNC_NAME);
    if( !magicInitFunc ){
        magicPassErr("Error: no " << MAGIC_INIT_FUNC_NAME << "() found");
        exit(1);
    }

    //look up magic dsentry stack functions
    Function *magicStackDsentryCreateFunc = M.getFunction(MAGIC_STACK_DSENTRIES_CREATE_FUNC_NAME);
    if (!magicStackDsentryCreateFunc) {
        magicPassErr("Error: no " << MAGIC_STACK_DSENTRIES_CREATE_FUNC_NAME << "() found");
        exit(1);
    }
    Function *magicStackDsentryDestroyFunc = M.getFunction(MAGIC_STACK_DSENTRIES_DESTROY_FUNC_NAME);
    if (!magicStackDsentryDestroyFunc) {
        magicPassErr("Error: no " << MAGIC_STACK_DSENTRIES_DESTROY_FUNC_NAME << "() found");
        exit(1);
    }

    //look up deepest long-lived function
    Function *deepestLLFunction = M.getFunction(DLLFName);
    if (!deepestLLFunction) {
        magicPassErr("Error: no " << DLLFName << "() found");
        exit(1);
    }

    //lookup magic get page size function
    Function *magicGetPageSizeFunc = M.getFunction(MAGIC_GET_PAGE_SIZE_FUNC_NAME);
    if(!magicGetPageSizeFunc){
        magicPassErr("Error: no " << MAGIC_GET_PAGE_SIZE_FUNC_NAME << "() found");
        exit(1);
    }

    //look up magic void pointer
    GlobalVariable *magicVoidPtr = M.getNamedGlobal(MAGIC_VOID_PTR_NAME);
    if(!magicVoidPtr) {
        magicPassErr("Error: no " << MAGIC_VOID_PTR_NAME << "variable found");
        exit(1);
    }
    assert(!isMagicGV(M, magicVoidPtr));

    //look up magic void array
    GlobalVariable *magicVoidArr = M.getNamedGlobal(MAGIC_VOID_ARRAY_NAME);
    if(!magicVoidArr) {
        magicPassErr("Error: no " << MAGIC_VOID_ARRAY_NAME << "variable found");
        exit(1);
    }
    assert(!isMagicGV(M, magicVoidArr));

    //look up magic void * type pointer
    GlobalVariable *magicVoidPtrTypePtr = M.getNamedGlobal(MAGIC_VOID_PTR_TYPE_PTR_NAME);
    if(!magicVoidPtrTypePtr) {
        magicPassErr("Error: no " << MAGIC_VOID_PTR_TYPE_PTR_NAME << "variable found");
        exit(1);
    }

    //determine lib path regexes
    PassUtil::parseStringListOpt(libPathRegexes, LibPathRegex);

    //determine void type aliases
    PassUtil::parseStringListOpt(voidTypeAliases, VoidTypeAlias);
    std::copy( voidTypeAliases.begin(), voidTypeAliases.end(), std::inserter( voidTypeAliasesSet, voidTypeAliasesSet.end() ) );

    //determine mm function prefixes
    PassUtil::parseStringListOpt(mmFuncPrefixes, MMFuncPrefix);

    //determine custom malloc/free style custom mm functions
    PassUtil::parseStringPairListOpt(mmFuncPairs, MMFuncPair);

    //determine the pool management sets of functions
    PassUtil::parseStringListOpt(mmPoolFunctions, MMPoolFunc);

    //determine mmap ctl functions
    PassUtil::parseStringListOpt(mmapCtlFunctions, MMAPCtlFunction);

    //determine magic data section regexes
    std::string DataSections = MagicDataSections;
    if (!DisableMallocSkip)
        DataSections += ":^" MAGIC_MALLOC_VARS_SECTION_PREFIX ".*$";
    PassUtil::parseRegexListOpt(magicDataSectionRegexes, DataSections);

    //determine magic function section regexes
    PassUtil::parseRegexListOpt(magicFunctionSectionRegexes, MagicFunctionSections);

    //determine magic ext lib section regexes
    PassUtil::parseRegexListOpt(extLibSectionRegexes, ExtLibSections);

    //look up inttoptr type casts
    Module::GlobalListType &globalList = M.getGlobalList();
    Module::FunctionListType &functionList = M.getFunctionList();
    std::vector<TYPECONST Type*> intCastTypes;
    std::vector<int> intCastValues;
    std::map<TYPECONST Type*, std::set<TYPECONST Type*> > bitCastMap;
    for (Module::iterator it = functionList.begin(); it != functionList.end(); ++it) {
        Function *F = it;
        if(isMagicFunction(M, F)) {
            continue;
        }
        for (inst_iterator I2 = inst_begin(F), E2 = inst_end(F); I2 != E2; ++I2) {
            indexCasts(M, &(*I2), intCastTypes, intCastValues, bitCastMap);
        }
    }
    for (Module::global_iterator it = globalList.begin(); it != globalList.end(); ++it) {
        GlobalVariable *GV = it;
        StringRef GVName = GV->getName();
        if(isMagicGV(M, GV) || GVName.startswith(".str") || GVName.startswith(".arr") || GVName.startswith("C.")) {
            continue;
        }
        if(GV->hasInitializer()) {
            indexCasts(M, GV->getInitializer(), intCastTypes, intCastValues, bitCastMap);
        }
    }

    //index and set cast maps
    std::map<TYPECONST Type*, std::set<int> > intCastMap;
    std::map<TYPECONST Type*, std::set<int> >::iterator intCastMapIt;
    for(i=0;i<intCastTypes.size();i++) {
        TYPECONST Type* type = intCastTypes[i];
        int value = intCastValues[i];
        intCastMapIt = intCastMap.find(type);
        if(intCastMapIt == intCastMap.end()) {
            std::set<int> valueSet;
            intCastMap.insert(std::pair<TYPECONST Type*, std::set<int> >(type, valueSet));
            intCastMapIt = intCastMap.find(type);
        }
        assert(intCastMapIt != intCastMap.end());
        std::set<int> *setPtr = &(intCastMapIt->second);
        if(setPtr->size() == 1 && *(setPtr->begin()) == 0) {
            continue;
        }
        if(value == 0) {
            setPtr->clear();
        }
        setPtr->insert(value);
    }
    TypeInfo::setIntCastTypes(intCastMap);
    TypeInfo::setBitCastTypes(bitCastMap);

#if MAGIC_INSTRUMENT_MEM_FUNCS
    std::vector<MagicMemFunction> magicMemFunctions;
    std::set<Function*> originalMagicMemFunctions;
    std::vector<MagicDebugFunction> magicDebugFunctions;
    std::vector<MagicMmapCtlFunction> magicMmapCtlFunctions;
    if (!DisableMemFunctions) {
        //look up magic memory functions and corresponding wrappers
        #define __X(P) #P
        std::string magicMemFuncNames[] = { MAGIC_MEM_FUNC_NAMES };
        std::string magicMemDeallocFuncNames[] = { MAGIC_MEMD_FUNC_NAMES };
        std::string magicMemNestedFuncNames[] = { MAGIC_MEMN_FUNC_NAMES };
        #undef __X
        int magicMemFuncAllocFlags[] = { MAGIC_MEM_FUNC_ALLOC_FLAGS };
        std::string magicMemPrefixes[] = { MAGIC_MEM_PREFIX_STRS };
        std::vector<std::string> llvmCallPrefixes;
        for (std::vector<std::string>::iterator it = mmFuncPrefixes.begin(); it != mmFuncPrefixes.end(); ++it) {
            llvmCallPrefixes.push_back(*it);
        }
        llvmCallPrefixes.push_back("");
        llvmCallPrefixes.push_back("\01"); //llvm uses odd prefixes for some functions, sometimes (e.g. mmap64)
        for(i=0;magicMemFuncNames[i].compare("");i++) {
            int allocFlags = magicMemFuncAllocFlags[i];
            for(unsigned j=0;j<llvmCallPrefixes.size();j++) {
                std::string fName = magicMemFuncNames[i];
                Function *f = M.getFunction(llvmCallPrefixes[j] + fName);
                if(!f) {
                    continue;
                }
                TYPECONST FunctionType *fType = f->getFunctionType();
                if(fType->getNumParams() == 0 && fType->isVarArg()) {
                    //missing function prototype, i.e. no realistic caller. Skip.
                    continue;
                }
                if(!fName.compare("brk")) {
                    brkFunctions.insert(f);
                }
                if(!fName.compare("sbrk")) {
                    sbrkFunctions.insert(f);
                }
                bool isDeallocFunction = false;
                for(unsigned k=0;magicMemDeallocFuncNames[k].compare("");k++) {
                    if(!magicMemDeallocFuncNames[k].compare(fName)) {
                        isDeallocFunction = true;
                        break;
                    }
                }
                bool makeNestedFunction = false;
                for(unsigned k=0;magicMemNestedFuncNames[k].compare("");k++) {
                    if (!magicMemNestedFuncNames[k].compare(fName)) {
                        makeNestedFunction = true;
                        break;
                    }
                }

                Function* w = findWrapper(M, magicMemPrefixes, f, fName);
                MagicMemFunction memFunction(M, f, w, isDeallocFunction, false, allocFlags);
                magicMemFunctions.push_back(memFunction);
                if (makeNestedFunction) {
                    w = findWrapper(M, magicMemPrefixes, f, MAGIC_NESTED_PREFIX_STR + fName);
                    MagicMemFunction memFunction(M, f, w, isDeallocFunction, true, allocFlags);
                    magicMemFunctions.push_back(memFunction);
                }
                originalMagicMemFunctions.insert(f);

#if DEBUG_ALLOC_LEVEL >= 1
                magicPassErr("Memory management function/wrapper found: " << f->getName() << "()/" << w->getName() << "()");
#endif
            }
        }

        //look up custom memory management functions and build the corresponding wrappers
        int stdAllocFlags = 0;
        Function *stdAllocFunc, *stdAllocWrapperFunc;
        stdAllocFunc = M.getFunction(MAGIC_MALLOC_FUNC_NAME);
        assert(stdAllocFunc && "Could not find the standard allocation function.");
        for(i=0;magicMemFuncNames[i].compare("");i++) {
            if (!magicMemFuncNames[i].compare(MAGIC_MALLOC_FUNC_NAME)) {
                stdAllocFlags = magicMemFuncAllocFlags[i];
                break;
            }
        }
        assert(magicMemFuncNames[i].compare("") && "Could not find the flags for the standard allocation function.");
        std::string wName;
        for(i=0;magicMemPrefixes[i].compare("");i++) {
            wName = magicMemPrefixes[i] + MAGIC_MALLOC_FUNC_NAME;
            stdAllocWrapperFunc = M.getFunction(wName);
            if (stdAllocWrapperFunc) {
                break;
            }
        }
        assert(stdAllocWrapperFunc && "Could not find a wrapper for the standard allocation function.");
        for (std::set<std::pair<std::string, std::string> >::iterator it = mmFuncPairs.begin(); it != mmFuncPairs.end(); ++it) {
	    std::vector<std::string> allocTokens;
	    PassUtil::parseStringListOpt(allocTokens, (*it).first, "/");
		    assert((allocTokens.size() == stdAllocFunc->getFunctionType()->getNumParams() + 1) && "Bad option format, format is: customFuncName/stdFuncArg1Mapping/.../stdFuncArgNMapping");

		    // build custom wrapper for the allocation function
		    Function *allocFunction = MagicUtil::getFunction(M, allocTokens[0]);
		    if (!allocFunction) {
			    continue;
		    }
		    std::vector<unsigned> allocArgMapping;
		    int param;
		    for (unsigned i = 0; i < stdAllocFunc->getFunctionType()->getNumParams(); i++) {
			    int ret = StringRef(allocTokens[i + 1]).getAsInteger(10, param);
			    assert(!ret && "Bad option format, format is: customFuncName/stdFuncArg1Mapping/.../stdFuncArgNMapping");
			    assert(param > 0 && "The numbering of function parameters starts from 1.");
			    allocArgMapping.push_back(param);
		    }
		    FunctionType *allocFuncType = getFunctionType(allocFunction->getFunctionType(), allocArgMapping);
		    if(!isCompatibleMagicMemFuncType(allocFuncType, stdAllocWrapperFunc->getFunctionType())) {
			    magicPassErr("Error: standard wrapper function " << stdAllocWrapperFunc->getName() << " has incompatible type.");
			    magicPassErr(TypeUtil::getDescription(allocFuncType, MAGIC_TYPE_STR_PRINT_MAX, MAGIC_TYPE_STR_PRINT_MAX_LEVEL) << " != " << TypeUtil::getDescription(stdAllocWrapperFunc->getFunctionType(), MAGIC_TYPE_STR_PRINT_MAX, MAGIC_TYPE_STR_PRINT_MAX_LEVEL));
			    exit(1);
		    }
		    Function *allocWrapper = MagicMemFunction::getCustomWrapper(allocFunction, stdAllocFunc, stdAllocWrapperFunc, allocArgMapping, false);

		    // register the wrapper
		    MagicMemFunction memFunctionAlloc(M, allocFunction, allocWrapper, false, false, stdAllocFlags);
		    magicMemFunctions.push_back(memFunctionAlloc);
		    originalMagicMemFunctions.insert(allocFunction);
#if DEBUG_ALLOC_LEVEL >= 1
            magicPassErr("Allocation function/custom wrapper added: " << allocFunction->getName() << "()/" << allocWrapper->getName() << "()");
#endif
        }

        //lookup memory pool management functions and add the corresponding wrapper calls
        int mempoolAllocFlags = MAGIC_STATE_HEAP;
	    Function *mempoolBlockAllocTemplate, *mempoolBlockAllocTemplateWrapper;
	    mempoolBlockAllocTemplate = MagicUtil::getFunction(M,  MAGIC_MEMPOOL_BLOCK_ALLOC_TEMPLATE_FUNC_NAME);
	    assert(mempoolBlockAllocTemplate && "Could not find the pool block allocation template function.");
	    for(i = 0; magicMemPrefixes[i].compare(""); i++) {
		    wName = magicMemPrefixes[i] + MAGIC_MEMPOOL_BLOCK_ALLOC_TEMPLATE_FUNC_NAME;
		    mempoolBlockAllocTemplateWrapper = MagicUtil::getFunction(M, wName);
		    if (mempoolBlockAllocTemplateWrapper) {
			    break;
		    }
	    }
	    assert(mempoolBlockAllocTemplateWrapper && "Could not find a wrapper for the pool block allocation template function.");
#define __X(P) #P
        // C++11 Initializer Lists are not yet supported as of Clang 3.0 ...
        std::pair<std::string, std::string> magicMempoolFuncNames[] = {
		    std::pair<std::string, std::string>(MAGIC_MEMPOOL_CREATE_FUNCS),
		    std::pair<std::string, std::string>(MAGIC_MEMPOOL_DESTROY_FUNCS),
		    std::pair<std::string, std::string>(MAGIC_MEMPOOL_MGMT_FUNCS),
		    std::pair<std::string, std::string>(MAGIC_MEMPOOL_RESET_FUNCS)
        };
#undef __X
        int magicMempoolFuncFlags[] = { MAGIC_MEMPOOL_FUNC_FLAGS };
        unsigned numMagicMempoolFuncPairs = sizeof(magicMempoolFuncNames) / sizeof(magicMempoolFuncNames[0]);
        std::vector<std::pair<Function*, Function*> > magicMempoolFuncs(numMagicMempoolFuncPairs, std::pair<Function*, Function*>());
        for (i = 0; i < numMagicMempoolFuncPairs; i++) {
	    magicMempoolFuncs[i].first = MagicUtil::getFunction(M, magicMempoolFuncNames[i].first);
	    if (!magicMempoolFuncs[i].first) {
			    magicPassErr("Could not find one of the memory pool wrapper functions: " + magicMempoolFuncNames[i].first);
			    exit(1);
		    }
	    magicMempoolFuncs[i].second = MagicUtil::getFunction(M, magicMempoolFuncNames[i].second);
	    if (!magicMempoolFuncs[i].second) {
			    magicPassErr("Could not find one of the memory pool wrapper functions: " + magicMempoolFuncNames[i].second);
			    exit(1);
		    }
        }

        if (mmPoolFunctions.size()) {
		    assert(mmPoolFunctions.size() >= 3 && mmPoolFunctions.size() <= 5 &&
						    "Specify at least 3 and at most 5 of the pool management types of functions: block alloc,pool create,pool destroy,pool management functions,pool reset functions.");
		    std::vector<std::string>::iterator mmPoolFuncsIt = mmPoolFunctions.begin();
		    std::vector<MagicMemFunction> mempoolMagicMemFunctions;

		    // memory pool block allocation functions
		    std::vector<std::string> mempoolBlockAllocFuncs;
		    PassUtil::parseStringListOpt(mempoolBlockAllocFuncs, *(mmPoolFuncsIt++), ";");

		    for (std::vector<std::string>::iterator funcIt = mempoolBlockAllocFuncs.begin(); funcIt != mempoolBlockAllocFuncs.end(); ++funcIt) {
			    std::vector<std::string> funcTokens;
			    PassUtil::parseStringListOpt(funcTokens, *funcIt, "/");
			    assert(funcTokens.size() == 3 && "Bad option format, format is: block_alloc_func/pool_ptr_arg_number/size_arg_number");
			    Function* blockAllocFunc = MagicUtil::getFunction(M, funcTokens[0]);
			    if (!blockAllocFunc) {
			        magicPassErr("Memory pool block allocation function not found - " + funcTokens[0] + ". Skipping instrumentation!");
			        mempoolMagicMemFunctions.clear();
			        break;
			    }
			    std::vector<unsigned> argMapping;
			    unsigned param;
			    for (unsigned i = 1; i < funcTokens.size(); i++) {
				    assert(!StringRef(funcTokens[i]).getAsInteger(10, param) && "Bad option format, format is: block_alloc_func/pool_ptr_arg_number/size_arg_number");
				    assert(param > 0 && param <= blockAllocFunc->getFunctionType()->getNumParams()
						    && "Bad option format. The function parameter number is not valid.");
				    argMapping.push_back(param);
			    }
			    FunctionType *blockAllocFuncType = getFunctionType(mempoolBlockAllocTemplate->getFunctionType(), argMapping);
			    if(!isCompatibleMagicMemFuncType(blockAllocFuncType, mempoolBlockAllocTemplateWrapper->getFunctionType())) {
				    magicPassErr("Error: standard wrapper function " << mempoolBlockAllocTemplateWrapper->getName() << " has incompatible type.");
				    magicPassErr(TypeUtil::getDescription(blockAllocFuncType, MAGIC_TYPE_STR_PRINT_MAX, MAGIC_TYPE_STR_PRINT_MAX_LEVEL) << " != " << TypeUtil::getDescription(mempoolBlockAllocTemplateWrapper->getFunctionType(), MAGIC_TYPE_STR_PRINT_MAX, MAGIC_TYPE_STR_PRINT_MAX_LEVEL));
				    exit(1);
			    }
			    Function *blockAllocWrapper = MagicMemFunction::getCustomWrapper(blockAllocFunc, mempoolBlockAllocTemplate, mempoolBlockAllocTemplateWrapper, argMapping, false);
			    MagicMemFunction memFunctionBlockAlloc(M, blockAllocFunc, blockAllocWrapper, false, false, mempoolAllocFlags);
			    mempoolMagicMemFunctions.push_back(memFunctionBlockAlloc);
		    }
		    if (!mempoolMagicMemFunctions.empty()) { // only if the block allocation functions have been successfully processed
			    // continue with the rest of the memory pool management functions, which do not require a magic wrapper
			    std::vector<std::vector<Function*> >::iterator magicMempoolFuncIt;
			    std::vector<std::vector<int> >::iterator magicMempoolFuncFlagsIt;
			    for (unsigned magicMempoolFuncIndex = 1; mmPoolFuncsIt != mmPoolFunctions.end(); ++mmPoolFuncsIt, ++magicMempoolFuncIndex) {
				    std::vector<std::string> mempoolMgmtFuncs;
				    PassUtil::parseStringListOpt(mempoolMgmtFuncs, *mmPoolFuncsIt, ";");
				    for (std::vector<std::string>::iterator funcIt = mempoolMgmtFuncs.begin(); funcIt != mempoolMgmtFuncs.end(); ++funcIt) {
					    std::vector<std::string> funcTokens;
					    PassUtil::parseStringListOpt(funcTokens, *funcIt, "/");
					    assert(funcTokens.size() == 2 && "Bad option format, format is: mempool_mgmt_func/pool_ptr_arg_number");
					    Function* mempoolMgmtFunc = MagicUtil::getFunction(M, funcTokens[0]);
					    assert(mempoolMgmtFunc && "Bad memory pool configuration, instrumentation aborted!");
					    std::vector<unsigned> argMapping;
					    unsigned param;
					    for (unsigned i = 1; i < funcTokens.size(); i++) {
						    assert(!StringRef(funcTokens[i]).getAsInteger(10, param) && "Bad option format, format is: mempool_mgmt_func/pool_ptr_arg_number");
						    assert(param <= mempoolMgmtFunc->getFunctionType()->getNumParams() &&
								    "Bad option format. The function parameter number is not valid.");
						    argMapping.push_back(param);
					    }
					    std::vector<Value*> trailingArgs;
					    if (magicMempoolFuncIndex == 1) { // pool create funcs
						    TYPECONST Type* poolType = mempoolMgmtFunc->getFunctionType()->getContainedType(argMapping[0]);
						    int level = MagicUtil::getPointerIndirectionLevel(poolType);
						    trailingArgs.push_back(ConstantInt::get(Type::getInt32Ty(M.getContext()), (level > 1)));
					    } else if (magicMempoolFuncIndex == 2) { // pool destroy funcs
						    trailingArgs.push_back(ConstantInt::get(Type::getInt32Ty(M.getContext()), (EnablePoolMemReuse ? 1 : 0)));
					    }
					    if (magicMempoolFuncFlags[magicMempoolFuncIndex - 1] & MAGIC_HOOK_DEBUG_MASK) {
						    MagicDebugFunction magicDebugFunction(mempoolMgmtFunc);
						    magicDebugFunction.addHooks(magicMempoolFuncs[magicMempoolFuncIndex - 1], magicMempoolFuncFlags[magicMempoolFuncIndex - 1], argMapping, trailingArgs);
						    magicDebugFunctions.push_back(magicDebugFunction);
					    } else {
						    bool ret = MagicDebugFunction::inlineHookCalls(mempoolMgmtFunc,
								    magicMempoolFuncs[magicMempoolFuncIndex - 1], magicMempoolFuncFlags[magicMempoolFuncIndex - 1], argMapping, trailingArgs);
						    if (!ret) {
							    magicPassErr("Unable to inline wrapper function calls for " + funcTokens[0]);
							    exit(1);
						    }
					    }
				    }
			    }
			    for (std::vector<MagicMemFunction>::iterator magicIt = mempoolMagicMemFunctions.begin(); magicIt != mempoolMagicMemFunctions.end(); ++magicIt) {
				    magicMemFunctions.push_back(*magicIt);
				    originalMagicMemFunctions.insert(magicIt->getFunction());
			    }
		    }
        }

        //lookup mmap ctl functions whose call arguments need to be fixed
        for (std::vector<std::string>::iterator it = mmapCtlFunctions.begin(); it != mmapCtlFunctions.end(); ++it) {
            std::vector<std::string> tokens;
            tokens.clear();
            PassUtil::parseStringListOpt(tokens, *it, "/");
            assert(tokens.size() == 3 && "Bad option format, format is: function/[ptr_arg_name]/[len_arg_name]");

            Function *function = M.getFunction(tokens[0]);
            if(!function) {
                continue;
            }
            std::string &ptrArgName = tokens[1];
            std::string &lenArgName = tokens[2];
            MagicMmapCtlFunction magicMmapCtlFunction(function, PointerType::get(IntegerType::get(M.getContext(), 8), 0), ptrArgName, lenArgName);
            magicMmapCtlFunctions.push_back(magicMmapCtlFunction);
        }
    }
#endif /*MAGIC_INSTRUMENT_MEM_FUNCS*/

    //everything as expected, set magic enabled variable to TRUE
    magicEnabled->setInitializer(ConstantInt::get(M.getContext(), APInt(32, 1)));

    //scan the list of global variables
    unsigned strGlobalVariables = 0;
    unsigned constGlobalVariables = 0;
    for (Module::global_iterator it = globalList.begin(); it != globalList.end(); ++it) {
        GlobalVariable *GV = it;
        StringRef GVName = GV->getName();
        TYPECONST Type *GVType = GV->getType()->getElementType();
        bool isPrimitiveOrPointerType = !GVType->isAggregateType();
        DATA_LAYOUT_TY DL = DATA_LAYOUT_TY(&M);
        bool isExternal = GV->hasExternalLinkage() || GV->hasExternalWeakLinkage();
        int typeSize = isExternal ? 0 : DL.getTypeSizeInBits(GVType)/8;
        int align = MAGIC_FORCE_ALIGN;

        if(isMagicGV(M, GV)) {
            magicPassLog("Skipping magic variable: " << GVName);
            continue;
        }
        assert(!MAGIC_STRINGREF_HAS_MAGIC_HIDDEN_PREFIX(GVName));
        if(GVName.startswith("C.")) {
            //LLVM code we are not interested in
            continue;
        }
        if(MagicUtil::isLocalConstant(M, GV)) {
            //Local constants we are not interested in
            continue;
        }
#if GLOBAL_VARS_IN_SECTION
        MagicUtil::setGlobalVariableSection(GV, GV->isConstant() ? GLOBAL_VARS_SECTION_RO : GLOBAL_VARS_SECTION_DATA);
#endif
        if(GVName.startswith(".str")) {
            assert(GV->hasInitializer());
#if LLVM_VERSION >= 31
            /* XXX Check. */
            ConstantDataArray *initializer = dyn_cast<ConstantDataArray>(GV->getInitializer());
#else
            ConstantArray *initializer = dyn_cast<ConstantArray>(GV->getInitializer());
#endif
            if(initializer) {
                assert(initializer->isString());
                MagicUtil::putStringRefCache(M, initializer->getAsString(), GV);
            }
            else {
                MagicUtil::putStringRefCache(M, "", GV);
            }

            strGlobalVariables++;
            Value *stringOwner = MagicUtil::getStringOwner(GV);
            if(stringOwner) {
                GlobalVariable *GVOwner = dyn_cast<GlobalVariable>(stringOwner);
                AllocaInst *AIOwner = dyn_cast<AllocaInst>(stringOwner);
                assert(GVOwner || AIOwner);
                bool stringOwnerFound = false;
                std::string ownerName;
                raw_string_ostream ostream(ownerName);
                if(GVOwner && !isMagicGV(M, GVOwner)) {
                    ostream << "#" << MagicUtil::getGVSourceName(M, GVOwner, NULL, baseBuildDir);
                    stringOwnerFound = true;
                }
                else if(AIOwner && !isMagicFunction(M, AIOwner->getParent()->getParent())) {
                    ostream << MagicUtil::getFunctionSourceName(M, AIOwner->getParent()->getParent(), NULL, baseBuildDir) << "#" << MagicUtil::getLVSourceName(M, AIOwner);
                    stringOwnerFound = true;
                }
                if(stringOwnerFound) {
                    ostream.flush();
                    stringOwnerMapIt = stringOwnerMap.find(ownerName);
                    if(stringOwnerMapIt == stringOwnerMap.end()) {
                        stringOwnerMap.insert(std::pair<std::string, GlobalVariable*>(ownerName, GV));
                        stringOwnerInvertedMap.insert(std::pair<GlobalVariable*, std::string>(GV, ownerName));
                    }
                    else {
                        stringOwnerInvertedMapIt = stringOwnerInvertedMap.find(stringOwnerMapIt->second);
                        if(stringOwnerInvertedMapIt != stringOwnerInvertedMap.end()) {
                            stringOwnerInvertedMap.erase(stringOwnerInvertedMapIt);
                        }
                    }
                }
            }
        }
        else if(GV->isConstant()) {
            constGlobalVariables++;
        }
        if(!isPrimitiveOrPointerType && align) {
            GV->setAlignment(align);
            if(typeSize % align) {
                typeSize = typeSize - (typeSize % align) + align;
            }
        }
        else if(MAGIC_OFF_BY_N_PROTECTION_N && GVType->isArrayTy() && typeSize>0) {
            unsigned alignment = typeSize + (DL.getTypeSizeInBits(GVType->getContainedType(0))/8) * MAGIC_OFF_BY_N_PROTECTION_N;
            unsigned a = 2;
            while(a < alignment) a = a << 1;
            GV->setAlignment(a);
        }
        globalVariableSizes.push_back(typeSize);
        globalVariables.push_back(GV);
        if(MagicUtil::hasAddressTaken(GV)) {
            globalVariablesWithAddressTaken.insert(GV);
        }
    }
    magicPassLog(">>>> Number of global variables found: " << globalVariables.size() << " of which " << strGlobalVariables << " .str variables, " << constGlobalVariables << " constants, and " << globalVariables.size()-strGlobalVariables-constGlobalVariables << " regular variables");

    //build the list of functions having their address taken (include the last function no matter what to get the function ranges right)
    std::vector<const SmartType *> functionTypes;
    std::vector<GlobalValue *> functionTypeParents;
    std::vector<TYPECONST FunctionType *> externalFunctionTypes;
    std::vector<GlobalValue *> externalFunctionTypeParents;
    for (Module::iterator it = functionList.begin(); it != functionList.end(); ++it) {
        Function *F = it;
        if(F->hasAddressTaken() || it == --functionList.end() || F->getName().startswith(MAGIC_EVAL_FUNC_PREFIX)) {
            if(isMagicFunction(M, F)) {
                continue;
            }
            functions.push_back(F);
            const SmartType *FSmartType = SmartType::getSmartTypeFromFunction(M, F);
            if(FSmartType && !FSmartType->isTypeConsistent()) {
                delete FSmartType;
                //pretend the function is external if an invalid type has been found.
                FSmartType = NULL;
            }
            if(!FSmartType) {
                externalFunctionTypes.push_back(F->getFunctionType());
                externalFunctionTypeParents.push_back(F);
            }
            else {
                functionTypes.push_back(FSmartType);
                functionTypeParents.push_back(F);
            }
        }
    }
    magicPassLog(">>>> Number of functions with address taken found: " << functions.size() << ", of which " << functionTypes.size() << " internal and " << externalFunctionTypes.size() << " external...");

    //build the list of global types
    std::vector<const SmartType *> smartTypes;
    std::vector<GlobalValue *> smartTypeParents;
    std::vector<TYPECONST Type *> externalTypes;
    std::vector<GlobalValue *> externalTypeParents;
    for(i=0;i<globalVariables.size();i++) {
        GlobalVariable *GV = globalVariables[i];
        TYPECONST Type* GVType = GV->getType()->getElementType();

        const SmartType *GVSmartType = SmartType::getSmartTypeFromGV(M, GV);

        if(!GV->hasAppendingLinkage()){
            // llvm.global_ctors and llvm.global_dtors have appending linkage, don't have compile unit debug info, and therefore cannot be linked to GV debug info, and so are skipped.
            if(!GVSmartType) {
		bool isExternal = GV->hasExternalLinkage() || GV->hasExternalWeakLinkage();
                if (!isExternal && !GV->isConstant()) {
                    magicPassErr("var is: " << GV->getName());
                    magicPassErr("type is: " << TypeUtil::getDescription(GV->getType()->getElementType(), MAGIC_TYPE_STR_PRINT_MAX, MAGIC_TYPE_STR_PRINT_MAX_LEVEL));
            	}
                assert(isExternal || GV->isConstant());
                externalTypes.push_back(GVType);
                externalTypeParents.push_back(GV);
            }
            else {
                smartTypes.push_back(GVSmartType);
                smartTypeParents.push_back(GV);
            }
        }
    }
    magicPassLog(">>>> Number of global types found: " << globalVariables.size() << ", of which " << smartTypes.size() << " internal and " << externalTypes.size() << " external...");

    //build type infos
    TypeInfo* magicVoidPtrTypeInfo = NULL;
    TypeInfo* magicVoidArrTypeInfo = NULL;
    TypeInfo* magicVoidTypeInfo = NULL;
    for(i=0;i<smartTypes.size();i++) {
        TypeInfo sourceTypeInfo(smartTypes[i]);
        sourceTypeInfo.addParent(smartTypeParents[i]);
        TypeInfo *aTypeInfo = fillTypeInfos(sourceTypeInfo, globalTypeInfos);
        if(smartTypeParents[i] == magicVoidPtr) {
            //get a pointer to void and void* types
            magicVoidPtrTypeInfo = aTypeInfo;
            assert(magicVoidPtrTypeInfo->getTypeID() == MAGIC_TYPE_POINTER);
            magicVoidTypeInfo = magicVoidPtrTypeInfo->getContainedType(0);
            assert(magicVoidTypeInfo->getTypeID() == MAGIC_TYPE_VOID);
        }
        else if(smartTypeParents[i] == magicVoidArr) {
            //get a pointer to void array types
            magicVoidArrTypeInfo = aTypeInfo;
            assert(magicVoidArrTypeInfo->getTypeID() == MAGIC_TYPE_ARRAY);
        }
    }
    assert(magicVoidPtrTypeInfo && magicVoidTypeInfo && magicVoidArrTypeInfo);
    std::vector<TypeInfo*> magicVoidTypeInfoArr;
    magicVoidTypeInfoArr.push_back(magicVoidTypeInfo);
    magicVoidArrTypeInfo->setContainedTypes(magicVoidTypeInfoArr);
    magicPassLog(">>>> Number of types found: " << globalTypeInfos.size());
    for(i=0;i<functionTypes.size();i++) {
        TypeInfo sourceTypeInfo(functionTypes[i]);
        sourceTypeInfo.addParent(functionTypeParents[i]);
        fillTypeInfos(sourceTypeInfo, globalTypeInfos);
    }
    magicPassLog(">>>> Number of types + function types found: " << globalTypeInfos.size());

    //add external function types
    for(i=0;i<externalFunctionTypes.size();i++) {
        TypeInfo sourceTypeInfo(externalFunctionTypes[i]);
        sourceTypeInfo.addParent(externalFunctionTypeParents[i]);
        fillTypeInfos(sourceTypeInfo, globalTypeInfos);
    }
    magicPassLog(">>>> Number of types + function types + external function types found: " << globalTypeInfos.size());

    //add external variable types
    for(i=0;i<externalTypes.size();i++) {
        TypeInfo* aTypeInfo = fillExternalTypeInfos(externalTypes[i], externalTypeParents[i], globalTypeInfos);
        if(aTypeInfo == NULL) {
            magicPassErr("var is: " << externalTypeParents[i]->getName());
            magicPassErr("type is: " << TypeUtil::getDescription(externalTypes[i], MAGIC_TYPE_STR_PRINT_MAX, MAGIC_TYPE_STR_PRINT_MAX_LEVEL));
        }
        assert(aTypeInfo != NULL && "External type not supported!");
    }
    magicPassLog(">>>> Number of types + external types + function types + external function types found: " << globalTypeInfos.size());

    //process types, split them when some parent has a valid value set
    std::vector<TypeInfo*> splitTypeInfos;
    for(i=0;i<globalTypeInfos.size();i++) {
        bool isTypeInfoSplit = globalTypeInfos[i]->splitByParentValueSet(splitTypeInfos, globalVariablesWithAddressTaken);
        if(isTypeInfoSplit) {
#if DEBUG_VALUE_SET
            unsigned splitTypeInfosSize = splitTypeInfos.size();
            errs() << "MagicPass: Found type info split with different parents and value sets: original type is: " << globalTypeInfos[i]->getDescription() << ", type splits are:\n";
            for(unsigned j=splitTypeInfosSize;j<splitTypeInfos.size();j++) {
                errs() << " - value set is: [ ";
                std::vector<int> valueSet = splitTypeInfos[j]->getValueSet();
                for(unsigned k=1;k<valueSet.size();k++) {
                    errs() << (k==1 ? "" : ", ") << valueSet[k];
                }
                errs() << " ], parents are: [ ";
                std::vector<GlobalValue*> parents = splitTypeInfos[j]->getParents();
                for(unsigned k=0;k<parents.size();k++) {
                    errs() << (k==0 ? "" : ", ") << parents[k]->getName();
                }
                errs() << " ]\n";
            }
#endif
        }
    }

    //index type parents
    globalTypeInfos.clear();
    for(i=0;i<splitTypeInfos.size();i++) {
        TypeInfo *aTypeInfo = splitTypeInfos[i];
        std::vector<GlobalValue*> parents = aTypeInfo->getParents();
        for(unsigned j=0;j<parents.size();j++) {
            parentMapIt = globalParentMap.find(parents[j]);
            assert(parentMapIt == globalParentMap.end());
            globalParentMap.insert(std::pair<GlobalValue*, TypeInfo*>(parents[j], aTypeInfo));
        }
        globalTypeInfos.push_back(aTypeInfo);
    }

    std::vector< TypeInfo* > magicDsindexTypeInfoList;
    std::vector< std::pair<std::string,std::string> > magicDsindexNamesList;
    std::vector<int> magicDsindexFlagsList;

#if MAGIC_INSTRUMENT_MEM_FUNCS
    std::vector<MagicMemFunction> magicMemFunctionCalls;
    if (!DisableMemFunctions) {
        //gather magic memory function calls to replace and figure out the type (adding more (local) types if needed)
        std::map< std::pair<std::string,std::string>, int> namesMap;
        int allocFlags;
        std::set<Function*> extendedMagicMemFunctions;
        for (std::set<Function*>::iterator it = originalMagicMemFunctions.begin(); it != originalMagicMemFunctions.end(); ++it) {
            PassUtil::getFunctionsInDirectBUCallgraph(*it, extendedMagicMemFunctions);
        }
        while(!magicMemFunctions.empty()) {
            MagicMemFunction magicMemFunction = magicMemFunctions.front();
            magicMemFunctions.erase(magicMemFunctions.begin());
            std::vector<User*> Users(magicMemFunction.getFunction()->user_begin(), magicMemFunction.getFunction()->user_end());
            std::vector<Value*> EqPointers;
            while (!Users.empty()) {
              int annotation;
              User *U = Users.back();
              Users.pop_back();

              if (Instruction *I = dyn_cast<Instruction>(U)) {
                Function *parent = I->getParent()->getParent();
                if (isMagicFunction(M, parent) || MagicMemFunction::isCustomWrapper(parent)) {
                    continue;
                }
                CallSite CS = MagicUtil::getCallSiteFromInstruction(I);
                if (CS.getInstruction() &&
                    (!CS.arg_empty() || magicMemFunction.getWrapper() == NULL) &&
                    (MagicUtil::getCalledFunctionFromCS(CS) == magicMemFunction.getFunction() ||
                     std::find(EqPointers.begin(), EqPointers.end(),
                               CS.getCalledValue()) != EqPointers.end())) {
                  bool isDeallocFunction = magicMemFunction.isDeallocFunction();
                  bool wrapParent = false;
                  bool isNested = false;
                  TypeInfo *typeInfo = magicVoidTypeInfo;
                  std::string allocName = "";
                  std::string allocParentName = "";
                  //check if we have to skip
                  //if this call site is only called from some predefined mem function, it is nested
                  //some function wrappers are for such nested calls, some are not. this must match.
                  isNested = (extendedMagicMemFunctions.find(CS.getInstruction()->getParent()->getParent()) != extendedMagicMemFunctions.end());
                  if (isNested != magicMemFunction.isNestedFunction()) {
                      continue;
                  }
                  if(sbrkFunctions.find(MagicUtil::getCalledFunctionFromCS(CS)) != sbrkFunctions.end()) {
                      ConstantInt *arg = dyn_cast<ConstantInt>(CS.getArgument(0));
                      if(arg && arg->getZExtValue() == 0) {
                          //ignore sbrk(0) calls. This does not skip calls with a variable argument (when arg == NULL)
#if DEBUG_ALLOC_LEVEL >= 1
                          magicPassErr("Skipping instrumentation of sbrk(0) MM call found in " << parent->getName() << "():");
                          I->print(errs());
                          errs() << "\n";
#endif
                          continue;
                      }
                  }
                  else if(MagicUtil::getCallAnnotation(M, CS, &annotation)
                      && annotation == MAGIC_CALL_MEM_SKIP_INSTRUMENTATION) {
                      //ignore calls we are supposed to skip
#if DEBUG_ALLOC_LEVEL >= 1
                      magicPassErr("Skipping instrumentation of annotated MM call found in " << parent->getName() << "():");
                      I->print(errs());
                      errs() << "\n";
#endif
                      continue;
                  }
                  //figure out the type and the names
                  if(!isDeallocFunction && !isNested) {
                      int allocCounter = 1;
                      int ret;
                      std::map< std::pair<std::string,std::string>, int>::iterator namesMapIt;
                      //get alloc types and names
                      TypeInfo *allocTypeInfo = getAllocTypeInfo(M, magicVoidPtrTypeInfo, CS, allocName, allocParentName);
#if !MAGIC_INSTRUMENT_MEM_CUSTOM_WRAPPERS
                      if(!allocTypeInfo) {
                          allocTypeInfo = voidTypeInfo;
                      }
#endif
                      if(allocTypeInfo) {
                          typeInfo = allocTypeInfo;
                      }
                      else {
                          int pointerParam = MagicMemFunction::getMemFunctionPointerParam(I->getParent()->getParent(), brkFunctions, magicVoidPtrTypeInfo);
                          if(pointerParam >= 0 /* && !I->getParent()->getParent()->hasAddressTaken() */) {
                              //the parent is a valid magic mem function to wrap
                              wrapParent = true;
                          }
                      }
                      if(!wrapParent) {
                          assert(allocParentName.compare("") && "Empty parent name!");
                          if(!allocName.compare("")) {
                             allocName = MAGIC_ALLOC_NONAME;
                          }

#if (MAGIC_NAMED_ALLOC_USE_DBG_INFO || (MAGIC_MEM_USAGE_OUTPUT_CTL == 1))
                          //extend names with debug information when requested
                          if (MDNode *N = I->getMetadata("dbg")) {
                             DILocation Loc(N);
                             std::string string;
                             raw_string_ostream ostream(string);
                             ostream << allocName << MAGIC_ALLOC_NAME_SEP << Loc.getFilename() << MAGIC_ALLOC_NAME_SEP << Loc.getLineNumber();
                             ostream.flush();
                             allocName = string;
                          }
#endif

#if MAGIC_FORCE_ALLOC_EXT_NAMES
                          if (isExtLibrary(parent, NULL)) {
                             allocName = MAGIC_ALLOC_EXT_NAME;
                             allocName = MAGIC_ALLOC_EXT_PARENT_NAME;
                          }
    #endif

                          //avoid duplicates
                          namesMapIt = namesMap.find(std::pair<std::string, std::string>(allocParentName, allocName));
                          if(namesMapIt != namesMap.end()) {
                             allocCounter = namesMapIt->second + 1;
                             ret = namesMap.erase(std::pair<std::string, std::string>(allocParentName, allocName));
                             assert(ret == 1);
                             namesMap.insert(std::pair<std::pair<std::string, std::string>, int>(std::pair<std::string, std::string>(allocParentName, allocName), allocCounter));
                             std::string string;
                             raw_string_ostream ostream(string);
                             ostream << allocName << MAGIC_ALLOC_NAME_SUFFIX << allocCounter;
                             ostream.flush();
                             allocName = string;
                          }
                          else {
                             namesMap.insert(std::pair<std::pair<std::string, std::string>, int>(std::pair<std::string, std::string>(allocParentName, allocName), allocCounter));
                             allocName += MAGIC_ALLOC_NAME_SUFFIX;
                          }
                          magicMemFunction.setInstructionTypeInfo(typeInfo, allocName, allocParentName);
                          //add dsindex entries
                          magicDsindexTypeInfoList.push_back(typeInfo);
                          magicDsindexNamesList.push_back(std::pair<std::string, std::string>(allocParentName, allocName));
                          allocFlags = magicMemFunction.getAllocFlags();
                          assert(allocFlags);
                          magicDsindexFlagsList.push_back(allocFlags);
                      }
                  }
                  magicMemFunction.setInstruction(I);
                  Function *instructionParent = I->getParent()->getParent();
                  //see if we can find the parent in our lists
                  MagicMemFunction *magicMemParent = NULL;
                  for(unsigned k=0;k<magicMemFunctions.size();k++) {
                      if(magicMemFunctions[k].getFunction() == instructionParent) {
                          magicMemParent = &magicMemFunctions[k];
                          break;
                      }
                  }
                  if(!magicMemParent) {
                      for(unsigned k=0;k<magicMemFunctionCalls.size();k++) {
                          if(magicMemFunctionCalls[k].getFunction() == instructionParent) {
                              magicMemParent = &magicMemFunctionCalls[k];
                              break;
                          }
                      }
                  }
                  if(!magicMemParent && wrapParent) {
                      //if there is no existing parent but we have to wrap the parent, create a parent now and add it to the function queue
                      assert(!isNested);
                      MagicMemFunction newMagicMemFunction(M, instructionParent, NULL, false, false, 0);
                      magicMemFunctions.push_back(newMagicMemFunction);
                      magicMemParent = &magicMemFunctions[magicMemFunctions.size()-1];
                  }
                  if(magicMemParent) {
                      //if we have a parent, add a dependency
                      magicMemParent->addInstructionDep(magicMemFunction);
                      assert(magicMemParent->getAllocFlags());
                  }
                  else {
                      //if there is no parent, add it to the call queue
                      magicMemFunctionCalls.push_back(magicMemFunction);
                  }
                }
              } else if (GlobalValue *GV = dyn_cast<GlobalValue>(U)) {
                Users.insert(Users.end(), GV->user_begin(), GV->user_end());
                EqPointers.push_back(GV);
              } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(U)) {
                if (CE->isCast()) {
                  Users.insert(Users.end(), CE->user_begin(), CE->user_end());
                  EqPointers.push_back(CE);
                }
              }
            }
        }
    }
#endif /*MAGIC_INSTRUMENT_MEM_FUNCS*/

#if MAGIC_INSTRUMENT_STACK
    std::vector<std::map<AllocaInst*, std::pair<TypeInfo*, std::string> > > localTypeInfoMaps;
    std::map<AllocaInst*, std::pair<TypeInfo*, std::string> > localTypeInfoMap;
    std::vector<Function*> stackIntrumentedFuncs;
    fillStackInstrumentedFunctions(stackIntrumentedFuncs, deepestLLFunction);
    std::string stackIntrumentedFuncsStr;
    for(i=0;i<stackIntrumentedFuncs.size();i++) {
        localTypeInfoMap.clear();
        indexLocalTypeInfos(M, stackIntrumentedFuncs[i], localTypeInfoMap);
        localTypeInfoMaps.push_back(localTypeInfoMap);
        stackIntrumentedFuncsStr += (i==0 ? "" : ", ") + stackIntrumentedFuncs[i]->getName().str() + "()";
    }
    magicPassLog(">>>> Set of stack-instrumented functions expanded from function " << deepestLLFunction->getName() << "(): " << stackIntrumentedFuncsStr);
    magicPassLog(">>>> Number of types + external types + function types + external function types + local types found: " << globalTypeInfos.size());
#endif

    //add raw types
    std::vector<TypeInfo*> rawTypeInfos;
    for(i=0;i<globalTypeInfos.size();i++) {
        TypeInfo* aTypeInfo = globalTypeInfos[i];
        if(!aTypeInfo->hasRawTypeRepresentation()) {
            continue;
        }
        assert(aTypeInfo->getNumContainedTypes() == 0);
        TypeInfo* aRawTypeInfo = new TypeInfo(*magicVoidArrTypeInfo);
        aRawTypeInfo->setPersistent();
        aRawTypeInfo->removeAllParents();
        rawTypeInfos.push_back(aRawTypeInfo);
        std::vector<TypeInfo*> aTypeInfoContainedTypes;
        aTypeInfoContainedTypes.push_back(aRawTypeInfo);
        aTypeInfo->setContainedTypes(aTypeInfoContainedTypes);
        assert(aTypeInfo->getContainedType(0)->getContainedType(0) == magicVoidTypeInfo);
    }
    for(i=0;i<rawTypeInfos.size();i++) {
        globalTypeInfos.push_back(rawTypeInfos[i]);
        assert(rawTypeInfos[i]->getNumContainedTypes() == 1);
    }
    magicPassLog(">>>> Number of types + external types + function types + external function types + local types found + raw types: " << globalTypeInfos.size());

    //find max recursive sequence length
    unsigned length, maxRecursiveSequenceLength = 0;
    for(i=0;i<globalTypeInfos.size();i++) {
        if(globalTypeInfos[i]->getParents().size() > 0) {
            length = getMaxRecursiveSequenceLength(globalTypeInfos[i]);
            if(length > maxRecursiveSequenceLength) {
                maxRecursiveSequenceLength = length;
            }
        }
    }
    magicPassLog(">>>> Max recursive sequence length: " << maxRecursiveSequenceLength);

    //debug type infos when needed
#if DEBUG_TYPE_INFOS
        for(i=0;i<globalTypeInfos.size();i++) {
            std::vector<GlobalValue*> parents = globalTypeInfos[i]->getParents();
            if(parents.size() > 0) {
                std::string parentString, typeString;
                for(unsigned j=0;j<parents.size();j++) {
                    parentString.append((j>0 ? ", " : "") + parents[j]->getName().str());
                }
                typeString = globalTypeInfos[i]->getDescription();
                magicPassErr("     Global type group found, parents=( " << parentString << "), type=" << typeString << ", name=" << globalTypeInfos[i]->getName() << ", names_string=" << globalTypeInfos[i]->getNamesString());
                if(DEBUG_TYPE_INFOS >= 2) {
                    printInterestingTypes(globalTypeInfos[i]);
                }
            }
        }
        for(i=0;i<globalTypeInfos.size();i++) {
            std::string name = globalTypeInfos[i]->getName();
            if(name.compare("")) {
                magicPassErr("     Named type found: " << name << " (names string: " << globalTypeInfos[i]->getNamesString() << ", id: " << i << ")");
            }
        }
#endif

#if DEBUG_DUPLICATED_TYPE_INFOS
    std::map<std::string, TypeInfo*> duplicatedTypeInfoMap;
    std::map<std::string, TypeInfo*>::iterator duplicatedTypeInfoMapIt;
    for(i=0;i<globalTypeInfos.size();i++) {
        if(globalTypeInfos[i]->getType()->isStructTy()) {
            std::string name = globalTypeInfos[i]->getName();
            if(!name.compare("")) {
                continue;
            }
            duplicatedTypeInfoMapIt = duplicatedTypeInfoMap.find(name);
            if(duplicatedTypeInfoMapIt != duplicatedTypeInfoMap.end()) {
                magicPassErr("Duplicated struct name found: " << name << ": " << globalTypeInfos[i]->getVerboseDescription() << " != " << (duplicatedTypeInfoMapIt->second)->getVerboseDescription());
            }
            else {
                duplicatedTypeInfoMap.insert(std::pair<std::string, TypeInfo*>(name, globalTypeInfos[i]));
            }
        }
    }
#endif

    //allocate magic type array
    ArrayType* magicTypeArrayType = ArrayType::get(magicTypeStructType, globalTypeInfos.size());
    magicTypeArray = new GlobalVariable(M, magicTypeArrayType, false, GlobalValue::InternalLinkage, ConstantAggregateZero::get(magicTypeArrayType), MAGIC_TYPE_ARRAY_NAME);
    MagicUtil::setGlobalVariableSection(magicTypeArray, MAGIC_STATIC_VARS_SECTION_DATA);

    //allocate magic array
    ArrayType* magicArrayType = ArrayType::get(magicStructType, globalVariables.size());
    magicArray = new GlobalVariable(M, magicArrayType, false, GlobalValue::InternalLinkage, ConstantAggregateZero::get(magicArrayType), MAGIC_ARRAY_NAME);
    MagicUtil::setGlobalVariableSection(magicArray, MAGIC_STATIC_VARS_SECTION_DATA);

    //allocate magic function array
    ArrayType* magicFunctionArrayType = ArrayType::get(magicFunctionStructType, functions.size());
    magicFunctionArray = new GlobalVariable(M, magicFunctionArrayType, false, GlobalValue::InternalLinkage, ConstantAggregateZero::get(magicFunctionArrayType), MAGIC_FUNC_ARRAY_NAME);
    MagicUtil::setGlobalVariableSection(magicFunctionArray, MAGIC_STATIC_VARS_SECTION_DATA);

    //build magic type array in build function
    i=0;
    std::map<TypeInfo*, Constant*> magicArrayTypePtrMap;
    std::map<TypeInfo*, Constant*>::iterator magicArrayTypePtrMapIt;
    std::map<TypeInfo*, unsigned> magicArrayTypeIndexMap;
    std::map<TypeInfo*, unsigned>::iterator magicArrayTypeIndexMapIt;
    std::vector<Value*> arrayIndexes;
    for(;i<globalTypeInfos.size();i++) {
        TypeInfo* aTypeInfo = globalTypeInfos[i];
        TYPECONST Type *aType = aTypeInfo->getType();
        arrayIndexes.clear();
        arrayIndexes.push_back(ConstantInt::get(M.getContext(), APInt(64, 0, 10)));     //pointer to A[]
        arrayIndexes.push_back(ConstantInt::get(M.getContext(), APInt(64, i, 10))); //pointer to A[index]
        Constant* magicTypeArrayPtr = MagicUtil::getGetElementPtrConstant(magicTypeArray, arrayIndexes);
        magicArrayTypePtrMap.insert(std::pair<TypeInfo*, Constant*>(aTypeInfo, magicTypeArrayPtr));
        magicArrayTypeIndexMap.insert(std::pair<TypeInfo*, unsigned>(aTypeInfo, i));

        //storing id field
        Value* structIdField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_ID);
        Constant* idValue = ConstantInt::get(M.getContext(), APInt(32, i+1, 10));
        new StoreInst(idValue, structIdField, false, magicArrayBuildFuncInst);

        //storing name field
        Value* structNameField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_NAME);
        Constant* nameValue = MagicUtil::getArrayPtr(M, MagicUtil::getStringRef(M, aTypeInfo->getName()));
        new StoreInst(nameValue, structNameField, false, magicArrayBuildFuncInst);

        //storing names field
        std::vector<std::string> names = aTypeInfo->getNames();
        Value* structNamesField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_NAMES);
        Constant* namesValue;
        if(names.size() > 0) {
            namesValue = MagicUtil::getArrayPtr(M, MagicUtil::getStringArrayRef(M, names.size(), &names));
        }
        else {
            namesValue = ConstantPointerNull::get((TYPECONST PointerType*) ((TYPECONST PointerType*)structNamesField->getType())->getElementType());
        }
        new StoreInst(namesValue, structNamesField, false, magicArrayBuildFuncInst);

        //storing num_names field
        Value* structNumNamesField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_NUM_NAMES);
        Constant* numNamesValue = ConstantInt::get(M.getContext(), APInt(32, names.size(), 10));
        new StoreInst(numNamesValue, structNumNamesField, false, magicArrayBuildFuncInst);

        //storing type_str field
        Value* structTypeStrField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_TYPE_STR);
        Constant* typeStrValue = MagicUtil::getArrayPtr(M, MagicUtil::getStringRef(M, aTypeInfo->getTypeString()));
        new StoreInst(typeStrValue, structTypeStrField, false, magicArrayBuildFuncInst);

        //filling size field
        Value* structSizeField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_SIZE);
        Value* typeSizeValue;
        if(aType->isFunctionTy() || TypeUtil::isOpaqueTy(aType) || aType->isVoidTy()) {
            typeSizeValue = ConstantInt::get(M.getContext(), APInt(32, 1, 10));
        }
        else {
            assert(aType->isSized());
            typeSizeValue = ConstantExpr::getIntegerCast(ConstantExpr::getSizeOf(aType), (TYPECONST IntegerType*)((TYPECONST PointerType*)structSizeField->getType())->getElementType(), true);
        }
        new StoreInst(typeSizeValue, structSizeField, false, magicArrayBuildFuncInst);

        //storing num_child_types field
        Value* structNumChildTypesField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_NUM_CHILD_TYPES);
        Constant* numChildTypesValue = ConstantInt::get(M.getContext(), APInt(32, aTypeInfo->getNumChildTypes(), 10));
        new StoreInst(numChildTypesValue, structNumChildTypesField, false, magicArrayBuildFuncInst);

        //storing member_names field
        std::vector<std::string> memberNames = aTypeInfo->getMemberNames();
        Value* structMemberNamesField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_MEMBER_NAMES);
        Constant* memberNamesValue;
        if(memberNames.size() > 0) {
            assert(aType->isStructTy());
            assert(aTypeInfo->getNumContainedTypes() == memberNames.size());
            memberNamesValue = MagicUtil::getArrayPtr(M, MagicUtil::getStringArrayRef(M, memberNames.size(), &memberNames));
        }
        else {
            memberNamesValue = ConstantPointerNull::get((TYPECONST PointerType*) ((TYPECONST PointerType*)structMemberNamesField->getType())->getElementType());
        }
        new StoreInst(memberNamesValue, structMemberNamesField, false, magicArrayBuildFuncInst);

        //storing member_offsets field
        Value* structMemberOffsetsField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_MEMBER_OFFSETS);
        Constant* memberOffsetsValue;
        if(memberNames.size() > 0) {
            assert(aType->isStructTy());
            assert(aTypeInfo->getNumContainedTypes() == aTypeInfo->getNumChildTypes());
            bool isConstant = false;
            GlobalVariable *memberOffsetArray = MagicUtil::getIntArrayRef(M, aTypeInfo->getNumChildTypes(), NULL, isConstant);
            for(unsigned j=0;j<aTypeInfo->getNumChildTypes();j++) {
                std::vector<Value*> arrayIndexes;
                arrayIndexes.clear();
                arrayIndexes.push_back(ConstantInt::get(M.getContext(), APInt(64, 0, 10))); //pointer to A[]
                arrayIndexes.push_back(ConstantInt::get(M.getContext(), APInt(64, j, 10))); //pointer to A[j]
                Constant* memberOffsetArrayPtr = MagicUtil::getGetElementPtrConstant(memberOffsetArray, arrayIndexes);

                Value* memberOffsetValue = ConstantExpr::getIntegerCast(ConstantExpr::getOffsetOf((TYPECONST StructType*) aTypeInfo->getType(), j), (TYPECONST IntegerType*)((TYPECONST PointerType*)memberOffsetArrayPtr->getType())->getElementType(), true);
                new StoreInst(memberOffsetValue, memberOffsetArrayPtr, false, magicArrayBuildFuncInst);
            }
            memberOffsetsValue = MagicUtil::getArrayPtr(M, memberOffsetArray);
        }
        else {
            memberOffsetsValue = ConstantPointerNull::get((TYPECONST PointerType*) ((TYPECONST PointerType*)structMemberOffsetsField->getType())->getElementType());
        }
        new StoreInst(memberOffsetsValue, structMemberOffsetsField, false, magicArrayBuildFuncInst);

        //storing value set field (for enum values and value set analysis)
        std::vector<int> valueSet = aTypeInfo->getValueSet();
        Value* structValueSetField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_VALUE_SET);
        Constant* valueSetValue;
        if(valueSet.size() > 0) {
            valueSetValue = ConstantExpr::getCast(Instruction::BitCast, MagicUtil::getArrayPtr(M, MagicUtil::getIntArrayRef(M, valueSet.size(), &valueSet)), magicVoidPtrTypeInfo->getType());
        }
        else {
            valueSetValue = ConstantPointerNull::get((TYPECONST PointerType*)magicVoidPtrTypeInfo->getType());
        }
        new StoreInst(valueSetValue, structValueSetField, false, magicArrayBuildFuncInst);
        
        //storing type_id field
        Value* structTypeIDField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_TYPE_ID);
        Constant* typeIDValue = ConstantInt::get(M.getContext(), APInt(32, aTypeInfo->getTypeID(), 10));
        new StoreInst(typeIDValue, structTypeIDField, false, magicArrayBuildFuncInst);

        //storing flags field
        Value* structFlagsField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_FLAGS);
        Constant* flagsValue = ConstantInt::get(M.getContext(), APInt(32, aTypeInfo->getFlags(), 10));
        new StoreInst(flagsValue, structFlagsField, false, magicArrayBuildFuncInst);

        //storing bit_width field
        Value* structBitWidthField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_BIT_WIDTH);
        Constant* bitWidthValue = ConstantInt::get(M.getContext(), APInt(32, aTypeInfo->getBitWidth(), 10));
        new StoreInst(bitWidthValue, structBitWidthField, false, magicArrayBuildFuncInst);
    }

    i=0;
    //build contained types pointers
    unsigned dstIndex, voidTypeIndex;
    magicArrayTypeIndexMapIt = magicArrayTypeIndexMap.find(magicVoidTypeInfo);
    assert(magicArrayTypeIndexMapIt != magicArrayTypeIndexMap.end());
    voidTypeIndex = magicArrayTypeIndexMapIt->second;
    for(;i<globalTypeInfos.size();i++) {
        TypeInfo* aTypeInfo = globalTypeInfos[i];
        std::vector<Constant*> containedTypePtrs;
        for(unsigned j=0;j<aTypeInfo->getNumContainedTypes();j++) {
            TypeInfo* containedType = aTypeInfo->getContainedType(j);
            magicArrayTypePtrMapIt = magicArrayTypePtrMap.find(containedType);
            assert(magicArrayTypePtrMapIt != magicArrayTypePtrMap.end());

            containedTypePtrs.push_back(magicArrayTypePtrMapIt->second);
        }
        Value* structContainedTypesField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_CONTAINED_TYPES);
        Constant *containedTypesValue;
        if(containedTypePtrs.size() > 0) {
            containedTypesValue = MagicUtil::getArrayPtr(M, MagicUtil::getGenericArrayRef(M, containedTypePtrs));
        }
        else {
            containedTypesValue = ConstantPointerNull::get((TYPECONST PointerType*) ((TYPECONST PointerType*)structContainedTypesField->getType())->getElementType());
        }
        new StoreInst(containedTypesValue, structContainedTypesField, false, magicArrayBuildFuncInst);
        if(!aTypeInfo->hasRawTypeRepresentation()) {
            continue;
        }

        //handle raw array types
        assert(aTypeInfo->getNumContainedTypes() == 1 && aTypeInfo->getContainedType(0)->getType()->isArrayTy());
        magicArrayTypeIndexMapIt = magicArrayTypeIndexMap.find(aTypeInfo->getContainedType(0));
        assert(magicArrayTypeIndexMapIt != magicArrayTypeIndexMap.end());
        dstIndex = magicArrayTypeIndexMapIt->second;

        //fix size field (inherited by parent type)
        Value* srcStructSizeField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_SIZE);
        Value* dstStructSizeField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, dstIndex, 10)), MAGIC_TSTRUCT_FIELD_SIZE);
        Value* srcStructSizeValue = new LoadInst(srcStructSizeField, "", false, magicArrayBuildFuncInst);
        new StoreInst(srcStructSizeValue, dstStructSizeField, false, magicArrayBuildFuncInst);

        //fix num_child_types field
        Value* dstStructNumChildTypesField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, dstIndex, 10)), MAGIC_TSTRUCT_FIELD_NUM_CHILD_TYPES);
        Value* voidStructSizeField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, voidTypeIndex, 10)), MAGIC_TSTRUCT_FIELD_SIZE);
        Value* voidStructSizeValue = new LoadInst(voidStructSizeField, "", false, magicArrayBuildFuncInst);
        BinaryOperator* numChildTypesValue = BinaryOperator::Create(Instruction::SDiv, srcStructSizeValue, voidStructSizeValue, "", magicArrayBuildFuncInst);
        new StoreInst(numChildTypesValue, dstStructNumChildTypesField, false, magicArrayBuildFuncInst);
    }

    i=0;
    //build cast types pointers
    for(;i<globalTypeInfos.size();i++) {
        TypeInfo* aTypeInfo = globalTypeInfos[i];
        std::vector<Constant*> castTypePtrs;
        std::vector<TypeInfo*> castTypes = aTypeInfo->getCastTypes();

        Value* structCompatibleTypesField = MagicUtil::getMagicTStructFieldPtr(M, magicArrayBuildFuncInst, magicTypeArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_TSTRUCT_FIELD_COMPATIBLE_TYPES);
        TYPECONST PointerType* nullArrayType = (TYPECONST PointerType*) ((TYPECONST PointerType*)structCompatibleTypesField->getType())->getElementType();
        for(unsigned j=0;j<castTypes.size();j++) {
            TypeInfo* castType = castTypes[j];
            if(castType == NULL) {
                castTypePtrs.push_back(ConstantPointerNull::get((TYPECONST PointerType*) nullArrayType->getContainedType(0)));
            }
            else {
                magicArrayTypePtrMapIt = magicArrayTypePtrMap.find(castType);
                assert(magicArrayTypePtrMapIt != magicArrayTypePtrMap.end());

                castTypePtrs.push_back(magicArrayTypePtrMapIt->second);
            }
        }

        Constant *compatibleTypesValue;
        if(castTypePtrs.size() > 0) {
            compatibleTypesValue = MagicUtil::getArrayPtr(M, MagicUtil::getGenericArrayRef(M, castTypePtrs));
        }
        else {
            compatibleTypesValue = ConstantPointerNull::get(nullArrayType);
        }
        new StoreInst(compatibleTypesValue, structCompatibleTypesField, false, magicArrayBuildFuncInst);
    }

    //build magic array in build function
    i=0;
    strGlobalVariables = 0;
    PointerType* voidPointerType = PointerType::get(IntegerType::get(M.getContext(), 8), 0);
    for(;i<globalVariables.size();i++) {
        GlobalVariable *GV = globalVariables[i];
        DIGlobalVariable *DIGV = NULL;
        StringRef GVName;
        bool isFromLibrary, hasAddressTaken, isString, isNamedString;
        isString = GV->getName().startswith(".str");
        isNamedString = false;
        if(isString) {
            stringOwnerInvertedMapIt = stringOwnerInvertedMap.find(GV);
            if(stringOwnerInvertedMapIt != stringOwnerInvertedMap.end()) {
                isNamedString = true;
                DIGV = NULL;
                GVName = ".str#" + stringOwnerInvertedMapIt->second;
            }
        }
        if(!isNamedString) {
            GVName = MagicUtil::getGVSourceName(M, GV, &DIGV, baseBuildDir);
        }
        isFromLibrary = isExtLibrary(GV, DIGV);
        hasAddressTaken = globalVariablesWithAddressTaken.find(GV) != globalVariablesWithAddressTaken.end();
        std::string GVNameStr(GVName.str());

        //storing id field
        Value* structIdField = MagicUtil::getMagicSStructFieldPtr(M, magicArrayBuildFuncInst, magicArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_SSTRUCT_FIELD_ID);
        Constant* idValue = ConstantInt::get(M.getContext(), APInt(32, i+1, 10));
        new StoreInst(idValue, structIdField, false, magicArrayBuildFuncInst);

        //storing name field
        Value* structNameField = MagicUtil::getMagicSStructFieldPtr(M, magicArrayBuildFuncInst, magicArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_SSTRUCT_FIELD_NAME);
        Constant* nameValue = MagicUtil::getArrayPtr(M, MagicUtil::getStringRef(M, GVNameStr));
        new StoreInst(nameValue, structNameField, false, magicArrayBuildFuncInst);

        //storing type field
        parentMapIt = globalParentMap.find(GV);
        if(parentMapIt == globalParentMap.end()) {
            continue;
        }
        assert(parentMapIt != globalParentMap.end());
        TypeInfo* aTypeInfo = parentMapIt->second;
        magicArrayTypePtrMapIt = magicArrayTypePtrMap.find(aTypeInfo);
        assert(magicArrayTypePtrMapIt != magicArrayTypePtrMap.end());
        Value* structTypeField = MagicUtil::getMagicSStructFieldPtr(M, magicArrayBuildFuncInst, magicArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_SSTRUCT_FIELD_TYPE);
        Constant* typeValue = magicArrayTypePtrMapIt->second;
        new StoreInst(typeValue, structTypeField, false, magicArrayBuildFuncInst);

        //filling flags field
        int annotation, flags = MAGIC_STATE_DATA;
        if(GV->hasExternalLinkage() || GV->hasExternalWeakLinkage()) {
            flags |= MAGIC_STATE_EXTERNAL;
        }
        if(GV->isConstant()) {
            flags |= MAGIC_STATE_CONSTANT;
        }
        if(GV->isThreadLocal()) {
            flags |= MAGIC_STATE_THREAD_LOCAL;
        }
        if(isFromLibrary) {
            flags |= MAGIC_STATE_LIB;
        }
        if(!hasAddressTaken) {
            flags |= MAGIC_STATE_ADDR_NOT_TAKEN;
        }
        if(isString) {
            flags |= MAGIC_STATE_STRING;
            if(isNamedString) {
                flags |= MAGIC_STATE_NAMED_STRING;
            }
            strGlobalVariables++;
        }
        if(MagicUtil::getVarAnnotation(M, GV, &annotation)) {
            magicPassLog("Magic annotation found for global variable: " << GV->getName());
            flags |= (annotation & MAGIC_STATE_ANNOTATION_MASK);
        }
        Value* structFlagsField = MagicUtil::getMagicSStructFieldPtr(M, magicArrayBuildFuncInst, magicArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_SSTRUCT_FIELD_FLAGS);
        Constant* flagsValue = ConstantInt::get(M.getContext(), APInt(32, flags, 10));
        new StoreInst(flagsValue, structFlagsField, false, magicArrayBuildFuncInst);

        //filling address field
        Value* structAddressField = MagicUtil::getMagicSStructFieldPtr(M, magicArrayBuildFuncInst, magicArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_SSTRUCT_FIELD_ADDRESS);
        Constant* varAddressValue = ConstantExpr::getCast(Instruction::BitCast, GV, voidPointerType);
        new StoreInst(varAddressValue, structAddressField, false, magicArrayBuildFuncInst);

        //filling shadow address field
        Value* structShadowAddressField = MagicUtil::getMagicSStructFieldPtr(M, magicArrayBuildFuncInst, magicArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_SSTRUCT_FIELD_SHADOW_ADDRESS);
        Constant* varShadowAddressValue;
        if(EnableShadowing && !GV->isConstant()) {
            GlobalVariable* varShadow = MagicUtil::getShadowRef(M, GV);
            shadowGlobalVariables.push_back(varShadow);
            varShadowAddressValue = ConstantExpr::getCast(Instruction::BitCast, varShadow, voidPointerType);
        }
        else {
            varShadowAddressValue = ConstantPointerNull::get((TYPECONST PointerType*) ((TYPECONST PointerType*)structShadowAddressField->getType())->getElementType());
        }
        new StoreInst(varShadowAddressValue, structShadowAddressField, false, magicArrayBuildFuncInst);
    }

    //build magic function array in build function
    i=0;
    for(;i<functions.size();i++) {
        Function *F = functions[i];
        DISubprogram *DIS = NULL;
        StringRef FName = MagicUtil::getFunctionSourceName(M, F, &DIS, baseBuildDir);
        std::string FNameStr(FName.str());
        bool isFromLibrary = isExtLibrary(F, DIS);

        //storing id field
        Value* structIdField = MagicUtil::getMagicFStructFieldPtr(M, magicArrayBuildFuncInst, magicFunctionArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_FSTRUCT_FIELD_ID);
        Constant* idValue = ConstantInt::get(M.getContext(), APInt(32, i+1, 10));
        new StoreInst(idValue, structIdField, false, magicArrayBuildFuncInst);

        //storing name field
        Value* structNameField = MagicUtil::getMagicFStructFieldPtr(M, magicArrayBuildFuncInst, magicFunctionArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_FSTRUCT_FIELD_NAME);
        Constant* nameValue = MagicUtil::getArrayPtr(M, MagicUtil::getStringRef(M, FNameStr));
        new StoreInst(nameValue, structNameField, false, magicArrayBuildFuncInst);

        //storing type field
        parentMapIt = globalParentMap.find(F);
        assert(parentMapIt != globalParentMap.end());
        TypeInfo* aTypeInfo = parentMapIt->second;
        magicArrayTypePtrMapIt = magicArrayTypePtrMap.find(aTypeInfo);
        assert(magicArrayTypePtrMapIt != magicArrayTypePtrMap.end());
        Value* structTypeField = MagicUtil::getMagicFStructFieldPtr(M, magicArrayBuildFuncInst, magicFunctionArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_FSTRUCT_FIELD_TYPE);
        Constant* typeValue = magicArrayTypePtrMapIt->second;
        new StoreInst(typeValue, structTypeField, false, magicArrayBuildFuncInst);

        //filling flags field
        int flags = MAGIC_STATE_TEXT|MAGIC_STATE_CONSTANT;
        if(isFromLibrary) {
            flags |= MAGIC_STATE_LIB;
        }
        if(!F->hasAddressTaken()) {
            flags |= MAGIC_STATE_ADDR_NOT_TAKEN;
        }
        Value* structFlagsField = MagicUtil::getMagicFStructFieldPtr(M, magicArrayBuildFuncInst, magicFunctionArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_FSTRUCT_FIELD_FLAGS);
        Constant* flagsValue = ConstantInt::get(M.getContext(), APInt(32, flags, 10));
        new StoreInst(flagsValue, structFlagsField, false, magicArrayBuildFuncInst);

        //filling address field
        Value* structAddressField = MagicUtil::getMagicFStructFieldPtr(M, magicArrayBuildFuncInst, magicFunctionArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_FSTRUCT_FIELD_ADDRESS);
        Constant* varAddressValue = ConstantExpr::getCast(Instruction::BitCast, F, voidPointerType);
        new StoreInst(varAddressValue, structAddressField, false, magicArrayBuildFuncInst);
    }

#if MAGIC_INSTRUMENT_MEM_FUNCS
    if (!DisableMemFunctions) {
        //replace magic memory function calls with their wrappers
        for(i=0;i<magicMemFunctionCalls.size();i++) {
            MagicMemFunction *magicMemFunctionCall = &magicMemFunctionCalls[i];
            magicMemFunctionCall->replaceInstruction(magicArrayTypePtrMap, magicVoidPtrTypeInfo);
        }

        //fix debug function calls and their arguments
        for (i=0;i<magicDebugFunctions.size();i++) {
            MagicDebugFunction *magicDebugFunction = &magicDebugFunctions[i];
            magicDebugFunction->fixCalls(M, baseBuildDir);
        }

        //fix mmap ctl function calls and their arguments
        for (i=0;i<magicMmapCtlFunctions.size();i++) {
            MagicMmapCtlFunction *magicMmapCtlFunction = &magicMmapCtlFunctions[i];
            magicMmapCtlFunction->fixCalls(M, magicGetPageSizeFunc);
        }
    }
#endif /*MAGIC_INSTRUMENT_MEM_FUNCS*/

#if MAGIC_INSTRUMENT_STACK
    //instrument the stack for the relevant set of functions and add dsindex entries
    for(i=0;i<stackIntrumentedFuncs.size();i++) {
        addMagicStackDsentryFuncCalls(M, stackIntrumentedFuncs[i], stackIntrumentedFuncs[i], magicStackDsentryCreateFunc, magicStackDsentryDestroyFunc,
            magicDsentryStructType, localTypeInfoMaps[i], magicArrayTypePtrMap, magicVoidPtrTypeInfo, magicDsindexTypeInfoList, magicDsindexNamesList, magicDsindexFlagsList);
    }
#endif

    //allocate magic dsindex array
    ArrayType* magicDsindexArrayType = ArrayType::get(magicDsindexStructType, magicDsindexTypeInfoList.size());
    magicDsindexArray = new GlobalVariable(M, magicDsindexArrayType, false, GlobalValue::InternalLinkage, ConstantAggregateZero::get(magicDsindexArrayType), MAGIC_DSINDEX_ARRAY_NAME);
    MagicUtil::setGlobalVariableSection(magicDsindexArray, MAGIC_STATIC_VARS_SECTION_DATA);

    //build magic dsindex array in build function
    i=0;
    for(;i<magicDsindexTypeInfoList.size();i++) {
        //storing type field
        TypeInfo* aTypeInfo = magicDsindexTypeInfoList[i];
        magicArrayTypePtrMapIt = magicArrayTypePtrMap.find(aTypeInfo);
        assert(magicArrayTypePtrMapIt != magicArrayTypePtrMap.end());
        Value* structTypeField = MagicUtil::getMagicDStructFieldPtr(M, magicArrayBuildFuncInst, magicDsindexArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_DSTRUCT_FIELD_TYPE);
        Constant* typeValue = magicArrayTypePtrMapIt->second;
        new StoreInst(typeValue, structTypeField, false, magicArrayBuildFuncInst);

        //storing name field
        Value* structNameField = MagicUtil::getMagicDStructFieldPtr(M, magicArrayBuildFuncInst, magicDsindexArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_DSTRUCT_FIELD_NAME);
        Constant* nameValue = MagicUtil::getArrayPtr(M, MagicUtil::getStringRef(M, magicDsindexNamesList[i].second));
        new StoreInst(nameValue, structNameField, false, magicArrayBuildFuncInst);

        //storing parent name field
        Value* structParentNameField = MagicUtil::getMagicDStructFieldPtr(M, magicArrayBuildFuncInst, magicDsindexArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_DSTRUCT_FIELD_PARENT_NAME);
        Constant* parentNameValue = MagicUtil::getArrayPtr(M, MagicUtil::getStringRef(M, magicDsindexNamesList[i].first));
        new StoreInst(parentNameValue, structParentNameField, false, magicArrayBuildFuncInst);

        //storing flags field
        Value* structFlagsField = MagicUtil::getMagicDStructFieldPtr(M, magicArrayBuildFuncInst, magicDsindexArray, ConstantInt::get(M.getContext(), APInt(64, i, 10)), MAGIC_DSTRUCT_FIELD_FLAGS);
        Constant* flagsValue = ConstantInt::get(M.getContext(), APInt(32, magicDsindexFlagsList[i], 10));
        new StoreInst(flagsValue, structFlagsField, false, magicArrayBuildFuncInst);
    }

    // apply qprof instrumentation
    qprofInstrumentationApply(M);

    //set pointer to magic type array in build function
    new StoreInst(MagicUtil::getArrayPtr(M, magicTypeArray), magicTypeArrayPtr, false, magicArrayBuildFuncInst);

    // set runtime flags
    new StoreInst(ConstantInt::get(M.getContext(), APInt(32, DisableMemFunctions ? 1 : 0)), magicNoMemInst, false, magicArrayBuildFuncInst);

    //set magic type array size in build function
    new StoreInst(ConstantInt::get(M.getContext(), APInt(32, globalTypeInfos.size())), magicTypeArraySize, false, magicArrayBuildFuncInst);

    //set magic type next id in build function
    new StoreInst(ConstantInt::get(M.getContext(), APInt(32, globalTypeInfos.size()+1)), magicTypeNextId, false, magicArrayBuildFuncInst);

    //set pointer to magic array in build function
    new StoreInst(MagicUtil::getArrayPtr(M, magicArray), magicArrayPtr, false, magicArrayBuildFuncInst);

    //set magic array size in build function
    new StoreInst(ConstantInt::get(M.getContext(), APInt(32, globalVariables.size())), magicArraySize, false, magicArrayBuildFuncInst);

    //set magic array string size in build function
    new StoreInst(ConstantInt::get(M.getContext(), APInt(32, strGlobalVariables)), magicArrayStrSize, false, magicArrayBuildFuncInst);

    //set magic next id in build function
    new StoreInst(ConstantInt::get(M.getContext(), APInt(32, globalVariables.size()+1)), magicNextId, false, magicArrayBuildFuncInst);

    //set pointer to magic function array in build function
    new StoreInst(MagicUtil::getArrayPtr(M, magicFunctionArray), magicFunctionArrayPtr, false, magicArrayBuildFuncInst);

    //set magic function array size in build function
    new StoreInst(ConstantInt::get(M.getContext(), APInt(32, functions.size())), magicFunctionArraySize, false, magicArrayBuildFuncInst);

    //set magic function next id in build function
    new StoreInst(ConstantInt::get(M.getContext(), APInt(32, functions.size()+1)), magicFunctionNextId, false, magicArrayBuildFuncInst);

    //set pointer to magic dsindex array in build function
    new StoreInst(MagicUtil::getArrayPtr(M, magicDsindexArray), magicDsindexArrayPtr, false, magicArrayBuildFuncInst);

    //set magic dsindex array size in build function
    new StoreInst(ConstantInt::get(M.getContext(), APInt(32, magicDsindexTypeInfoList.size())), magicDsindexArraySize, false, magicArrayBuildFuncInst);

    //set magic void type pointer in build function
    magicArrayTypePtrMapIt = magicArrayTypePtrMap.find(magicVoidPtrTypeInfo);
    assert(magicArrayTypePtrMapIt != magicArrayTypePtrMap.end());
    Constant* magicVoidPtrTypeValue = magicArrayTypePtrMapIt->second;
    new StoreInst(magicVoidPtrTypeValue, magicVoidPtrTypePtr, false, magicArrayBuildFuncInst);

    //inject magic init call at the beginning of magic entry point function
    std::vector<Value*> args;
    MagicUtil::createCallInstruction(magicInitFunc, args, "", magicEntryPointFunc->getBasicBlockList().begin()->begin());

    //check invariants
#if MAGIC_CHECK_INVARIANTS
    if(maxRecursiveSequenceLength > MAGIC_MAX_RECURSIVE_TYPES) {
        magicPassErr("Max recursive sequence length is: " << maxRecursiveSequenceLength);
    }
    assert(maxRecursiveSequenceLength <= MAGIC_MAX_RECURSIVE_TYPES && "MAGIC_MAX_RECURSIVE_TYPES is too small!");
    if(TypeInfo::getMaxNameLength() > MAGIC_MAX_NAME_LEN) {
        magicPassErr("Max name length is: " << TypeInfo::getMaxNameLength());
    }
    assert(TypeInfo::getMaxNameLength() <= MAGIC_MAX_NAME_LEN && "MAGIC_MAX_NAME_LEN is too small!");
    if(TypeInfo::getMaxTypeStringLength() > MAGIC_MAX_TYPE_STR_LEN) {
        magicPassErr("Max type string length is: " << TypeInfo::getMaxTypeStringLength());
    }
    assert(TypeInfo::getMaxTypeStringLength() <= MAGIC_MAX_TYPE_STR_LEN && "MAGIC_MAX_TYPE_STR_LEN is too small!");
#endif

    return true;
}

//===----------------------------------------------------------------------===//
// Private methods
//===----------------------------------------------------------------------===//

static std::vector<int> currPtrVarIndexes;
static std::set< std::pair<Value*,std::vector<int> > > visitedValues;

bool MagicPass::checkPointerVariableIndexes(TYPECONST Type *type, std::vector<int> &ptrVarIndexes, unsigned offset)
{
    if(offset >= ptrVarIndexes.size()) {
        return true;
    }
    unsigned ptrVarIndex = (unsigned) ptrVarIndexes[ptrVarIndexes.size()-1 - offset];
    if(ptrVarIndex >= type->getNumContainedTypes()) {
        return false;
    }
    return checkPointerVariableIndexes(type->getContainedType(ptrVarIndex), ptrVarIndexes, offset+1);
}

void MagicPass::findPointerVariables(Function* function, Value *value, std::vector<Value*> &ptrVars, std::vector<std::vector<int> > &ptrVarIndexes, Value *parent, bool isUser)
{

#define RETURN_IF(X) do{ if(X){ return; } } while(0)
#define DEBUG_VALUE(M, V) do{ if(DEBUG_ALLOC_LEVEL >= 2) { errs() << M; V->print(errs()); errs() << "\n"; } } while(0)
#define DEBUG_INDEXES() do{ if(DEBUG_ALLOC_LEVEL >= 3) { errs() << ">>> Indexes: "; for(unsigned i=0;i<currPtrVarIndexes.size();i++) errs() << currPtrVarIndexes[i] << " "; errs() << "\n"; } } while(0)

    std::pair<Value*,std::vector<int> > visitedPair(value, currPtrVarIndexes);
    if(visitedValues.find(visitedPair) != visitedValues.end()) {
        return;
    }

    DEBUG_VALUE(" >>>> findPointerVariables: Value is: ", value);
    DEBUG_VALUE(" >>>> findPointerVariables: Parent value is: ", parent);
    DEBUG_INDEXES();
    std::vector<int> savedPtrVarIndexes;
    visitedValues.insert(visitedPair);
    ConstantExpr *constantExpr = dyn_cast<ConstantExpr>(value);
    if(currPtrVarIndexes.size() == 0) {
        if(DEBUG_ALLOC_LEVEL >= 2) {
            magicPassErr("Empty indexes, skipping search path!");
        }
        RETURN_IF(true);
    }
    else if(GlobalVariable *GV = dyn_cast<GlobalVariable>(value)) {
        if(DEBUG_ALLOC_LEVEL >= 2) {
            magicPassErr("Found global variable!");
        }
        ptrVars.push_back(GV);
        ptrVarIndexes.push_back(currPtrVarIndexes);
        assert(!isUser);
        if(GV->getType()->getElementType() != PointerType::get(IntegerType::get(function->getParent()->getContext(), 8), 0)) {
            RETURN_IF(true);
        }
    }
    else if(AllocaInst *AI = dyn_cast<AllocaInst>(value)) {
        if(DEBUG_ALLOC_LEVEL >= 2) {
            magicPassErr("Found local variable!");
        }
        ptrVars.push_back(AI);
        ptrVarIndexes.push_back(currPtrVarIndexes);
        assert(!isUser);
        if(AI->getAllocatedType() != PointerType::get(IntegerType::get(function->getParent()->getContext(), 8), 0)) {
            RETURN_IF(true);
        }
    }
    else if(dyn_cast<ReturnInst>(value)) {
        if(DEBUG_ALLOC_LEVEL >= 2) {
            magicPassErr("Found return variable!");
        }
        assert(isUser);
        RETURN_IF(true);
    }
    else if(StoreInst *SI = dyn_cast<StoreInst>(value)) {
        DEBUG_VALUE(" >>>> findPointerVariables: Digging store instruction: ", value);
        assert(isUser);
        if(parent == SI->getOperand(1)) {
            assert(currPtrVarIndexes.size() > 0 && currPtrVarIndexes[currPtrVarIndexes.size()-1] == 0);
            currPtrVarIndexes.pop_back();
            findPointerVariables(function, SI->getOperand(0), ptrVars, ptrVarIndexes, value);
            currPtrVarIndexes.push_back(0);
        }
        else {
            currPtrVarIndexes.push_back(0);
            findPointerVariables(function, SI->getOperand(1), ptrVars, ptrVarIndexes, value);
            currPtrVarIndexes.pop_back();
        }
    }
    else if(LoadInst *LI = dyn_cast<LoadInst>(value)) {
        DEBUG_VALUE(" >>>> findPointerVariables: Digging load instruction: ", value);
        if(isUser) {
            assert(currPtrVarIndexes.size() > 0 && currPtrVarIndexes[currPtrVarIndexes.size()-1] == 0);
            savedPtrVarIndexes.push_back(currPtrVarIndexes.back());
            currPtrVarIndexes.pop_back();
        }
        else {
            currPtrVarIndexes.push_back(0);
            findPointerVariables(function, LI->getOperand(0), ptrVars, ptrVarIndexes, value);
            currPtrVarIndexes.pop_back();
        }
    }
    else if(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(value)) {
        if(GEPI->getNumIndices() == 1) {
            DEBUG_VALUE(" >>>> findPointerVariables: Digging GEP instruction: ", value);
            findPointerVariables(function, GEPI->getOperand(0), ptrVars, ptrVarIndexes, value);
        }
        else {
            RETURN_IF(isUser);
            DEBUG_VALUE(" >>>> findPointerVariables: Digging GEP instruction: ", value);
            unsigned k = 0;
            int index;
            assert(currPtrVarIndexes.size() > 0 && currPtrVarIndexes[currPtrVarIndexes.size()-1] == 0);
            currPtrVarIndexes.pop_back(); //pop 0
            for(GetElementPtrInst::const_op_iterator i=GEPI->idx_end()-1, b=GEPI->idx_begin();i>b;i--,k++) {
                index = 0;
                if(ConstantInt *CI = dyn_cast<ConstantInt>(*i)) {
                    index = CI->getSExtValue();
                }
                currPtrVarIndexes.push_back(index);
            }
            currPtrVarIndexes.push_back(0); //push 0
            findPointerVariables(function, GEPI->getOperand(0), ptrVars, ptrVarIndexes, value);
            currPtrVarIndexes.pop_back();   //pop 0
            while(k-->0) {
                currPtrVarIndexes.pop_back();
            }
            currPtrVarIndexes.push_back(0); //push 0
        }
    }
    else if(constantExpr && constantExpr->getOpcode() == Instruction::GetElementPtr) {
        assert(constantExpr->getNumOperands() >= 2);
        if(constantExpr->getNumOperands() == 2) {
            DEBUG_VALUE(" >>>> findPointerVariables: Digging GEP expression: ", value);
            findPointerVariables(function, constantExpr->getOperand(0), ptrVars, ptrVarIndexes, value);
        }
        else {
            RETURN_IF(isUser);
            DEBUG_VALUE(" >>>> findPointerVariables: Digging GEP expression: ", value);
            unsigned k = 0;
            int index;
            assert(currPtrVarIndexes.size() > 0 && currPtrVarIndexes[currPtrVarIndexes.size()-1] == 0);
            currPtrVarIndexes.pop_back(); //pop 0
            for(unsigned i=constantExpr->getNumOperands()-1;i>1;i--,k++) {
                index = 0;
                if(ConstantInt *CI = dyn_cast<ConstantInt>(constantExpr->getOperand(i))) {
                    index = CI->getSExtValue();
                }
                currPtrVarIndexes.push_back(index);
            }
            currPtrVarIndexes.push_back(0); //push 0
            findPointerVariables(function, constantExpr->getOperand(0), ptrVars, ptrVarIndexes, value);
            currPtrVarIndexes.pop_back();   //pop 0
            while(k-->0) {
                currPtrVarIndexes.pop_back();
            }
            currPtrVarIndexes.push_back(0); //push 0
        }
    }
    else if(BitCastInst *CI = dyn_cast<BitCastInst>(value)) {
        if((isUser && !checkPointerVariableIndexes(CI->getType(), currPtrVarIndexes))
            || (!isUser && !checkPointerVariableIndexes(CI->getOperand(0)->getType(), currPtrVarIndexes))) {
            DEBUG_VALUE(" >>>> findPointerVariables: Skipping unsafe cast instruction: ", value);
            RETURN_IF(true);
        }
        DEBUG_VALUE(" >>>> findPointerVariables: Digging cast instruction: ", value);
        findPointerVariables(function, CI->getOperand(0), ptrVars, ptrVarIndexes, value);
    }
    else if(dyn_cast<CallInst>(value) || dyn_cast<InvokeInst>(value)) {
        RETURN_IF(isUser);
        DEBUG_VALUE(" >>>> findPointerVariables: found call instruction: ", value);
    }
    else if(CmpInst *CI = dyn_cast<CmpInst>(value)) {
        assert(isUser);
        DEBUG_VALUE(" >>>> findPointerVariables: Digging cmp instruction: ", value);
        findPointerVariables(function, CI->getOperand(0), ptrVars, ptrVarIndexes, value);
        findPointerVariables(function, CI->getOperand(1), ptrVars, ptrVarIndexes, value);
        RETURN_IF(true);
    }
    else if(SelectInst *SI = dyn_cast<SelectInst>(value)) {
        DEBUG_VALUE(" >>>> findPointerVariables: Digging select instruction: ", value);
        findPointerVariables(function, SI->getOperand(1), ptrVars, ptrVarIndexes, value);
        findPointerVariables(function, SI->getOperand(2), ptrVars, ptrVarIndexes, value);
    }
    else if(constantExpr && constantExpr->getOpcode() == Instruction::Select) {
        DEBUG_VALUE(" >>>> findPointerVariables: Digging select expression: ", value);
        findPointerVariables(function, constantExpr->getOperand(1), ptrVars, ptrVarIndexes, value);
        findPointerVariables(function, constantExpr->getOperand(2), ptrVars, ptrVarIndexes, value);
    }
    else if(PHINode *PN = dyn_cast<PHINode>(value)) {
        DEBUG_VALUE(" >>>> findPointerVariables: Digging PHI instruction: ", value);
        for(unsigned i=0;i<PN->getNumIncomingValues();i++) {
            findPointerVariables(function, PN->getIncomingValue(i), ptrVars, ptrVarIndexes, value);
        }
    }
    else if(Argument *ARG = dyn_cast<Argument>(value)) {
        DEBUG_VALUE(" >>>> findPointerVariables: Digging Argument: ", value);
        AllocaInst *AI = MagicUtil::getAllocaInstFromArgument(ARG);
        assert(AI);
        currPtrVarIndexes.push_back(0);
        findPointerVariables(function, AI, ptrVars, ptrVarIndexes, value);
        currPtrVarIndexes.pop_back();
        RETURN_IF(true);
    }
    else {
        DEBUG_VALUE(" ************************************************************ findPointerVariables: Unknown value: ", value);
        RETURN_IF(true);
    }
    for (Value::user_iterator i = value->user_begin(), e = value->user_end(); i != e; ++i) {
        User *user = *i;
        Instruction *instruction = dyn_cast<Instruction>(user);
        if(!instruction || instruction->getParent()->getParent() != function) {
            continue;
        }
        DEBUG_VALUE(" >>>> findPointerVariables: Found user: ", user);
        findPointerVariables(function, user, ptrVars, ptrVarIndexes, value, true);
    }
    while(savedPtrVarIndexes.size() > 0) {
        currPtrVarIndexes.push_back(savedPtrVarIndexes.back());
        savedPtrVarIndexes.pop_back();
    }
}

TypeInfo* MagicPass::typeInfoFromPointerVariables(Module &M, TypeInfo *voidPtrTypeInfo, std::vector<Value*> &ptrVars, std::vector<std::vector<int> > &ptrVarIndexes, std::string &allocName)
{
    std::vector<TypeInfo*> validTypeInfos;
    std::set<TypeInfo*> validTypeInfoSet;
    std::vector<unsigned> validTypeTags;
    std::vector<unsigned> voidTypeTags;
    std::vector<int> indexes;
    TypeInfo *aTypeInfo = NULL;
    TypeInfo *voidTypeInfo = voidPtrTypeInfo->getContainedType(0);
    allocName = "";
    if(ptrVars.size()==0) {
        return voidTypeInfo;
    }

    for(unsigned i=0;i<ptrVars.size();i++) {
        DIVariable DIV;
        unsigned tag = 0;
        std::string varName = "";
        if(GlobalVariable *GV = dyn_cast<GlobalVariable>(ptrVars[i])) {
            parentMapIt = globalParentMap.find(GV);
            assert(parentMapIt != globalParentMap.end());
            aTypeInfo = parentMapIt->second;
            tag = dwarf::DW_TAG_variable;
            varName = MagicUtil::getGVSourceName(M, GV, NULL, baseBuildDir);
        }
        else {
            AllocaInst *AI = dyn_cast<AllocaInst>(ptrVars[i]);
            assert(AI);
            if(DEBUG_ALLOC_LEVEL >= 4) {
            	AI->print(errs()); errs() << "\n";
            }
            const SmartType *aSmartType = SmartType::getSmartTypeFromLV(M, AI, &DIV);
            if(aSmartType == (const SmartType *)-1) {
                //a temporary variable
                if(DEBUG_ALLOC_LEVEL >= 4) {
                    magicPassErr("typeInfoFromPointerVariables: Skipping temporary variable");
                }
                continue;
            }
            else if(!aSmartType) {
                //a return variable
                if(DEBUG_ALLOC_LEVEL >= 4) {
                    magicPassErr("typeInfoFromPointerVariables: Processing return variable");
                }
                if(AI->getAllocatedType() == voidPtrTypeInfo->getType()) {
                    aTypeInfo = voidPtrTypeInfo;
                }
                else {
                    aTypeInfo = fillExternalTypeInfos(AI->getAllocatedType(), NULL, globalTypeInfos);
                    if(aTypeInfo == NULL) {
                        magicPassErr("typeInfoFromPointerVariables: type is: " << TypeUtil::getDescription(AI->getAllocatedType(), MAGIC_TYPE_STR_PRINT_MAX, MAGIC_TYPE_STR_PRINT_MAX_LEVEL));
                        if(!MAGIC_ABORT_ON_UNSUPPORTED_LOCAL_EXTERNAL_TYPE) {
                            magicPassErr("typeInfoFromPointerVariables: Warning: Local external type not supported, resorting to void* type...");
                            aTypeInfo = voidPtrTypeInfo;
                        }
                        else {
                            assert(aTypeInfo != NULL && "Local external type not supported!");
                        }
                    }
                }
                tag = dwarf::DW_TAG_unspecified_type;
            }
            else {
                //a regular variable (potentially returning a value to the caller)
                if(DEBUG_ALLOC_LEVEL >= 4) {
                    magicPassErr("typeInfoFromPointerVariables: Processing regular variable");
                }
                TypeInfo newTypeInfo(aSmartType);
                aTypeInfo = fillTypeInfos(newTypeInfo, globalTypeInfos);
                if(aTypeInfo->getSmartType() != aSmartType) {
                    delete aSmartType;
                }
                if (PassUtil::isReturnedValue(AI->getParent()->getParent(), AI)) {
                    // treat this variable as a return variable
                    tag = dwarf::DW_TAG_unspecified_type;
                }
                else {
                    tag = DIV.getTag();
                }
            }
            varName = DIV.getName();
        }
        //see if the target type is an alias for void*
        assert(aTypeInfo);
        if(aTypeInfo->getType()->isPointerTy()) {
            stringSetIt = voidTypeAliasesSet.find(aTypeInfo->getContainedType(0)->getName());
            if(stringSetIt != voidTypeAliasesSet.end()) {
                aTypeInfo = voidPtrTypeInfo;
            }
        }
        //fix tag if needed
        if(tag == dwarf::DW_TAG_unspecified_type) {
            TYPECONST Type *type = aTypeInfo->getType();
            if(!type->isPointerTy() || type->getContainedType(0)->isPointerTy()) {
                //not a good return type, switch to a regular variable
                tag = dwarf::DW_TAG_auto_variable;
            }
        }
        if(tag == dwarf::DW_TAG_arg_variable) {
            TYPECONST Type *type = aTypeInfo->getType();
            if(!type->isPointerTy() || !type->getContainedType(0)->isPointerTy() || type->getContainedType(0)->getContainedType(0)->isPointerTy()) {
                //not a good arg type, switch to a regular variable
                tag = dwarf::DW_TAG_auto_variable;
            }
        }
        if(DEBUG_ALLOC_LEVEL >= 3) {
            switch(tag) {
                case dwarf::DW_TAG_unspecified_type:
                    magicPassErr("typeInfoFromPointerVariables: Found return variable: " << varName);
                break;
                case dwarf::DW_TAG_variable:
                    magicPassErr("typeInfoFromPointerVariables: Found global variable:" << varName);
                break;
                case dwarf::DW_TAG_auto_variable:
                    magicPassErr("typeInfoFromPointerVariables: Found local variable:" << varName);
                break;
                case dwarf::DW_TAG_arg_variable:
                    magicPassErr("typeInfoFromPointerVariables: Found argument variable:" << varName);
                break;
                default:
                    assert(0 && "Should never get here!");
                break;
            }
        }
        indexes = ptrVarIndexes[i];
        assert(indexes.back() == 0);
        indexes.pop_back();
        if(DEBUG_ALLOC_LEVEL >= 4) {
            magicPassErr("typeInfoFromPointerVariables: " << indexes.size() << " indexes to process.");
        }
        while(!indexes.empty()) {
            int index = indexes.back();
            if(aTypeInfo->hasRawTypeRepresentation()) {
                if(DEBUG_ALLOC_LEVEL >= 4) {
                    magicPassErr("typeInfoFromPointerVariables: Skipping index (raw type representation): " << index << ", type is: " << aTypeInfo->getVerboseDescription());
                }
                aTypeInfo = voidTypeInfo;
                break;
            }
            if(!aTypeInfo->getType()->isStructTy()) {
                index = 0;
            }
            aTypeInfo = aTypeInfo->getContainedType(index);
            if(DEBUG_ALLOC_LEVEL >= 4) {
                magicPassErr("typeInfoFromPointerVariables: Processing index: " << index << ", type is: " << aTypeInfo->getVerboseDescription());
            }
            indexes.pop_back();
        }
        if(aTypeInfo == voidTypeInfo) {
            voidTypeTags.push_back(tag);
        }
        else {
            validTypeInfos.push_back(aTypeInfo);
            validTypeInfoSet.insert(aTypeInfo);
            validTypeTags.push_back(tag);
        }
        if(!allocName.compare("")) {
            allocName = varName;
        }
    }
    //see if we have a valid void return type
    bool hasValidVoidReturnType = false;
    for(unsigned i=0;i<voidTypeTags.size();i++) {
        if(voidTypeTags[i] == dwarf::DW_TAG_unspecified_type || voidTypeTags[i] == dwarf::DW_TAG_arg_variable) {
            hasValidVoidReturnType = true;
            break;
        }
    }

    //count the number of weak local types
    unsigned numWeakLocalTypes = 0;
    unsigned nonWeakTypeIndex = 0;
    unsigned index = 0;
    for (std::set<TypeInfo*>::iterator it=validTypeInfoSet.begin() ; it != validTypeInfoSet.end(); it++ ) {
        if((*it)->getType() == voidTypeInfo->getType()) {
            numWeakLocalTypes++;
        } else {
            nonWeakTypeIndex = index;
        }
        index++;
    }
    bool hasOnlyWeakLocalTypes = (numWeakLocalTypes == validTypeInfoSet.size());
    bool hasOnlyOneNonWeakLocalType = (validTypeInfoSet.size() - numWeakLocalTypes == 1);

    if(DEBUG_ALLOC_LEVEL >= 3) {
        magicPassErr("typeInfoFromPointerVariables: Status: voidTypeTagsSize=" << voidTypeTags.size() << ", hasValidVoidReturnType=" << hasValidVoidReturnType << ", hasOnlyWeakLocalTypes=" << hasOnlyWeakLocalTypes << ", hasOnlyOneNonWeakLocalType=" << hasOnlyOneNonWeakLocalType);
    }

    //return NULL (treat the function as a wrapper) if we have a valid return type and only weak local types
    if(hasValidVoidReturnType && hasOnlyWeakLocalTypes) {
        if(DEBUG_ALLOC_LEVEL >= 3) {
            magicPassErr("typeInfoFromPointerVariables: Returning no type at all: treat the function as a wrapper");
        }
        return NULL;
    }

    //a single valid type has been found, return it
    if(hasOnlyOneNonWeakLocalType || (hasOnlyWeakLocalTypes && validTypeInfoSet.size() > 0)) {
        if(validTypeTags[nonWeakTypeIndex] == dwarf::DW_TAG_unspecified_type) {
            if(DEBUG_ALLOC_BAD_TYPES) {
                magicPassErr("typeInfoFromPointerVariables: warning: non-void return type");
            }
        }
        if(validTypeTags[nonWeakTypeIndex] == dwarf::DW_TAG_arg_variable) {
            if(DEBUG_ALLOC_BAD_TYPES) {
                magicPassErr("typeInfoFromPointerVariables: warning: non-void arg type");
            }
        }
        if(DEBUG_ALLOC_LEVEL >= 3) {
            magicPassErr("typeInfoFromPointerVariables: Returning single valid type");
        }
        return validTypeInfos[nonWeakTypeIndex];
    }
    //multiple valid types found, print warning and resort to void
    else if(validTypeInfoSet.size() > 1 && DEBUG_ALLOC_BAD_TYPES) {
        magicPassErr("typeInfoFromPointerVariables: warning: multiple valid types found:");
        for (std::set<TypeInfo*>::iterator it=validTypeInfoSet.begin() ; it != validTypeInfoSet.end(); it++ ) {
            magicPassErr(" - " << (*it)->getVerboseDescription());
        }
        if(DEBUG_ALLOC_LEVEL >= 3) {
            magicPassErr("typeInfoFromPointerVariables: Multiple valid types found");
        }
    }

    if(DEBUG_ALLOC_LEVEL >= 3) {
        magicPassErr("typeInfoFromPointerVariables: Returning default void type");
    }
    return voidTypeInfo;
}

TypeInfo* MagicPass::getAllocTypeInfo(Module &M, TypeInfo *voidPtrTypeInfo, const CallSite &CS, std::string &allocName, std::string &allocParentName)
{
    Value *allocPointer = NULL;
    Function *function = MagicUtil::getCalledFunctionFromCS(CS);
    Function *parentFunction = CS.getInstruction()->getParent()->getParent();
    if(DEBUG_ALLOC_LEVEL >= 1) {
        magicPassErr("Function is: " << function->getName());
        magicPassErr("Parent is: " << parentFunction->getName());
    }
    std::vector<Value*> ptrVars;
    std::vector<std::vector<int> > ptrVarIndexes;
    currPtrVarIndexes.clear();
    visitedValues.clear();
    int pointerParam = MagicMemFunction::getMemFunctionPointerParam(function, brkFunctions, voidPtrTypeInfo);
    assert(pointerParam >= 0 && "Invalid wrapper function!");
    if(pointerParam == 0) {
        allocPointer = CS.getInstruction();
        currPtrVarIndexes.push_back(0);
    }
    else {
        allocPointer = CS.getArgument(pointerParam-1);
        currPtrVarIndexes.push_back(0);
        //brk is a special case and takes the pointer by value
        if(brkFunctions.find(function) == brkFunctions.end()) {
            currPtrVarIndexes.push_back(0);
        }
    }
    findPointerVariables(parentFunction, allocPointer, ptrVars, ptrVarIndexes);
    TypeInfo* aTypeInfo = typeInfoFromPointerVariables(M, voidPtrTypeInfo, ptrVars, ptrVarIndexes, allocName);
    allocParentName = MagicUtil::getFunctionSourceName(M, parentFunction, NULL, baseBuildDir);
    if(DEBUG_ALLOC_LEVEL >= 1) {
        magicPassErr("**************** type found: " << (aTypeInfo ? aTypeInfo->getType()->isStructTy() ? "struct " + aTypeInfo->getName() : aTypeInfo->getVerboseDescription() : "NULL"));
    }
    return aTypeInfo;
}

TypeInfo* MagicPass::fillTypeInfos(TypeInfo &sourceTypeInfo, std::vector<TypeInfo*> &typeInfos) {
    static std::vector<TypeInfo*> nestedTypes;
    static unsigned level = 0;
    if(DEBUG_FILL_TYPE_INFOS) {
        magicPassErr("Entering level: " << level << ", Examining type: " << sourceTypeInfo.getDescription() << ", types so far: " << typeInfos.size());
    }

    if(sourceTypeInfo.getType()) {
        TYPECONST Type* type = sourceTypeInfo.getType();
        for(unsigned i=0;i<nestedTypes.size();i++) {
            if(type == nestedTypes[i]->getType()) {
                const SmartType *nestedSType = nestedTypes[i]->getSmartType();
                const SmartType *sourceSType = sourceTypeInfo.getSmartType();
                if((!nestedSType && !sourceSType) || (nestedSType && sourceSType && nestedSType->getEDIType()->equals(sourceSType->getEDIType()))) {
                    nestedTypes[i]->addParents(sourceTypeInfo.getParents());
                    return nestedTypes[i];
                }
            }
        }
    }
    assert(sourceTypeInfo.getParents().size() <= 1);
    for(unsigned i=0;i<typeInfos.size();i++) {
        if(typeInfos[i]->equals(&sourceTypeInfo)) {
            typeInfos[i]->addParents(sourceTypeInfo.getParents());
            return typeInfos[i];
        }
    }
    TypeInfo *aTypeInfo = new TypeInfo(sourceTypeInfo);
    aTypeInfo->setPersistent();
    const SmartType *aSmartType = aTypeInfo->getSmartType();
    unsigned numContainedTypes = aSmartType ? aSmartType->getNumContainedTypes() : 0;
    const SmartType* containedSmartType = NULL;
    TypeInfo* addedTypeInfo = NULL;
    std::vector<TypeInfo*> aTypeInfoContainedTypes;
    nestedTypes.push_back(aTypeInfo);
    level++;
    for(unsigned i=0;i<numContainedTypes;i++) {
        containedSmartType = aSmartType->getContainedType(i);
        if(!containedSmartType->isFunctionTy() || containedSmartType->isTypeConsistent()) {
            TypeInfo containedTypeInfo(containedSmartType);
            addedTypeInfo = fillTypeInfos(containedTypeInfo, typeInfos);
        }
        else {
            TYPECONST FunctionType* type = (TYPECONST FunctionType*) containedSmartType->getType();
            TypeInfo containedTypeInfo(type);
            addedTypeInfo = fillTypeInfos(containedTypeInfo, typeInfos);
        }
        if(addedTypeInfo->getSmartType() != containedSmartType) {
            delete containedSmartType;
        }
        aTypeInfoContainedTypes.push_back(addedTypeInfo);
    }
    level--;
    nestedTypes.pop_back();
    aTypeInfo->setContainedTypes(aTypeInfoContainedTypes);
    typeInfos.push_back(aTypeInfo);
    if(DEBUG_FILL_TYPE_INFOS) {
        magicPassErr("Exiting level: " << level << ", types so far: " << typeInfos.size());
    }
    return aTypeInfo;
}

TypeInfo* MagicPass::fillExternalTypeInfos(TYPECONST Type *sourceType, GlobalValue* parent, std::vector<TypeInfo*> &typeInfos) {
    static std::map<TYPECONST Type *, TypeInfo*> externalTypeInfoCache;
    std::map<TYPECONST Type*, TypeInfo*>::iterator externalTypeInfoCacheIt;
    TypeInfo* aTypeInfo = NULL;
    std::vector<TypeInfo*> compatibleTypeInfos;
    //see if we already have the type in the cache first
    externalTypeInfoCacheIt = externalTypeInfoCache.find(sourceType);
    if(externalTypeInfoCacheIt != externalTypeInfoCache.end()) {
        aTypeInfo = externalTypeInfoCacheIt->second;
        if(parent) {
            aTypeInfo->addParent(parent);
        }
        return aTypeInfo;
    }

    for(unsigned i=0;i<typeInfos.size();i++) {
        if(typeInfos[i]->getSmartType() && typeInfos[i]->getSmartType()->getType() == sourceType && (!sourceType->isArrayTy() || typeInfos[i]->getTypeID() == MAGIC_TYPE_ARRAY)) {
            compatibleTypeInfos.push_back(typeInfos[i]);
        }
    }
    if(compatibleTypeInfos.size() > 0) {
        unsigned minStringTypeInfo = 0;
        if(compatibleTypeInfos.size() > 1) {
            /* Select the first type in alphabetical order to ensure deterministic behavior. */
            for(unsigned i=1;i<compatibleTypeInfos.size();i++) {
                if(compatibleTypeInfos[i]->getSmartType()->getEDIType()->getDescription().compare(compatibleTypeInfos[minStringTypeInfo]->getSmartType()->getEDIType()->getDescription()) < 0) {
                    minStringTypeInfo = i;
                }
            }
        }
        aTypeInfo = compatibleTypeInfos[minStringTypeInfo];
    }
    if(DEBUG_FILL_EXT_TYPE_INFOS && compatibleTypeInfos.size() > 1) {
        std::string typeString;
        for(unsigned i=0;i<compatibleTypeInfos.size();i++) {
            assert(compatibleTypeInfos[i]->getSmartType());
            typeString += (i==0 ? "" : ", ") + compatibleTypeInfos[i]->getSmartType()->getEDIType()->getDescription();
        }
        magicPassErr("Multiple compatible types found for external type " << TypeUtil::getDescription(sourceType, MAGIC_TYPE_STR_PRINT_MAX, MAGIC_TYPE_STR_PRINT_MAX_LEVEL) << ": " << typeString << "; selecting the first type in alphabetical order: " << aTypeInfo->getSmartType()->getEDIType()->getDescription());
    }
    if(aTypeInfo == NULL) {
        TypeInfo *targetTypeInfo = NULL;
        if(TypeUtil::isOpaqueTy(sourceType)) {
            aTypeInfo = new TypeInfo((TYPECONST StructType*) sourceType, TYPEINFO_PERSISTENT);
            typeInfos.push_back(aTypeInfo);
        }
        else if(sourceType->isPointerTy()) {
            TYPECONST Type *targetType = sourceType->getContainedType(0);
            targetTypeInfo = fillExternalTypeInfos(targetType, NULL, typeInfos);
            if(targetTypeInfo == NULL) {
                return NULL;
            }
            aTypeInfo = new TypeInfo((TYPECONST PointerType*) sourceType, TYPEINFO_PERSISTENT);
        }
        else if(sourceType->isArrayTy()) {
            TYPECONST Type *targetType = sourceType->getContainedType(0);
            targetTypeInfo = fillExternalTypeInfos(targetType, NULL, typeInfos);
            if(targetTypeInfo == NULL) {
                return NULL;
            }
            aTypeInfo = new TypeInfo((TYPECONST ArrayType*) sourceType, TYPEINFO_PERSISTENT);
        }
        else if(sourceType->isIntegerTy()) {
            aTypeInfo = new TypeInfo((TYPECONST IntegerType*) sourceType, TYPEINFO_PERSISTENT);
            typeInfos.push_back(aTypeInfo);
        }
        else if(sourceType->isFunctionTy()) {
            aTypeInfo = new TypeInfo((TYPECONST FunctionType*) sourceType, TYPEINFO_PERSISTENT);
            typeInfos.push_back(aTypeInfo);
        }
        if(targetTypeInfo != NULL) {
            std::vector<TypeInfo*> containedTypes;
            containedTypes.push_back(targetTypeInfo);
            aTypeInfo->setContainedTypes(containedTypes);
            typeInfos.push_back(aTypeInfo);
        }
    }
    if(aTypeInfo && parent) {
        aTypeInfo->addParent(parent);
    }
    externalTypeInfoCache.insert(std::pair<TYPECONST Type*, TypeInfo*>(sourceType, aTypeInfo));
    return aTypeInfo;
}

void MagicPass::printInterestingTypes(TYPECONST TypeInfo *aTypeInfo) {
    static std::vector<TYPECONST TypeInfo*> nestedTypes;
    static std::vector<unsigned> nestedIndexes;
    static std::vector<TYPECONST TypeInfo*> interestingTypesSoFar;
    static std::string typeName;
    static unsigned level = 0;
    for(unsigned i=0;i<nestedTypes.size();i++) {
        if(aTypeInfo == nestedTypes[i]) {
            return;
        }
    }

    bool isInterestingType = false;
    const SmartType *aSmartType = aTypeInfo->getSmartType();
    if(aSmartType) {
        if(aSmartType->isStructTy() && !aTypeInfo->getName().compare("")) {
            isInterestingType = true;
            typeName = "Anonymous";
        }
        if(aSmartType->getEDIType()->isEnumTy()) {
            isInterestingType = true;
            typeName = "Enum";
        }
        if(aSmartType->isOpaqueTy()) {
            isInterestingType = true;
            typeName = "Opaque";
        }
    }
    if(isInterestingType) {
        bool isNewInterestingType = true;
        for(unsigned i=0;i<interestingTypesSoFar.size();i++) {
            if(aTypeInfo == interestingTypesSoFar[i]) {
                isNewInterestingType = false;
                break;
            }
        }
        if(isNewInterestingType) {
            interestingTypesSoFar.push_back(aTypeInfo);
            if(nestedTypes.size() == 0) {
                dbgs() << "**** " << typeName << " top type found, printing it: \n";
                dbgs() << aSmartType->getDescription();
                aSmartType->getEDIType()->getDIType()->print(dbgs());
            }
            else {
                dbgs() << "**** " << typeName << " type found, printing path: \n";
                dbgs() << "**************** LEVEL 0\n";
                dbgs() << "**************** NAME: " << nestedTypes[0]->getName() << "\n";
                dbgs() << nestedTypes[0]->getSmartType()->getDescription();
                unsigned i;
                for(i=1;i<nestedTypes.size();i++) {
                    dbgs() << "**************** LEVEL " << i << "\n";
                    dbgs() << "**************** NAME: " << nestedTypes[i]->getName() << "\n";
                    dbgs() << "**************** PARENT INDEX " << nestedIndexes[i-1] << "\n";
                    dbgs() << nestedTypes[i]->getSmartType()->getDescription();
                }
                dbgs() << "**************** LAST LEVEL " << i << "\n";
                dbgs() << "**************** PARENT INDEX " << nestedIndexes[i-1] << "\n";
                dbgs() << aSmartType->getDescription();
                aSmartType->getEDIType()->getDIType()->print(dbgs());
                dbgs() << "*****************************************\n";
            }
        }
    }

    unsigned numContainedTypes = aTypeInfo->getNumContainedTypes();
    nestedTypes.push_back(aTypeInfo);
    level++;
    for(unsigned i=0;i<numContainedTypes;i++) {
        nestedIndexes.push_back(i);
        printInterestingTypes(aTypeInfo->getContainedType(i));
        nestedIndexes.pop_back();
    }
    level--;
    nestedTypes.pop_back();
}

unsigned MagicPass::getMaxRecursiveSequenceLength(TYPECONST TypeInfo *aTypeInfo) {
    static std::vector<TYPECONST TypeInfo*> nestedTypes;
    static unsigned level = 0;
    for(unsigned i=0;i<nestedTypes.size();i++) {
        if(aTypeInfo == nestedTypes[i]) {
            return nestedTypes.size()+1;
        }
    }

    unsigned numContainedTypes = aTypeInfo->getNumContainedTypes();
    unsigned length, maxLength = 0;
    nestedTypes.push_back(aTypeInfo);
    level++;
    for(unsigned i=0;i<numContainedTypes;i++) {
        length = getMaxRecursiveSequenceLength(aTypeInfo->getContainedType(i));
        if(length > maxLength) {
            maxLength = length;
        }
    }
    level--;
    nestedTypes.pop_back();
    return maxLength;
}

FunctionType* MagicPass::getFunctionType(TYPECONST FunctionType *baseType, std::vector<unsigned> selectedArgs) {
	std::vector<TYPECONST Type*> ArgTypes;
	for (unsigned i = 0; i < selectedArgs.size(); i++) {
		ArgTypes.push_back(baseType->getParamType(selectedArgs[i] - 1));
	}
	// Create a new function type...
	FunctionType *FTy = FunctionType::get(baseType->getReturnType(), ArgTypes, baseType->isVarArg());
	return FTy;
}

bool MagicPass::isCompatibleMagicMemFuncType(TYPECONST FunctionType *type, TYPECONST FunctionType* magicType) {
    if(type->getReturnType() != magicType->getReturnType()) {
        return false;
    }
    unsigned numContainedTypes = type->getNumContainedTypes();
    unsigned numContainedMagicTypes = magicType->getNumContainedTypes();
    if(numContainedTypes > numContainedMagicTypes) {
        return false;
    }
    for(unsigned i=0;i<numContainedTypes-1;i++) {
        TYPECONST Type* cType = type->getContainedType(numContainedTypes-1-i);
        TYPECONST Type* cMagicType = magicType->getContainedType(numContainedMagicTypes-1-i);
        if (!MagicUtil::isCompatibleType(cType, cMagicType)) {
        	return false;
        }
    }
    return true;
}

Function* MagicPass::findWrapper(Module &M, std::string *magicMemPrefixes, Function *f, std::string fName)
{
    std::string wName, wName2;
    Function *w = NULL, *w2 = NULL;
    for(unsigned k=0;magicMemPrefixes[k].compare("");k++) {
        wName = magicMemPrefixes[k] + fName;
        w = M.getFunction(wName);
        if(w) {
            wName2 = wName + "_";
            break;
        }
    }
    if(!w) {
        magicPassErr("Error: no wrapper function found for " << fName << "()");
        exit(1);
    }
    while(!isCompatibleMagicMemFuncType(f->getFunctionType(), w->getFunctionType()) && (w2 = M.getFunction(wName2))) {
        w = w2;
        wName2.append("_");
    }
    if(!isCompatibleMagicMemFuncType(f->getFunctionType(), w->getFunctionType())) {
        magicPassErr("Error: wrapper function with incompatible type " << wName << "() found");
        magicPassErr(TypeUtil::getDescription(f->getFunctionType(), MAGIC_TYPE_STR_PRINT_MAX, MAGIC_TYPE_STR_PRINT_MAX_LEVEL) << " != " << TypeUtil::getDescription(w->getFunctionType(), MAGIC_TYPE_STR_PRINT_MAX, MAGIC_TYPE_STR_PRINT_MAX_LEVEL));
        exit(1);
    }
    return w;
}

#if MAGIC_INDEX_BIT_CAST
static void processBitCast(Module &M, std::map<TYPECONST Type*, std::set<TYPECONST Type*> > &bitCastMap, TYPECONST Type* srcType, TYPECONST Type* dstType);

static void processFunctionBitCast(Module &M, std::map<TYPECONST Type*, std::set<TYPECONST Type*> > &bitCastMap, TYPECONST Type* srcType, TYPECONST Type* dstType) {
    // The rough intuition: we are casting one function to another, so we expect these functions to be
    // compatible, so their respective parameters must also be compatible, if they are different at all.
    // We limit ourselves to pointer parameters because we are only interested in pointer compatibility.
    // This routine basically aims to mark two structure pointers as compatible when one structure has
    // an opaque pointer and the other one does not.
    TYPECONST FunctionType* srcF = dyn_cast<TYPECONST FunctionType>(srcType);
    TYPECONST FunctionType* dstF = dyn_cast<TYPECONST FunctionType>(dstType);

    // Both functions must have the same number of parameters.
    unsigned int numParams = srcF->getNumParams();

    if (numParams != dstF->getNumParams()) return;

    TYPECONST Type* voidPtrType = PointerType::get(IntegerType::get(M.getContext(), 8), 0);

    // If any of the parameters are pointers for different types, these types must be compatible.
    for (unsigned int i = 0; i < numParams; i++) {
        TYPECONST Type* spType = srcF->getParamType(i);
        TYPECONST Type* dpType = dstF->getParamType(i);

        // The parameters must have different types, but they must both be pointers.
        if (spType == dpType) continue;

        if (!spType->isPointerTy() || !dpType->isPointerTy()) continue;

        // We ignore certain types, depending on our configuration.
        TYPECONST Type* dpElType = TypeUtil::getRecursiveElementType(dpType);

        if (!((MAGIC_INDEX_FUN_PTR_BIT_CAST && dpElType->isFunctionTy()) ||
            (MAGIC_INDEX_STR_PTR_BIT_CAST && dpElType->isStructTy()) ||
            MAGIC_INDEX_OTH_PTR_BIT_CAST)) continue;

        // TODO: this needs configuration testing as well.
        if (spType == voidPtrType || dpType == voidPtrType) continue;

#if DEBUG_CASTS
        errs() << "Compatible function parameter " << i << ": " << TypeUtil::getDescription(spType) <<
            " -> " << TypeUtil::getDescription(dpType) << "\n";
#endif

        // The two pointers should be compatible, so mark them as such.
        // TODO: prevent infinite recursion
	processBitCast(M, bitCastMap, spType, dpType);
    }
}

#if 0
static void processStructBitCast(Module &M, std::map<TYPECONST Type*, std::set<TYPECONST Type*> > &bitCastMap, TYPECONST Type* srcType, TYPECONST Type* dstType) {
    // The rough intuition: the given structure types are subject to pointer casting. This does not
    // mean they are compatible by itself (struct sockaddr..). HOWEVER, if they differ only by
    // elements which are different only (recursively) by opaque pointers in one of them, and
    // non-opaque pointer in the other, then those pointers are highly likely to be compatible.
    TYPECONST StructType* srcS = dyn_cast<TYPECONST StructType>(srcType);
    TYPECONST StructType* dstS = dyn_cast<TYPECONST StructType>(dstType);

    // The structures must be similar..
    if (srcS->isPacked() != dstS->isPacked()) return false;

    unsigned int numElements = srcS->getNumElements();

    if (numElements != dstS->getNumElements()) return false;

    // ..but not the same.
    if (srcS->isLayoutIdentical(dstS)) return false;

    // Pass 1: see if the structures differ only by opaque (sub)elements.
    for (unsigned int i = 0; i < numElements; i++) {
        TYPECONST Type* seType = srcS->getElementType(i);
        TYPECONST Type* deType = dstS->getElementType(i);

        if (seType != deType) {
            if (seType->isPointerTy() && deType->isPointerTy()) {
                TYPECONST PointerType* sePtrType = dyn_cast<PointerType>(seType);
                TYPECONST PointerType* dePtrType = dyn_cast<PointerType>(deType);

                // ..TODO..
                // this may involve recursive testing!
            }

            // ..TODO..
        }
    }

    // Pass 2: register all pointers to compatible elements.
    // ..TODO..
    // this may involve recursive registration!
}
#endif

static void processBitCast(Module &M, std::map<TYPECONST Type*, std::set<TYPECONST Type*> > &bitCastMap, TYPECONST Type* srcType, TYPECONST Type* dstType) {
    std::map<TYPECONST Type*, std::set<TYPECONST Type*> >::iterator bitCastMapIt;
    unsigned int dstDepth, srcDepth;
    TYPECONST PointerType* ptrType;

    // The pointers are compatible, so add them to the bitcast map.
    bitCastMapIt = bitCastMap.find(dstType);
    if(bitCastMapIt == bitCastMap.end()) {
        std::set<TYPECONST Type*> typeSet;
        typeSet.insert(srcType);
        bitCastMap.insert(std::pair<TYPECONST Type*, std::set<TYPECONST Type*> >(dstType, typeSet));
    }
    else {
        std::set<TYPECONST Type*> *typeSet = &(bitCastMapIt->second);
        typeSet->insert(srcType);
    }

    // Unfortunately, this is not the whole story. The compiler may pull crazy stunts like storing
    // a well-defined pointer in a structure, and then bitcast that structure to an almost-equivalent
    // structure which has the pointer marked as opaque. Worse yet, it may bitcast between functions
    // with such structures as parameters.  In those case, we never see a cast of the actual pointer,
    // even though they are compatible. Failing to mark them as such could cause runtime failures.
    // The code below is a first attempt to deal with a subset of cases that we have actually run
    // into in practice. A better approach would be a separate pass that eliminates opaque pointers
    // whenever possible altogether, but that would be even more work. TODO! Note that in general,
    // it seems that there is no way to get to know which pointers the linker decided are equivalent,
    // so this procedure is inherently going to involve guessing, with false positives and negatives.

    // Follow the pointers to see what they actually point to.
    // The caller may already have done so, but without getting the depth.
    for (dstDepth = 0; (ptrType = dyn_cast<PointerType>(dstType)); dstDepth++)
        dstType = ptrType->getElementType();

    for (srcDepth = 0; (ptrType = dyn_cast<PointerType>(srcType)); srcDepth++)
        srcType = ptrType->getElementType();

    // The pointers' indirection levels must be the same.
    if (srcDepth != dstDepth) return;

    // Do more processing for certain types.
    if (dstType->isFunctionTy() && srcType->isFunctionTy())
        processFunctionBitCast(M, bitCastMap, srcType, dstType);
    // TODO: add support for structures and their elements
#if 0
    else if (dstType->isStructTy() && srcType->isStructTy())
        processStructBitCast(M, bitCastMap, srcType, dstType);
#endif
}
#endif /* MAGIC_INDEX_BIT_CAST */

void MagicPass::indexCasts(Module &M, User *U, std::vector<TYPECONST Type*> &intCastTypes, std::vector<int> &intCastValues, std::map<TYPECONST Type*, std::set<TYPECONST Type*> > &bitCastMap) {
    unsigned i;
    TYPECONST Type* voidPtrType = PointerType::get(IntegerType::get(M.getContext(), 8), 0);

    //look at instructions first
#if MAGIC_INDEX_INT_CAST
    if(CastInst* CI = dyn_cast<IntToPtrInst>(U)) {
        TYPECONST Type* type = TypeUtil::getArrayFreePointerType(CI->getDestTy());
        TYPECONST Type* elType = TypeUtil::getRecursiveElementType(type);
        if((MAGIC_INDEX_FUN_PTR_INT_CAST && elType->isFunctionTy()) || (MAGIC_INDEX_STR_PTR_INT_CAST && elType->isStructTy()) || MAGIC_INDEX_OTH_PTR_INT_CAST) {
            if(MAGIC_INDEX_VOID_PTR_INT_CAST || type != voidPtrType) {
                intCastTypes.push_back(type);
                ConstantInt *value = dyn_cast<ConstantInt>(CI->getOperand(0));
                intCastValues.push_back(value ? value->getSExtValue() : 0);
#if DEBUG_CASTS
                CI->print(errs()); errs() << "\n";
#endif
            }
        }
    }
#endif

#if MAGIC_INDEX_BIT_CAST
    if(BitCastInst* CI = dyn_cast<BitCastInst>(U)) {
        TYPECONST Type* type = TypeUtil::getArrayFreePointerType(CI->getDestTy());
        TYPECONST Type* elType = TypeUtil::getRecursiveElementType(type);
        if((MAGIC_INDEX_FUN_PTR_BIT_CAST && elType->isFunctionTy()) || (MAGIC_INDEX_STR_PTR_BIT_CAST && elType->isStructTy()) || MAGIC_INDEX_OTH_PTR_BIT_CAST) {
            if(MAGIC_INDEX_VOID_PTR_BIT_CAST || type != voidPtrType) {
                TYPECONST Type* srcType = TypeUtil::getArrayFreePointerType(CI->getSrcTy());
                if(srcType != type && (!MAGIC_SKIP_TOVOID_PTR_BIT_CAST || srcType != voidPtrType)) {
#if DEBUG_CASTS
                    CI->print(errs()); errs() << "\n";
#endif
                    processBitCast(M, bitCastMap, srcType, type);
                }
            }
        }
    }
#endif

    //now dig looking for constant expressions
    std::vector<User*> users;
    users.push_back(U);
    while(!users.empty()) {
        User *user = users.front();
        users.erase(users.begin());
        ConstantExpr *CE = dyn_cast<ConstantExpr>(user);

#if MAGIC_INDEX_INT_CAST
        if(CE && CE->getOpcode() == Instruction::IntToPtr) {
            TYPECONST Type* type = TypeUtil::getArrayFreePointerType(CE->getType());
            TYPECONST Type* elType = TypeUtil::getRecursiveElementType(type);
            if((MAGIC_INDEX_FUN_PTR_INT_CAST && elType->isFunctionTy()) || (MAGIC_INDEX_STR_PTR_INT_CAST && elType->isStructTy()) || MAGIC_INDEX_OTH_PTR_INT_CAST) {
                if(MAGIC_INDEX_VOID_PTR_INT_CAST || type != voidPtrType) {
#if DEBUG_CASTS
                    CE->print(errs()); errs() << "\n";
#endif
                    intCastTypes.push_back(type);
                    ConstantInt *value = dyn_cast<ConstantInt>(CE->getOperand(0));
                    intCastValues.push_back(value ? value->getSExtValue() : 0);
                }
            }
        }
#endif

#if MAGIC_INDEX_BIT_CAST
        if(CE && CE->getOpcode() == Instruction::BitCast) {
            TYPECONST Type* type = TypeUtil::getArrayFreePointerType(CE->getType());
            TYPECONST Type* elType = TypeUtil::getRecursiveElementType(type);
            if((MAGIC_INDEX_FUN_PTR_BIT_CAST && elType->isFunctionTy()) || (MAGIC_INDEX_STR_PTR_BIT_CAST && elType->isStructTy()) || MAGIC_INDEX_OTH_PTR_BIT_CAST) {
                if(MAGIC_INDEX_VOID_PTR_BIT_CAST || type != voidPtrType) {
                    TYPECONST Type* srcType = TypeUtil::getArrayFreePointerType(CE->getOperand(0)->getType());
                    if(srcType != type && (!MAGIC_SKIP_TOVOID_PTR_BIT_CAST || srcType != voidPtrType)) {
#if DEBUG_CASTS
                        CE->print(errs()); errs() << "\n";
#endif
                        processBitCast(M, bitCastMap, srcType, type);
                    }
                }
            }
        }
#endif

        for(i=0;i<user->getNumOperands();i++) {
            User *operand = dyn_cast<User>(user->getOperand(i));
            if(operand && !isa<Instruction>(operand) && !isa<GlobalVariable>(operand)) {
                users.push_back(operand);
            }
        }
    }
}

void MagicPass::fillStackInstrumentedFunctions(std::vector<Function*> &stackIntrumentedFuncs, Function *deepestLLFunction) {
    assert(!deepestLLFunction->hasAddressTaken() && "Indirect calls not supported for detection of long-lived functions");
    for(unsigned i=0;i<stackIntrumentedFuncs.size();i++) {
        if(stackIntrumentedFuncs[i] == deepestLLFunction) {
            return;
        }
    }
    stackIntrumentedFuncs.push_back(deepestLLFunction);
    for (Value::user_iterator i = deepestLLFunction->user_begin(), e = deepestLLFunction->user_end(); i != e; ++i) {
        User *user = *i;
        if(Instruction *I = dyn_cast<Instruction>(user)) {
            fillStackInstrumentedFunctions(stackIntrumentedFuncs, I->getParent()->getParent());
        }
    }
}

void MagicPass::indexLocalTypeInfos(Module &M, Function *F, std::map<AllocaInst*, std::pair<TypeInfo*, std::string> > &localMap) {
  DIVariable DIV;
  for (inst_iterator it = inst_begin(F), et = inst_end(F); it != et; ++it) {
      AllocaInst *AI = dyn_cast<AllocaInst>(&(*it));
      if(!AI) {
          break;
      }
      const SmartType *aSmartType = SmartType::getSmartTypeFromLV(M, AI, &DIV);
      if(!aSmartType || aSmartType == (const SmartType *)-1) {
          // skip return and temporary variables
          continue;
      }
      TypeInfo newTypeInfo(aSmartType);
      TypeInfo *aTypeInfo = fillTypeInfos(newTypeInfo, globalTypeInfos);
      if(aTypeInfo->getSmartType() != aSmartType) {
          delete aSmartType;
      }
      std::string name = MagicUtil::getLVSourceName(M, AI).data();
      std::pair<TypeInfo*, std::string> infoNamePair(aTypeInfo, name);
      localMap.insert(std::pair<AllocaInst*, std::pair<TypeInfo*, std::string> >(AI, infoNamePair));
  }
}

void MagicPass::addMagicStackDsentryFuncCalls(Module &M, Function *insertCallsInFunc, Function *localsFromFunc, Function *dsentryCreateFunc, Function *dsentryDestroyFunc, TYPECONST StructType *dsentryStructType, std::map<AllocaInst*, std::pair<TypeInfo*, std::string> > localTypeInfoMap, std::map<TypeInfo*, Constant*> &magicArrayTypePtrMap, TypeInfo *voidPtrTypeInfo, std::vector<TypeInfo*> &typeInfoList, std::vector<std::pair<std::string, std::string> > &namesList, std::vector<int> &flagsList) {
  std::vector<Value*> locals;
  std::map<AllocaInst*, std::pair<TypeInfo*, std::string> >::iterator localTypeInfoMapIt;
  std::map<TypeInfo*, Constant*>::iterator magicArrayTypePtrMapIt;
  std::vector<TypeInfo*> localTypeInfos;
  std::vector<Value*> localTypeInfoValues;
  std::vector<Value*> localDsentryValues;
  std::string allocName, allocParentName;
  Instruction *allocaI = NULL, *dsentryCreateI = NULL, *dsentryDestroyI = NULL;
  // find local variables and types
  for (inst_iterator it = inst_begin(localsFromFunc), et = inst_end(localsFromFunc); it != et; ++it) {
      AllocaInst *AI = dyn_cast<AllocaInst>(&(*it));
      if(!AI) {
          break;
      }
      localTypeInfoMapIt = localTypeInfoMap.find(AI);
      if(localTypeInfoMapIt != localTypeInfoMap.end()) {
          assert(AI->hasName());
          TypeInfo *aTypeInfo = localTypeInfoMapIt->second.first;
          magicArrayTypePtrMapIt = magicArrayTypePtrMap.find(aTypeInfo);
          assert(magicArrayTypePtrMapIt != magicArrayTypePtrMap.end());
          Constant *aTypeInfoValue = magicArrayTypePtrMapIt->second;
          localTypeInfos.push_back(aTypeInfo);
          localTypeInfoValues.push_back(aTypeInfoValue);
          locals.push_back(AI);
      }
  }
  // find the first and the last valid instruction to place a call and the alloca point
  dsentryCreateI = MagicUtil::getFirstNonAllocaInst(insertCallsInFunc);
  dsentryDestroyI = insertCallsInFunc->back().getTerminator();
  allocaI = MagicUtil::getFirstNonAllocaInst(insertCallsInFunc, false);

  // create one dsentry for each local variable
  for(unsigned i=0;i<locals.size();i++) {
      AllocaInst *AI = new AllocaInst(dsentryStructType, "dsentry_" + (locals[i]->hasName() ? locals[i]->getName() : "anon"), allocaI);
      localDsentryValues.push_back(AI);
  }
  assert(localTypeInfoValues.size() == localDsentryValues.size());

  // create one dsentry and value set array for the return address
  localTypeInfos.push_back(voidPtrTypeInfo);
  localTypeInfoValues.push_back(new AllocaInst(ArrayType::get(IntegerType::get(M.getContext(), 32), 2), "dsentry_ret_addr_value_set", allocaI)); //pass the value set pointer as though it were a type pointer
  localDsentryValues.push_back(new AllocaInst(dsentryStructType, "dsentry_ret_addr", allocaI));

  // create one dsentry pointer to remember the last stack dsentry
  AllocaInst *prevLastStackDsentry = new AllocaInst(PointerType::get(dsentryStructType, 0), "prev_last_stack_dsentry", allocaI);

  // get the frame address of the function and pass the value as though it were a data pointer
  Function *frameAddrIntrinsic = MagicUtil::getIntrinsicFunction(M, Intrinsic::frameaddress);
  std::vector<Value*> frameAddrArgs;
  frameAddrArgs.push_back(ConstantInt::get(M.getContext(), APInt(32, 0)));
  CallInst *callInst = MagicUtil::createCallInstruction(frameAddrIntrinsic, frameAddrArgs, "", dsentryCreateI);
  locals.push_back(callInst);

  // place calls
  std::vector<Value*> dsentryCreateArgs;
  std::vector<Value*> dsentryDestroyArgs;
  dsentryCreateArgs.push_back(prevLastStackDsentry);
  dsentryDestroyArgs.push_back(prevLastStackDsentry);
  dsentryCreateArgs.push_back(ConstantInt::get(M.getContext(), APInt(32, locals.size())));
  dsentryDestroyArgs.push_back(ConstantInt::get(M.getContext(), APInt(32, locals.size())));
  allocParentName = MagicUtil::getFunctionSourceName(M, insertCallsInFunc, NULL, baseBuildDir);
  int allocFlags = MAGIC_STATE_STACK;
  for(unsigned i=0;i<locals.size();i++) {
      //get name
      if(AllocaInst *AI = dyn_cast<AllocaInst>(locals[i])) {
          // local variable
          localTypeInfoMapIt = localTypeInfoMap.find(AI);
          assert(localTypeInfoMapIt != localTypeInfoMap.end());
          allocName = localTypeInfoMapIt->second.second;
      }
      else {
          // return address
          allocName = MAGIC_ALLOC_RET_ADDR_NAME;
      }
      //add args
      dsentryCreateArgs.push_back(localDsentryValues[i]);
      dsentryCreateArgs.push_back(localTypeInfoValues[i]);
      dsentryCreateArgs.push_back(locals[i]);
      dsentryCreateArgs.push_back(MagicUtil::getStringRef(M, allocParentName));
      dsentryCreateArgs.push_back(MagicUtil::getStringRef(M, allocName));
      dsentryDestroyArgs.push_back(localDsentryValues[i]);
      //add elements to type and names lists
      typeInfoList.push_back(localTypeInfos[i]);
      namesList.push_back(std::pair<std::string, std::string>(allocParentName, allocName));
      flagsList.push_back(allocFlags);
  }
  MagicUtil::createCallInstruction(dsentryCreateFunc, dsentryCreateArgs, "", dsentryCreateI);
  if(isa<ReturnInst>(dsentryDestroyI)) {
      MagicUtil::createCallInstruction(dsentryDestroyFunc, dsentryDestroyArgs, "", dsentryDestroyI);
  }
}

bool MagicPass::isExtLibrary(GlobalValue *GV, DIDescriptor *DID)
{
    static bool regexesInitialized = false;
    static std::vector<Regex*> regexes;
    if(!regexesInitialized) {
        std::vector<std::string>::iterator it;
        for (it = libPathRegexes.begin(); it != libPathRegexes.end(); ++it) {
            Regex* regex = new Regex(*it, 0);
            std::string error;
            assert(regex->isValid(error));
            regexes.push_back(regex);
        }
        regexesInitialized = true;
    }
    if (DID) {
	std::string relPath;
	PassUtil::getDbgLocationInfo(*DID, baseBuildDir, NULL, NULL, &relPath);
        for(unsigned i=0;i<regexes.size();i++) {
            if(regexes[i]->match(relPath, NULL)) {
                return true;
            }
        }
    }
    return PassUtil::matchRegexes(GV->getSection(), extLibSectionRegexes);
}

bool MagicPass::isMagicGV(Module &M, GlobalVariable *GV)
{
    if (GV->isThreadLocal() && (GV->getName().startswith(MAGIC_PREFIX_STR) || GV->getName().startswith("rcu"))) {
        return true;
    }
    if (!StringRef(GV->getSection()).compare(MAGIC_LLVM_METADATA_SECTION)) {
	return true;
    }
    if (GV->getName().startswith("__start") || GV->getName().startswith("__stop") || GV->getName().startswith("llvm.")) {
        return true;
    }
    return PassUtil::matchRegexes(GV->getSection(), magicDataSectionRegexes);
}

bool MagicPass::isMagicFunction(Module &M, Function *F)
{
	if (F->getName().startswith("llvm.")) return true;
    return PassUtil::matchRegexes(F->getSection(), magicFunctionSectionRegexes);
}

#if MAGIC_USE_QPROF_INSTRUMENTATION

void MagicPass::qprofInstrumentationInit(Module &M)
{
    // look up qprof configuration
    qprofConf = QProfConf::get(M, &magicLLSitestacks,
        &magicDeepestLLLoops,
        &magicDeepestLLLibs,
        &magicTaskClasses);
    qprofConf->mergeAllTaskClassesWithSameDeepestLLLoops();

#if DEBUG_QPROF
    qprofConf->print(errs());
#endif
}

void MagicPass::qprofInstrumentationApply(Module &M)
{
    Function *hook;
    std::vector<Value*> hookParams;
    QProfSite* site;
    std::vector<TYPECONST Type*>functionTyArgs;
    FunctionType *hookFunctionTy;

    /*
     * Instrument deepest long-lived loops. This creates a function
     * pointer of the form void (*MAGIC_DEEPEST_LL_LOOP_HOOK_NAME)(char*, int)
     * called (if set by instrumentation libraries) at the top of every loop.
     */
    functionTyArgs.push_back(PointerType::get(IntegerType::get(M.getContext(), 8), 0));
    functionTyArgs.push_back(IntegerType::get(M.getContext(), 32));
    hookFunctionTy = PassUtil::getFunctionType(Type::getVoidTy(M.getContext()), functionTyArgs, false);
    std::vector<QProfSite*> deepestLLLoops = qprofConf->getDeepestLLLoops();
    hook = PassUtil::getOrInsertFunction(M, MAGIC_DEEPEST_LL_LOOP_HOOK_NAME, hookFunctionTy,
        PASS_UTIL_LINKAGE_WEAK_POINTER, PASS_UTIL_FLAG(PASS_UTIL_PROP_PRESERVE));
    assert(hook);
    for (unsigned i=0;i<deepestLLLoops.size();i++) {
        site = deepestLLLoops[i];
        hookParams.clear();
        Constant* siteString = MagicUtil::getArrayPtr(M, MagicUtil::getStringRef(M, site->toString()));
        hookParams.push_back(siteString);
        hookParams.push_back(ConstantInt::get(M.getContext(), APInt(32, site->taskClassID, 10)));
        PassUtil::createCallInstruction(hook, hookParams, "", site->siteInstruction);
    }

    /*
     * Instrument deepest long-lived library calls. This creates a function
     * pointer of the form void (*MAGIC_DEEPEST_LL_LIB_HOOK_NAME)(char*, int, int, int)
     * called (if set by instrumentation libraries) before every library call.
     */
    functionTyArgs.clear();
    functionTyArgs.push_back(PointerType::get(IntegerType::get(M.getContext(), 8), 0));
    functionTyArgs.push_back(IntegerType::get(M.getContext(), 32));
    functionTyArgs.push_back(IntegerType::get(M.getContext(), 32));
    functionTyArgs.push_back(IntegerType::get(M.getContext(), 32));
    hookFunctionTy = PassUtil::getFunctionType(Type::getVoidTy(M.getContext()), functionTyArgs, false);
    std::vector<QProfSite*> deepestLLLibs = qprofConf->getDeepestLLLibs();
    hook = PassUtil::getOrInsertFunction(M, MAGIC_DEEPEST_LL_LIB_HOOK_NAME, hookFunctionTy,
        PASS_UTIL_LINKAGE_WEAK_POINTER, PASS_UTIL_FLAG(PASS_UTIL_PROP_PRESERVE));
    assert(hook);
    for (unsigned i=0;i<deepestLLLibs.size();i++) {
        site = deepestLLLibs[i];
        hookParams.clear();
        Constant* siteString = MagicUtil::getArrayPtr(M, MagicUtil::getStringRef(M, site->toString()));
        hookParams.push_back(siteString);
        hookParams.push_back(ConstantInt::get(M.getContext(), APInt(32, site->taskClassID, 10)));
        hookParams.push_back(ConstantInt::get(M.getContext(), APInt(32, site->taskSiteID, 10)));
        hookParams.push_back(ConstantInt::get(M.getContext(), APInt(32, site->libFlags, 10)));
        PassUtil::createCallInstruction(hook, hookParams, "", site->siteInstruction);
    }

    /*
     * Create relevant exported variables in use by the libraries.
     */
    MagicUtil::getExportedIntGlobalVar(M, MAGIC_NUM_LL_TASK_CLASSES_NAME, qprofConf->getNumLLTaskClasses());
    MagicUtil::getExportedIntGlobalVar(M, MAGIC_NUM_LL_BLOCK_EXT_TASK_CLASSES_NAME, qprofConf->getNumLLBlockExtTaskClasses());
    MagicUtil::getExportedIntGlobalVar(M, MAGIC_NUM_LL_BLOCK_INT_TASK_CLASSES_NAME, qprofConf->getNumLLBlockIntTaskClasses());
    MagicUtil::getExportedIntGlobalVar(M, MAGIC_NUM_LL_BLOCK_EXT_LIBS_NAME, qprofConf->getNumLLBlockExtLibs());
    MagicUtil::getExportedIntGlobalVar(M, MAGIC_NUM_LL_BLOCK_INT_LIBS_NAME, qprofConf->getNumLLBlockIntLibs());
}

#else

void MagicPass::qprofInstrumentationInit(Module &M) {}
void MagicPass::qprofInstrumentationApply(Module &M) {}

#endif

} // end namespace 

char MagicPass::ID = 0;
RegisterPass<MagicPass> MP("magic", "Magic Pass to Build a Table of Global Variables");

