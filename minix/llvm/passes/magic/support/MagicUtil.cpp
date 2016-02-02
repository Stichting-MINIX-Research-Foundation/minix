#include <magic/support/MagicUtil.h>

using namespace llvm;

namespace llvm {

//===----------------------------------------------------------------------===//
// Public static methods
//===----------------------------------------------------------------------===//

static std::map<const std::string, GlobalVariable*> stringRefCache;

unsigned getLabelHash(std::string label) {
    unsigned hash = 0;
    for(unsigned i=0;i<label.length();i++){
        hash ^= (label[i]);
        hash = (hash << 9) | (hash >> ((sizeof(unsigned)*8)-9));
    }
    return hash;
}

unsigned getModuleHash(DIDescriptor DID, const std::string &baseDir, StringRef extraField="") {
    std::string relPath;
    PassUtil::getDbgLocationInfo(DID, baseDir, NULL, NULL, &relPath);
    return getLabelHash(relPath + "/" + extraField.data());
}

StringRef MagicUtil::getGVSourceName(Module &M, GlobalVariable *GV, DIGlobalVariable **DIGVP, const std::string &baseDir) {
    static DIGlobalVariable Var;
    MDNode *DIGV = PassUtil::findDbgGlobalDeclare(GV);
    if(DIGV) {
        Var = DIGlobalVariable(DIGV);
        if(DIGVP) *DIGVP = &Var;
        if(GV->getLinkage() == GlobalValue::InternalLinkage){
            /* static variable */
            StringRef funcName, countStr;
            DIScope scope = Var.getContext();
            if(scope.isLexicalBlock()){
                /* find the subprogram that contains this basic block recursively */
                while(!scope.isSubprogram()){
                    scope = DILexicalBlock(scope).getContext();
                }
            }
            if(scope.isSubprogram()){
                /* static function variable */

                funcName = DISubprogram(scope).getName();

                int count=0; 
                Module::GlobalListType &globalList = M.getGlobalList();
                for (Module::global_iterator it = globalList.begin(); it != globalList.end(); ++it) {
                    GlobalVariable *OtherGV = &(*it);
                    MDNode *OtherDIGV = PassUtil::findDbgGlobalDeclare(OtherGV);
                    if(OtherDIGV) {
                        DIGlobalVariable OtherVar(OtherDIGV);

                        DIScope otherScope = OtherVar.getContext();
                        if(otherScope.isLexicalBlock()){
                            /* find the subprogram that contains this basic block recursively */
                            while(!otherScope.isSubprogram()){
                                otherScope = DILexicalBlock(otherScope).getContext();
                            }
                        }
                        if(otherScope.isSubprogram()){
                            if(!strcmp(Var.getName().data(), OtherVar.getName().data())){
                                if(DIGV == OtherDIGV){
                                    break;
                                }
                                count++;
                            }
                        }
                    }
                }

                std::stringstream stm;
                if(count > 0){
                    stm << "." << count;
                }
                countStr = StringRef(*new std::string(stm.str()));

            }else{
                /* static global variable */
                funcName = "";
                countStr = "";
            }

            std::stringstream stm;
            stm << Var.getName().data() << "." << getModuleHash(Var, baseDir, funcName) << countStr.data();
            return StringRef(*new std::string(stm.str()));

        }else{
            /* global variable */
            return Var.getName();
        }
    }else{
        /* llvm .str variables and assembly */
        if(DIGVP) *DIGVP = NULL;
        return GV->getName();
    }
}

StringRef MagicUtil::getLVSourceName(Module &M, AllocaInst *V, DIVariable **DIVP) {
    static DIVariable Var;
    const DbgDeclareInst *DDI = FindAllocaDbgDeclare(V);
    if(DDI && DDI != (const DbgDeclareInst *) -1){
        Var = DIVariable(cast<MDNode>(DDI->getVariable()));
        if(DIVP) *DIVP = &Var;

        int count = 0;

        Function *F = V->getParent()->getParent();
        for (inst_iterator it = inst_begin(F), et = inst_end(F); it != et; ++it) {
            Instruction *inst = &(*it);
            if (DbgDeclareInst *OtherDDI = dyn_cast<DbgDeclareInst>(inst)){
                DIVariable otherVar(cast<MDNode>(OtherDDI->getVariable()));
                if(!strcmp(Var.getName().data(), otherVar.getName().data())){
                    if(OtherDDI == DDI){
                        break;
                    }
                    count++;
                }
            }
        }

        std::stringstream stm;
        stm << Var.getName().data();
        if(count > 0){
            stm << "." << count;
        }
        return StringRef(*new std::string(stm.str()));
    }else{
        if(DIVP) *DIVP = NULL;
        return V->getName();
    }
}

StringRef MagicUtil::getFunctionSourceName(Module &M, Function *F, DISubprogram **DISP, const std::string &baseDir) {
    static DISubprogram Func;
    MDNode *DIF = PassUtil::findDbgSubprogramDeclare(F);
    if(DIF) {
        Func = DISubprogram(DIF);
        if(DISP) *DISP = &Func;
        if(F->getLinkage() == GlobalValue::InternalLinkage){
            std::stringstream stm;
            stm << Func.getName().data() << "." << getModuleHash(Func, baseDir);
            return StringRef(*new std::string(stm.str()));
        }else{
            return Func.getName();
        }
    }else{
        /* assembly */
        if(DISP) *DISP = NULL;
        return F->getName();
    }
}

void MagicUtil::putStringRefCache(Module &M, const std::string &str, GlobalVariable *GV) {
    std::map<const std::string, GlobalVariable*>::iterator it;
    it = stringRefCache.find(str);
    if(it == stringRefCache.end()) {
        stringRefCache.insert(std::pair<const std::string, GlobalVariable*>(str, GV));
    }
}

Constant* MagicUtil::getGetElementPtrConstant(Constant *constant, std::vector<Value*> &indexes) {
    return PassUtil::getGetElementPtrConstant(constant, indexes);
}

GetElementPtrInst* MagicUtil::createGetElementPtrInstruction(Value *ptr, std::vector<Value*> &indexes, const Twine &NameStr, Instruction *InsertBefore) {
    return PassUtil::createGetElementPtrInstruction(ptr, indexes, NameStr, InsertBefore);
}

GetElementPtrInst* MagicUtil::createGetElementPtrInstruction(Value *ptr, std::vector<Value*> &indexes, const Twine &NameStr, BasicBlock *InsertAtEnd) {
    return PassUtil::createGetElementPtrInstruction(ptr, indexes, NameStr, InsertAtEnd);
}

CallInst* MagicUtil::createCallInstruction(Value *F, std::vector<Value*> &args, const Twine &NameStr, Instruction *InsertBefore) {
    return PassUtil::createCallInstruction(F, args, NameStr, InsertBefore);
}

CallInst* MagicUtil::createCallInstruction(Value *F, std::vector<Value*> &args, const Twine &NameStr, BasicBlock *InsertAtEnd) {
    return PassUtil::createCallInstruction(F, args, NameStr, InsertAtEnd);
}

Function* MagicUtil::getIntrinsicFunction(Module &M, Intrinsic::ID id, TYPECONST Type** types, unsigned size) {
    return PassUtil::getIntrinsicFunction(M, id, types, size);
}

GlobalVariable *MagicUtil::getStringRef(Module &M, const std::string &str) {
    std::map<const std::string, GlobalVariable*>::iterator it;
    GlobalVariable *stringRef = NULL;
    bool debug = false;

    it = stringRefCache.find(str);
    if(it != stringRefCache.end()) {
        if(debug) magicUtilLog("*** getStringRef: cache hit for " << str);
        stringRef = it->second;
    }
    if(stringRef == NULL) {
    	stringRef = PassUtil::getStringGlobalVariable(M, str, MAGIC_HIDDEN_STR_PREFIX, MAGIC_STATIC_VARS_SECTION_RO);
        stringRefCache.insert(std::pair<const std::string, GlobalVariable*>(str, stringRef));
    }

     return stringRef;
 }


GlobalVariable *MagicUtil::getIntArrayRef(Module &M, unsigned arrSize, std::vector<int> *arr, bool isConstant) {
    static std::map<std::vector<int>, GlobalVariable*> arrayRefCache;
    std::map<std::vector<int>, GlobalVariable*>::iterator it;
    static std::vector<int> defInitilizer;

    //construct an appropriate initializer if we do not have one
    if(!arr) {
        arr = &defInitilizer;
        arr->clear();
        for(unsigned i=0;i<arrSize;i++) arr->push_back(0);
    }
    assert(arrSize == arr->size());

    //cache lookup
    if(isConstant) {
        it = arrayRefCache.find(*arr);
        if(it != arrayRefCache.end()) {
            return it->second;
        }
    }

    //create a constant internal array reference
    std::vector<Constant*> arrayElems;
    for(unsigned i=0;i<arr->size();i++) {
        arrayElems.push_back(ConstantInt::get(M.getContext(), APInt(32, (*arr)[i], 10)));
    }
    ArrayType* arrayTy = ArrayType::get(IntegerType::get(M.getContext(), 32), arr->size());
    Constant *arrayValue = ConstantArray::get(arrayTy, arrayElems);

    //create the global variable and record it in the module
    GlobalVariable *arrayRef = new GlobalVariable(arrayValue->getType(), isConstant,
        GlobalValue::InternalLinkage, arrayValue,
        MAGIC_HIDDEN_ARRAY_PREFIX);
    MagicUtil::setGlobalVariableSection(arrayRef, isConstant ? MAGIC_STATIC_VARS_SECTION_RO : MAGIC_STATIC_VARS_SECTION_DATA);
    M.getGlobalList().push_back(arrayRef);

    //populate cache
    if(isConstant) {
        arrayRefCache.insert(std::pair<std::vector<int>, GlobalVariable*>(*arr, arrayRef));
    }

    return arrayRef;
}

GlobalVariable *MagicUtil::getStringArrayRef(Module &M, unsigned arrSize, std::vector<std::string> *arr, bool isConstant) {
    static std::map<std::vector<std::string>, GlobalVariable*> arrayRefCache;
    std::map<std::vector<std::string>, GlobalVariable*>::iterator it;
    static std::vector<std::string> defInitilizer;
    //construct an appropriate initializer if we do not have one
    if(!arr) {
        arr = &defInitilizer;
        arr->clear();
        for(unsigned i=0;i<arrSize;i++) arr->push_back("");
    }
    assert(arrSize == arr->size());

    //cache lookup
    if(isConstant) {
        it = arrayRefCache.find(*arr);
        if(it != arrayRefCache.end()) {
            return it->second;
        }
    }

    //create a constant internal array reference
    std::vector<Constant*> arrayElems;
    std::vector<Value*> arrayIndexes;
    arrayIndexes.push_back(ConstantInt::get(M.getContext(), APInt(32, 0, 10)));
    arrayIndexes.push_back(ConstantInt::get(M.getContext(), APInt(32, 0, 10)));
    for(unsigned i=0;i<arr->size();i++) {
        arrayElems.push_back(getGetElementPtrConstant(getStringRef(M, (*arr)[i]), arrayIndexes));
    }
    ArrayType* arrayTy = ArrayType::get(PointerType::get(IntegerType::get(M.getContext(), 8), 0), arr->size());
    Constant *arrayValue = ConstantArray::get(arrayTy, arrayElems);

    //create the global variable and record it in the module
    GlobalVariable *arrayRef = new GlobalVariable(arrayValue->getType(), isConstant,
        GlobalValue::InternalLinkage, arrayValue,
        MAGIC_HIDDEN_ARRAY_PREFIX);
    MagicUtil::setGlobalVariableSection(arrayRef, isConstant ? MAGIC_STATIC_VARS_SECTION_RO : MAGIC_STATIC_VARS_SECTION_DATA);
    M.getGlobalList().push_back(arrayRef);

    //populate cache
    if(isConstant) {
        arrayRefCache.insert(std::pair<std::vector<std::string>, GlobalVariable*>(*arr, arrayRef));
    }

    return arrayRef;
}

GlobalVariable *MagicUtil::getGenericArrayRef(Module &M, std::vector<Constant*> &arrayElems, bool isConstant) {
    static std::map<std::vector<Constant*>, GlobalVariable*> arrayRefCache;
    std::map<std::vector<Constant*>, GlobalVariable*>::iterator it;
    assert(arrayElems.size() > 0);

    //cache lookup
    if(isConstant) {
        it = arrayRefCache.find(arrayElems);
        if(it != arrayRefCache.end()) {
            return it->second;
        }
    }

    //create a constant internal array reference
    ArrayType* arrayTy = ArrayType::get(arrayElems[0]->getType(), arrayElems.size());
    Constant *arrayValue = ConstantArray::get(arrayTy, arrayElems);

    //create the global variable and record it in the module
    GlobalVariable *arrayRef = new GlobalVariable(arrayValue->getType(), isConstant,
        GlobalValue::InternalLinkage, arrayValue,
        MAGIC_HIDDEN_ARRAY_PREFIX);
    MagicUtil::setGlobalVariableSection(arrayRef, isConstant ? MAGIC_STATIC_VARS_SECTION_RO : MAGIC_STATIC_VARS_SECTION_DATA);
    M.getGlobalList().push_back(arrayRef);

    //populate cache
    if(isConstant) {
        arrayRefCache.insert(std::pair<std::vector<Constant*>, GlobalVariable*>(arrayElems, arrayRef));
    }

    return arrayRef;
}

GlobalVariable *MagicUtil::getMagicTypePtrArrayRef(Module &M, Instruction *InsertBefore, std::vector<Value*> &globalTypeIndexes, GlobalVariable *magicTypeArray) {
    int numTypeIndexes = globalTypeIndexes.size();
    TYPECONST StructType* magicTypeStructTy = (TYPECONST StructType*) ((TYPECONST ArrayType*)magicTypeArray->getType()->getElementType())->getElementType();
    ArrayType* typeIndexesArrTy = ArrayType::get(PointerType::get(magicTypeStructTy, 0), numTypeIndexes+1);
    std::vector<Constant*> arrayElems;
    for(int i=0;i<numTypeIndexes;i++) {
        std::vector<Value*> magicTypeArrayIndexes;
        magicTypeArrayIndexes.clear();
        magicTypeArrayIndexes.push_back(ConstantInt::get(M.getContext(), APInt(64, 0, 10)));
        magicTypeArrayIndexes.push_back(globalTypeIndexes[i]);
        Constant* typePtr = getGetElementPtrConstant(magicTypeArray, magicTypeArrayIndexes);
        arrayElems.push_back(typePtr);
    }
    arrayElems.push_back(ConstantPointerNull::get(PointerType::get(magicTypeStructTy, 0))); //NULL-terminated array

    //create the global variable and record it in the module
    Constant *arrayValue = ConstantArray::get(typeIndexesArrTy, arrayElems);
    GlobalVariable *arrayRef = new GlobalVariable(arrayValue->getType(), true,
        GlobalValue::InternalLinkage, arrayValue,
        MAGIC_HIDDEN_ARRAY_PREFIX);
    MagicUtil::setGlobalVariableSection(arrayRef, MAGIC_STATIC_VARS_SECTION_RO);
    M.getGlobalList().push_back(arrayRef);

    return arrayRef;
}

GlobalVariable* MagicUtil::getExportedIntGlobalVar(Module &M, std::string name, int value, bool isConstant) {
    Constant *intValue = ConstantInt::get(M.getContext(), APInt(32, value, 10));

    //create the global variable and record it in the module
    GlobalVariable *GV = new GlobalVariable(intValue->getType(), isConstant,
        GlobalValue::LinkOnceAnyLinkage, intValue, name);
    MagicUtil::setGlobalVariableSection(GV, isConstant ? MAGIC_STATIC_VARS_SECTION_RO : MAGIC_STATIC_VARS_SECTION_DATA);
    M.getGlobalList().push_back(GV);

    return GV;
}

GlobalVariable* MagicUtil::getShadowRef(Module &M, GlobalVariable *GV) {
    //create the shadow global variable and record it in the module
    TYPECONST Type* type = GV->getType()->getElementType();
    GlobalVariable *SGV = new GlobalVariable(type, GV->isConstant(),
                                          GlobalValue::InternalLinkage, 0,
                                          MAGIC_SHADOW_VAR_PREFIX + GV->getName());
    SGV->setInitializer(Constant::getNullValue(type));
    MagicUtil::setGlobalVariableSection(SGV, GV->isConstant() ? MAGIC_SHADOW_VARS_SECTION_RO : MAGIC_SHADOW_VARS_SECTION_DATA);
    M.getGlobalList().push_back(SGV);

    if(!GV->hasInitializer()) {
        magicUtilLog("Shadowing for extern variable: " << GV->getName());
    }
    if(GV->isConstant()) {
        magicUtilLog("Shadowing for constant variable: " << GV->getName());
    }

    return SGV;
}

Value* MagicUtil::getMagicStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* var, Value* arrayIndex, const std::string &structFieldName, std::string *structFieldNames) {
    //lookup field index
    int structFieldIndex;
    Value *varPtr;
    for(structFieldIndex=0; structFieldName.compare(structFieldNames[structFieldIndex]) != 0; structFieldIndex++) {}

    if(arrayIndex) {
        //get array ptr
        std::vector<Value*> arrayIndexes;
        arrayIndexes.push_back(ConstantInt::get(M.getContext(), APInt(64, 0, 10)));
        arrayIndexes.push_back(arrayIndex);
        varPtr = createGetElementPtrInstruction(var, arrayIndexes, "", InsertBefore);
    }
    else {
    	varPtr = var;
    }

    //get struct field ptr
    std::vector<Value*> structFieldIndexes;
    structFieldIndexes.push_back(ConstantInt::get(M.getContext(), APInt(32, 0, 10)));
    structFieldIndexes.push_back(ConstantInt::get(M.getContext(), APInt(32, structFieldIndex, 10)));
    Instruction* structFieldPtr = createGetElementPtrInstruction(varPtr, structFieldIndexes, "", InsertBefore);

    return structFieldPtr;
}

Value* MagicUtil::getMagicSStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* magicArray, Value* magicArrayIndex, const std::string &structFieldName) {
    static std::string structFieldNames[] = { MAGIC_SSTRUCT_FIELDS };
    return getMagicStructFieldPtr(M, InsertBefore, magicArray, magicArrayIndex, structFieldName, structFieldNames);
}

Value* MagicUtil::getMagicTStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* magicTypeArray, Value* magicTypeArrayIndex, const std::string &structFieldName) {
    static std::string structFieldNames[] = { MAGIC_TSTRUCT_FIELDS };
    return getMagicStructFieldPtr(M, InsertBefore, magicTypeArray, magicTypeArrayIndex, structFieldName, structFieldNames);
}

Value* MagicUtil::getMagicFStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* magicFunctionArray, Value* magicFunctionArrayIndex, const std::string &structFieldName) {
    static std::string structFieldNames[] = { MAGIC_FSTRUCT_FIELDS };
    return getMagicStructFieldPtr(M, InsertBefore, magicFunctionArray, magicFunctionArrayIndex, structFieldName, structFieldNames);
}

Value* MagicUtil::getMagicRStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* magicVar, const std::string &structFieldName) {
    static std::string structFieldNames[] = { MAGIC_RSTRUCT_FIELDS };
    return getMagicStructFieldPtr(M, InsertBefore, magicVar, NULL, structFieldName, structFieldNames);
}

Value* MagicUtil::getMagicDStructFieldPtr(Module &M, Instruction *InsertBefore, GlobalVariable* magicDsindexArray, Value* magicDsindexArrayIndex, const std::string &structFieldName) {
    static std::string structFieldNames[] = { MAGIC_DSTRUCT_FIELDS };
    return getMagicStructFieldPtr(M, InsertBefore, magicDsindexArray, magicDsindexArrayIndex, structFieldName, structFieldNames);
}

Constant* MagicUtil::getArrayPtr(Module &M, GlobalVariable* array) {
    //indexes for array
    static std::vector<Value*> arrayIndexes;
    if(arrayIndexes.empty()) {
        arrayIndexes.push_back(ConstantInt::get(M.getContext(), APInt(64, 0, 10))); //pointer to A[]
        arrayIndexes.push_back(ConstantInt::get(M.getContext(), APInt(64, 0, 10))); //pointer to A[0]
    }

    //get array ptr
    Constant* arrayPtr = getGetElementPtrConstant(array, arrayIndexes);

    return arrayPtr;
}

void MagicUtil::insertMemcpyInst(Module &M, Instruction *InsertBefore, Value *Dst, Value *Src, Value *Len, unsigned Align) {
    bool useMemCpyIntrinsics = false;
    Function *MemCpy = M.getFunction("memcpy");
    if(!MemCpy) {
        TYPECONST Type *ArgTys[1] = { IntegerType::getInt32Ty(M.getContext()) };
        MemCpy = getIntrinsicFunction(M, Intrinsic::memcpy, ArgTys, 1);
        useMemCpyIntrinsics = true;
    }
    else {
        MemCpy = (Function*) M.getOrInsertFunction(MAGIC_MEMCPY_FUNC_NAME, MemCpy->getFunctionType());
    }

    // Insert the memcpy instruction
    std::vector<Value*> MemCpyArgs;
    MemCpyArgs.push_back(Dst);
    MemCpyArgs.push_back(Src);
    MemCpyArgs.push_back(Len);
    if(useMemCpyIntrinsics) {
        MemCpyArgs.push_back(ConstantInt::get(M.getContext(), APInt(32, Align, 10)));
    }
    createCallInstruction(MemCpy, MemCpyArgs, "", InsertBefore);
}

void MagicUtil::insertCopyInst(Module &M, Instruction *InsertBefore, GlobalVariable *GV, GlobalVariable *SGV, int GVSize, bool forceMemcpy) {
    //get type and type size
    TYPECONST Type *GVType = GV->getType()->getElementType();
    bool isPrimitiveOrPointerType = !GVType->isAggregateType();

    //no need for memcpy for primitive types or pointer types
    if(isPrimitiveOrPointerType && !forceMemcpy) {
        LoadInst* primitiveValue = new LoadInst(GV, "", false, InsertBefore);
        new StoreInst(primitiveValue, SGV, false, InsertBefore);
        return;
    }

    //cast pointers to match memcpy prototype
    PointerType* voidPointerType = PointerType::get(IntegerType::get(M.getContext(), 8), 0);
    Constant* varAddress = ConstantExpr::getCast(Instruction::BitCast, GV, voidPointerType);
    Constant* varShadowAddress = ConstantExpr::getCast(Instruction::BitCast, SGV, voidPointerType);

    //insert the memcpy instruction
    MagicUtil::insertMemcpyInst(M, InsertBefore, varShadowAddress, varAddress, ConstantInt::get(M.getContext(), APInt(32, GVSize, 10)), 0);
}

Function* MagicUtil::getCalledFunctionFromCS(const CallSite &CS) {
    assert(CS.getInstruction());
    Function *function = CS.getCalledFunction();
    if(function) {
        return function;
    }

    //handle the weird case of bitcasted function call
    //IMPORTANT! function may still be null, if it's an indirect call
    ConstantExpr *CE = dyn_cast<ConstantExpr>(CS.getCalledValue());
    if (CE) {
        assert(CE->getOpcode() == Instruction::BitCast && "Bitcast expected, something else found!");
        function = dyn_cast<Function>(CE->getOperand(0));
        assert(function);
    } else {
        errs() << "Warning! Indirect call encountered!\n";
    }

    return function;
}

void MagicUtil::replaceCallInst(Instruction *originalInst, CallInst *newInst, int argOffset, bool removeUnusedFunction) {
    SmallVector< std::pair< unsigned, MDNode * >, 8> MDs;
    originalInst->getAllMetadata(MDs);
    for(unsigned i=0;i<MDs.size();i++) {
        newInst->setMetadata(MDs[i].first, MDs[i].second);
    }
    CallSite CS = MagicUtil::getCallSiteFromInstruction(originalInst);
    assert(CS);
    CallingConv::ID CC = CS.getCallingConv();
    Function *originalFunction = getCalledFunctionFromCS(CS);
    newInst->setCallingConv(CC);
    ATTRIBUTE_SET_TY NewAttrs = PassUtil::remapCallSiteAttributes(CS, argOffset);
    newInst->setAttributes(NewAttrs);

    originalInst->replaceAllUsesWith(newInst);

    // If the old instruction was an invoke, add an unconditional branch
    // before the invoke, which will become the new terminator.
    if (InvokeInst *II = dyn_cast<InvokeInst>(originalInst))
      BranchInst::Create(II->getNormalDest(), originalInst);

    // Delete the old call site
    originalInst->eraseFromParent();

    // When asked, remove the original function when nobody uses it any more.
    if(removeUnusedFunction && originalFunction->use_empty()) {
        originalFunction->eraseFromParent();
    }
}

std::vector<Function*> MagicUtil::getGlobalVariablesShadowFunctions(Module &M, std::vector<GlobalVariable*> globalVariables, std::vector<GlobalVariable*> shadowGlobalVariables, std::vector<int> globalVariableSizes, GlobalVariable* magicArray, int magicArraySize, bool forceShadow, bool setDirtyFlag) {
    std::vector<Function*> globalVariableShadowFunctions;
    for(int i=0;i<magicArraySize;i++) {
        Function* func = getGlobalVariableShadowFunction(M, globalVariables[i], shadowGlobalVariables[i], globalVariableSizes[i], magicArray, i, forceShadow, setDirtyFlag);
        globalVariableShadowFunctions.push_back(func);
    }

    return globalVariableShadowFunctions;
}

Function* MagicUtil::getGlobalVariableShadowFunction(Module &M, GlobalVariable* GV, GlobalVariable* SGV, int GVSize, GlobalVariable* magicArray, int magicArrayIndex, bool forceShadow, bool setDirtyFlag) {
    static Constant* magicStateDirty = ConstantInt::get(M.getContext(), APInt(32, MAGIC_STATE_DIRTY, 10));
    static Function* shadowFunc = NULL;
    ConstantInt* magicArrayIndexConst = ConstantInt::get(M.getContext(), APInt(32, magicArrayIndex, 10));

    //determine name
    std::string name(MAGIC_SHADOW_FUNC_PREFIX);
    name.append("_");
    if(forceShadow) {
        name.append("force_");
    }
    if(setDirtyFlag) {
        name.append("setdf_");
    }
    name.append(GV->getName());

    //create function
    std::vector<TYPECONST Type*>shadowFuncArgs;
    FunctionType* shadowFuncType = FunctionType::get(Type::getVoidTy(M.getContext()), shadowFuncArgs, false);
    shadowFunc = Function::Create(shadowFuncType, GlobalValue::InternalLinkage, name, &M);
    shadowFunc->setCallingConv(CallingConv::C);

    //create blocks
    BasicBlock* label_entry = BasicBlock::Create(M.getContext(), "entry",shadowFunc,0);
    BasicBlock* label_shadow = BasicBlock::Create(M.getContext(), "shadow",shadowFunc,0);
    BasicBlock* label_return = BasicBlock::Create(M.getContext(), "return",shadowFunc,0);
    BranchInst::Create(label_shadow, label_entry);
    BranchInst::Create(label_return, label_shadow);
    Instruction* entryTerm = label_entry->getTerminator();
    Instruction* shadowTerm = label_shadow->getTerminator();

    if(!forceShadow || setDirtyFlag) {
        //get flags
        Value* structFlagsField = MagicUtil::getMagicSStructFieldPtr(M, entryTerm, magicArray, magicArrayIndexConst, MAGIC_SSTRUCT_FIELD_FLAGS);
        LoadInst* varFlags = new LoadInst(structFlagsField, "", false, entryTerm);

        //when not forcing, don't shadow if dirty is already set
        if(!forceShadow) {
            BinaryOperator* andedVarFlags = BinaryOperator::Create(Instruction::And, varFlags, magicStateDirty, "", entryTerm);
            ICmpInst* flagsCmp = new ICmpInst(entryTerm, ICmpInst::ICMP_EQ, andedVarFlags, ConstantInt::get(M.getContext(), APInt(32, 0, 10)), "");
            BranchInst::Create(label_shadow, label_return, flagsCmp, entryTerm);
            entryTerm->eraseFromParent();
        }

        //set the dirty flag for the variable
        if(setDirtyFlag) {
            BinaryOperator* oredVarFlags = BinaryOperator::Create(Instruction::Or, varFlags, magicStateDirty, "", shadowTerm);
            new StoreInst(oredVarFlags, structFlagsField, false, shadowTerm);
        }
    }

    //perform a memory copy from the original variable to the shadow variable
    MagicUtil::insertCopyInst(M, shadowTerm, GV, SGV, GVSize, /* forceMemcpy */ false);

    ReturnInst::Create(M.getContext(), label_return);

    return shadowFunc;
}

void MagicUtil::insertGlobalVariableCleanDirtyFlag(Module &M, GlobalVariable* GV, GlobalVariable* magicArray, int magicArrayIndex, Instruction *InsertBefore) {
    Value* structFlagsField = MagicUtil::getMagicSStructFieldPtr(M, InsertBefore, magicArray, ConstantInt::get(M.getContext(), APInt(32, magicArrayIndex, 10)), MAGIC_SSTRUCT_FIELD_FLAGS);
    new StoreInst(ConstantInt::get(M.getContext(), APInt(32, 0, 10)), structFlagsField, false, InsertBefore);
}

void MagicUtil::insertShadowTag(Module &M, GlobalVariable *GV, Instruction *InsertBefore) {
    static Function* shadowFunc = NULL;
    PointerType* voidPointerType = PointerType::get(IntegerType::get(M.getContext(), 8), 0);

    //create function
    if(!shadowFunc) {
            std::vector<TYPECONST Type*>shadowFuncArgs;
            shadowFuncArgs.push_back(voidPointerType);
            FunctionType* shadowFuncType = FunctionType::get(Type::getVoidTy(M.getContext()), shadowFuncArgs, false);
            shadowFunc = Function::Create(shadowFuncType, GlobalValue::ExternalLinkage, MAGIC_LAZY_CHECKPOINT_SHADOW_TAG, &M);
            shadowFunc->setCallingConv(CallingConv::C);
    }

    //shadow global variable
    std::vector<Value*> args;
    args.push_back(new BitCastInst(GV, voidPointerType, "", InsertBefore));
    CallInst *callInst = createCallInstruction(shadowFunc, args, "", InsertBefore);
    callInst->setCallingConv(CallingConv::C);
}

bool MagicUtil::isShadowTag(Instruction *inst) {
    if(dyn_cast<CallInst>(inst)) {
            CallInst *callInst = dyn_cast<CallInst>(inst);
            Function *function = callInst->getCalledFunction();
            if(function == NULL) {
                    return false;
            }
            std::string funcName = function->getName();
            if(!funcName.compare(MAGIC_LAZY_CHECKPOINT_SHADOW_TAG)) {
                    return true;
            }
    }
    return false;
}

GlobalVariable* MagicUtil::getGlobalVariableFromShadowTag(Instruction *inst, std::vector<Instruction*> &instructionsToRemove) {
    CallSite CS = MagicUtil::getCallSiteFromInstruction(inst);
    assert(CS.arg_size() == 1);
    instructionsToRemove.push_back(inst);
    CallSite::arg_iterator AI = CS.arg_begin();
    Value *ActualArg = *AI;

    while(true) {
        BitCastInst *castInst = dyn_cast<BitCastInst>(ActualArg);
        ConstantExpr *castExpr = dyn_cast<ConstantExpr>(ActualArg);
        if(castInst) {
            assert(castInst->getNumOperands() == 1);
            ActualArg = castInst->getOperand(0);
            instructionsToRemove.push_back(castInst);
        }
        else if(castExpr) {
            //assert(castExpr->getNumOperands() == 1);
            ActualArg = castExpr->getOperand(0);
        }
        else {
            break;
        }
    }

    GlobalVariable *GV = dyn_cast<GlobalVariable>(ActualArg);
    if(GV == NULL) {
        magicUtilLog("Weird ActualArg: " << *ActualArg);
    }
    assert(GV != NULL);

    return GV;
}

void MagicUtil::cleanupShadowTag(Module &M, std::vector<Instruction*> &instructionsToRemove) {
    int i=0;

    for(i =0;i<(int)instructionsToRemove.size();i++) {
        Instruction *inst = instructionsToRemove[i];
        inst->eraseFromParent();
    }
    Function* func = M.getFunction(MAGIC_LAZY_CHECKPOINT_SHADOW_TAG);
    if(func && func->getNumUses() == 0) {
        func->eraseFromParent();
    }
}

bool MagicUtil::hasAddressTaken(const GlobalValue *GV, bool includeMembers) {
  //Most of the code taken from LLVM's SCCP.cpp

  // Delete any dead constantexpr klingons.
  GV->removeDeadConstantUsers();

  std::vector<const User*> sourceUsers;
  sourceUsers.push_back(GV);
  if(includeMembers && isa<GlobalVariable>(GV)) {
      for (const Use &UI : GV->uses()) {
          const User *U = UI.getUser();
          const ConstantExpr *constantExpr = dyn_cast<ConstantExpr>(U);
          if(isa<GetElementPtrInst>(U)) {
              sourceUsers.push_back(U);
          }
          else if(constantExpr && constantExpr->getOpcode() == Instruction::GetElementPtr) {
              sourceUsers.push_back(U);
          }
      }
  }

  for(unsigned i=0;i<sourceUsers.size();i++) {
      for (const Use &UI : sourceUsers[i]->uses()) {
        const User *U = UI.getUser();
        if (const StoreInst *SI = dyn_cast<StoreInst>(U)) {
          if (SI->getOperand(0) == sourceUsers[i] || SI->isVolatile())
            return true;  // Storing addr of sourceUsers[i].
        } else if (isa<InvokeInst>(U) || isa<CallInst>(U)) {
          // Make sure we are calling the function, not passing the address.
          ImmutableCallSite CS(cast<Instruction>(U));
          if (!CS.isCallee(&UI))
            return true;
        } else if (const LoadInst *LI = dyn_cast<LoadInst>(U)) {
          if (LI->isVolatile())
            return true;
        } else if (isa<BlockAddress>(U)) {
          // blockaddress doesn't take the address of the function, it takes addr
          // of label.
        } else {
          return true;
        }
      }
  }
  return false;
}

bool MagicUtil::lookupValueSet(const GlobalVariable *GV, std::vector<int> &valueSet) {
  //Similar to hasAddressTaken above, but we look for values

  if(!isa<IntegerType>(GV->getType()->getElementType())) {
      //integers is all we are interested in
      return false;
  }
  if(!GV->hasInitializer()) {
      //external variable
      return false;
  }

  // Delete any dead constantexpr klingons.
  GV->removeDeadConstantUsers();

  std::set<int> set;
  for (Value::const_user_iterator UI = GV->user_begin(), E = GV->user_end();
      UI != E; ++UI) {
      const User *U = *UI;
      if (const StoreInst *SI = dyn_cast<StoreInst>(U)) {
          if (SI->getOperand(1) == GV) {
             Value *value = SI->getOperand(0);
             if(ConstantInt *intValue = dyn_cast<ConstantInt>(value)) {
                 set.insert(intValue->getSExtValue());
             }
             else {
                 return false;
             }
          }
      }
  }
  const Constant *constant = GV->getInitializer();
  if(const ConstantInt *intConstant = dyn_cast<const ConstantInt>(constant)) {
      set.insert(intConstant->getSExtValue());
  }
  else {
      return false;
  }

  assert(set.size() > 0);
  valueSet.push_back(set.size()); //push length as the first value
  for(std::set<int>::iterator it=set.begin() ; it != set.end(); it++) {
      valueSet.push_back(*it);
  }

  return true;
}

Value* MagicUtil::getStringOwner(GlobalVariable *GV)
{
  //Similar to hasAddressTaken above, but we look for string owners
  assert(GV && GV->isConstant());

  // Skip emtpy strings.
  if(GV->hasInitializer() && GV->getInitializer()->isNullValue()) {
      return NULL;
  }

  // Delete any dead constantexpr klingons.
  GV->removeDeadConstantUsers();

  std::vector<User*> sourceUsers;
  sourceUsers.push_back(GV);
  for (Value::user_iterator UI = GV->user_begin(), E = GV->user_end();
      UI != E; ++UI) {
      User *U = *UI;
      ConstantExpr *constantExpr = dyn_cast<ConstantExpr>(U);
      if(isa<GetElementPtrInst>(U)) {
          sourceUsers.push_back(U);
      }
      else if(constantExpr && constantExpr->getOpcode() == Instruction::GetElementPtr) {
          sourceUsers.push_back(U);
      }
  }

  Value *stringOwner = NULL;
  for(unsigned i=0;i<sourceUsers.size();i++) {
      for (Value::user_iterator UI = sourceUsers[i]->user_begin(), E = sourceUsers[i]->user_end();
           UI != E; ++UI) {
          User *U = *UI;
          Value *V = U;
          if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
              V = SI->getPointerOperand();
          }
          if(isa<GlobalVariable>(V) || isa<AllocaInst>(V)) {
              if(stringOwner != NULL && stringOwner != V) {
                  //no owner in the ambiguous cases
                  return NULL;
              }
              stringOwner = V;
          }
      }
  }

  return stringOwner;
}

Instruction* MagicUtil::getFirstNonAllocaInst(Function *F, bool skipAllocaPoint)
{
  Instruction *I = NULL;
  if (skipAllocaPoint) {
      PassUtil::getAllocaInfo(F, NULL, &I);
  }
  else {
      PassUtil::getAllocaInfo(F, &I, NULL);
  }
  assert(I);
  return I;
}

void MagicUtil::setGlobalVariableSection(GlobalVariable *GV, const std::string &section)
{
  if(GV->isThreadLocal()) {
      return;
  }

  GV->setSection(section);
}

bool MagicUtil::getCallAnnotation(Module &M, const CallSite &CS, int *annotation)
{
  static GlobalVariable *magicAnnotationVar = NULL;
  bool instFound = false;
  bool annotationFound = false;
  if(!magicAnnotationVar) {
      magicAnnotationVar = M.getNamedGlobal(MAGIC_CALL_ANNOTATION_VAR_NAME);
      assert(magicAnnotationVar);
  }
  Instruction *I = CS.getInstruction();
  if(!I) {
      return false;
  }
  BasicBlock *parent = I->getParent();
  for (BasicBlock::iterator i = parent->begin(), e = parent->end(); i != e; ++i) {
      Instruction *inst = i;
      if(inst != I && !instFound) {
          continue;
      }
      instFound = true;
      if(inst == I) {
          continue;
      }
      if(StoreInst *SI = dyn_cast<StoreInst>(inst)) {
          if(SI->getOperand(1) == magicAnnotationVar) {
              ConstantInt *CI = dyn_cast<ConstantInt>(SI->getOperand(0));
              assert(CI && "Bad call annotation!");
              annotationFound = true;
              *annotation = CI->getSExtValue();
              break;
          }
      }
      else if(isa<CallInst>(inst) || isa<InvokeInst>(inst)) {
          break;
      }
  }
  return annotationFound;
}

bool MagicUtil::getVarAnnotation(Module &M, const GlobalVariable *GV, int *annotation)
{
  std::string GVName = GV->getName();
  GlobalVariable *annotationGV = M.getNamedGlobal(MAGIC_VAR_ANNOTATION_PREFIX_NAME + GVName);
  if(!annotationGV || !annotationGV->hasInitializer()) {
      return false;
  }
  ConstantInt* annotationValue = dyn_cast<ConstantInt>(annotationGV->getInitializer());
  if(!annotationValue) {
      return false;
  }
  *annotation = (int) annotationValue->getSExtValue();
  return true;
}

CallSite MagicUtil::getCallSiteFromInstruction(Instruction *I)
{
  return PassUtil::getCallSiteFromInstruction(I);
}

AllocaInst* MagicUtil::getAllocaInstFromArgument(Argument *argument)
{
  Function *parent = argument->getParent();
  std::string targetString = argument->getName().str() + "_addr";
  std::string targetString2 = argument->getName().str() + ".addr";
  StringRef targetName(targetString);
  StringRef targetName2(targetString2);
  for (inst_iterator it = inst_begin(parent), et = inst_end(parent); it != et; ++it) {
      AllocaInst *AI = dyn_cast<AllocaInst>(&(*it));
      if(!AI) {
          break;
      }
      if(AI->getName().startswith(targetName) || AI->getName().startswith(targetName2)) {
          return AI;
      }
  }

  return NULL;
}

// searches for the specified function in module symbol table assuming that its name has been mangled
// returns NULL if the function has not been found
Function* MagicUtil::getMangledFunction(Module &M, StringRef functionName)
{
	Function *F = NULL;
	char* outbuf;
	const char* functionNameString = functionName.data();
	int status;
	for (Module::iterator it = M.begin(); it != M.end(); ++it) {
		StringRef mangledName = (*it).getName();
		outbuf = abi::__cxa_demangle(mangledName.data(), NULL, NULL, &status);
		if (status == -2) {
			continue; // mangledName is not a valid name under the C++ ABI mangling rules
		}
		assert(status == 0 && outbuf && "Error when trying to demangle a function name.");
		// testing whether this is the function we are looking for
		// the unmangled name is similar to a function prototype eg my_func(int, void*, int)
		char* pos = strstr(outbuf, functionNameString);
		if (!pos) {
			free(outbuf);
			continue;
		}
		// function names can only contain alpha-numeric characters and '_'
		// if the unmangled name refers to the target function, then that substring should not
		// be surrounded by characters allowed in a function name
		// (to exclude cases such as myfunc vs _myfunc vs myfunc2)
		if (pos > outbuf) {
			if (isalnum(*(pos - 1)) || (*(pos - 1) == '_')) {
				free(outbuf);
				continue;
			}
		}
		if (strlen(pos) > strlen(functionNameString)) {
			if (isalnum(*(pos + strlen(functionNameString))) || (*(pos + strlen(functionNameString)) == '_')) {
				free(outbuf);
				continue;
			}
		}
		F = it;
		free(outbuf);
		break;
	}

	return F;
}

Function* MagicUtil::getFunction(Module &M, StringRef functionName)
{
	Function* F = M.getFunction(functionName);
	if (!F) {
		F = MagicUtil::getMangledFunction(M, functionName);
	}
	return F;
}

// can Type1 be represented as Type2 (with no precision loss)
bool MagicUtil::isCompatibleType(const Type* Type1, const Type* Type2)
{
	if (Type1 == Type2) {
		return true;
	}
	if (Type1->isIntegerTy() && Type2->isIntegerTy()) {
		if (((const IntegerType*)Type1)->getBitWidth() <= ((const IntegerType*)Type2)->getBitWidth()) {
			return true;
		}
	}

	return false;
}

// inserts an inlined call to the pre-hook function before any other instruction is executed
// it can forward (some of) the original function's parameters and additional trailing arguments
void MagicUtil::inlinePreHookForwardingCall(Function* function, Function* preHookFunction, std::vector<unsigned> argsMapping, std::vector<Value*> trailingArgs)
{
	std::vector<Value*> callArgs;
	assert(preHookFunction->arg_size() == argsMapping.size() + trailingArgs.size() &&
			"The number of parameter values specified for the pre-hook function does not match the signature of the function.");
	for (std::vector<unsigned>::iterator it = argsMapping.begin(); it != argsMapping.end(); it++) {
		callArgs.push_back(MagicUtil::getFunctionParam(function, *it - 1));
	}
	for (std::vector<Value*>::iterator it = trailingArgs.begin(); it != trailingArgs.end(); it++) {
		callArgs.push_back(*it);
	}
	// insert the call after the alloca instructions so that they remain for sure in the entry block
	Instruction *FirstInst = MagicUtil::getFirstNonAllocaInst(function);
	for (unsigned i = 0; i < callArgs.size(); ++i) {
		TYPECONST Type* ArgType = callArgs[i]->getType();
		TYPECONST Type* ParamType = preHookFunction->getFunctionType()->getParamType(i);

		if (!MagicUtil::isCompatibleType(ArgType, ParamType)) {
			assert(CastInst::isCastable(ArgType, ParamType) && "The value of the argument cannot be "
					"casted to the parameter type required by the function to be called.");
			Instruction::CastOps CastOpCode = CastInst::getCastOpcode(callArgs[i], false, ParamType, false);
			callArgs[i] = CastInst::Create(CastOpCode, callArgs[i], ParamType, "", FirstInst);
		}
	}

	CallInst* WrapperFuncCall = MagicUtil::createCallInstruction(preHookFunction, callArgs, "", FirstInst);
	InlineFunctionInfo IFI;
	InlineFunction(WrapperFuncCall, IFI);
}

// inserts an inlined call to the post-hook function before all return instructions
// forwarded arguments from the first function come first, followed by the trailing ones
// use offsets > 0 for function parameter mappings, and 0 for the return value of the function
void MagicUtil::inlinePostHookForwardingCall(Function* function, Function* postHookFunction, std::vector<unsigned> mapping, std::vector<Value*> trailingArgs)
{
	std::vector<CallInst*> wrapperCalls;
	assert(postHookFunction->arg_size() == mapping.size() + trailingArgs.size()
			&& "The number of parameter values specified for the post-hook function does not match the signature of the function.");

	for (Function::iterator BI = function->getBasicBlockList().begin(); BI != function->getBasicBlockList().end(); ++BI) {
		ReturnInst *RetInst = dyn_cast<ReturnInst>(BI->getTerminator());
		if (RetInst) {
			std::vector<Value*> callArgs;
			for (std::vector<unsigned>::iterator it = mapping.begin(); it != mapping.end(); it++) {
				if (*it > 0) {
					callArgs.push_back(MagicUtil::getFunctionParam(function, *it - 1));
				} else {
					callArgs.push_back(RetInst->getReturnValue());
				}
			}
			for (std::vector<Value*>::iterator it = trailingArgs.begin(); it != trailingArgs.end(); it++) {
				callArgs.push_back(*it);
			}
			for (unsigned i = 0; i < callArgs.size(); i++) {
				TYPECONST Type* ArgType = callArgs[i]->getType();
				TYPECONST Type* ParamType = postHookFunction->getFunctionType()->getParamType(i);

				if (!MagicUtil::isCompatibleType(ArgType, ParamType)) {
					assert(CastInst::isCastable(ArgType, ParamType) && "The value of the argument cannot be "
							"casted to the parameter type required by the function to be called.");
					Instruction::CastOps CastOpCode = CastInst::getCastOpcode(callArgs[i], false, ParamType, false);
					callArgs[i] = CastInst::Create(CastOpCode, callArgs[i], ParamType, "", RetInst);
				}
			}
			CallInst* WrapperFuncCall = MagicUtil::createCallInstruction(postHookFunction, callArgs, "", RetInst);
			wrapperCalls.push_back(WrapperFuncCall);
		}
	}
	for (std::vector<CallInst*>::iterator it = wrapperCalls.begin(); it != wrapperCalls.end(); ++it) {
		InlineFunctionInfo IFI;
		InlineFunction(*it, IFI);
	}
}

int MagicUtil::getPointerIndirectionLevel(const Type* type)
{
	int level = 0;
	if (const PointerType* ptr_type = dyn_cast<PointerType>(type)) {
		while (ptr_type) {
			level++;
			ptr_type = dyn_cast<PointerType>(ptr_type->getElementType());
		}
	}

	return level;
}

Value* MagicUtil::getFunctionParam(Function* function, unsigned index)
{
	if (index >= function->arg_size()) {
		return NULL;
	}
	Function::arg_iterator AI = function->arg_begin();
	while (index --> 0) {
		AI++;
	}
	return AI;
}

bool MagicUtil::isLocalConstant(Module &M, GlobalVariable *GV)
{
	if (!GV->isConstant()) {
		return false;
	}
	if (GV->getName().endswith(".v")) {
		return true;
	}
	std::pair<StringRef, StringRef> stringPair = GV->getName().split('.');
	StringRef functionName = stringPair.first;
	if (!functionName.compare("") || M.getFunction(functionName) == NULL) {
		return false;
	}

	return true;
}

}
