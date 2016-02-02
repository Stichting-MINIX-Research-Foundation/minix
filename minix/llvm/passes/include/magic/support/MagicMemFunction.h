#ifndef MAGIC_MEM_FUNCTION_H
#define MAGIC_MEM_FUNCTION_H

#include <pass.h>
#include <magic/support/TypeInfo.h>

#define NUM_MAGIC_ARGS  3

using namespace llvm;

namespace llvm {

class MagicMemFunction {
public:
	MagicMemFunction(Module &M, Function *function, Function *wrapper, bool isDealloc, bool isNested, int allocFlags);

	Function* getFunction() const;
	Function* getWrapper() const;
	bool isDeallocFunction() const;
	bool isNestedFunction() const;
	int getAllocFlags() const;
	Instruction* getInstruction() const;
	Function* getInstructionParent() const;
	TypeInfo* getInstructionTypeInfo() const;
	Value* getInstructionTypeValue() const;
	bool hasInstructionType() const;
	std::vector<MagicMemFunction> getInstructionDeps() const;

	void setInstruction(Instruction* I);
	void setInstructionTypeInfo(TypeInfo* aTypeInfo, std::string &allocName, std::string &allocParentName);
	void setInstructionTypeValue(Value* typeValue, Value* allocNameValue, Value* allocParentNameValue);
	void addInstructionDep(MagicMemFunction &function);
	void replaceInstruction(std::map<TypeInfo*, Constant*> &magicArrayTypePtrMap, TypeInfo *voidPtrTypeInfo);

	void print(raw_ostream &OS) const;
	void printDescription(raw_ostream &OS) const;
	const std::string getDescription() const;

	static int getMemFunctionPointerParam(Function* function, std::set<Function*> &brkFunctions, TypeInfo *voidPtrTypeInfo);
	static Function* getCustomWrapper(Function* function, Function* stdFunction, Function* stdWrapper, std::vector<unsigned> argMapping,
			bool isDealloc);
	static bool isCustomWrapper(Function *function);

private:
	Module *module;
	Function *function;
	Function *wrapper;
	bool isDealloc;
	bool isNested;
	int allocFlags;
	Instruction *instruction;
	TypeInfo* aTypeInfo;
	std::string allocName;
	std::string allocParentName;
	Value* typeValue;
	Value* allocNameValue;
	Value* allocParentNameValue;
	std::vector<MagicMemFunction> instructionDeps;

	void buildWrapper(std::map<TypeInfo*, Constant*> &magicArrayTypePtrMap, TypeInfo *voidPtrTypeInfo);

	static Function *lastAllocWrapper;
	static std::map<std::string, Function*> allocWrapperCache;
	static std::set<Function*> customWrapperSet;
};

inline raw_ostream &operator<<(raw_ostream &OS, const MagicMemFunction &aMagicMemFunction) {
	aMagicMemFunction.print(OS);
	return OS;
}

inline void MagicMemFunction::print(raw_ostream &OS) const {
	OS << getDescription();
}

inline void MagicMemFunction::printDescription(raw_ostream &OS) const {
	OS << "[ function = ";
	OS << function->getName() << "(" << TypeUtil::getDescription(function->getFunctionType()) << ")";
	OS << ", wrapper = ";
	if (wrapper) {
		OS << wrapper->getName() << "(" << TypeUtil::getDescription(wrapper->getFunctionType()) << ")";
	} else
		OS << "NULL";
	OS << ", isDeallocFunction = ";
	OS << isDealloc;
	OS << ", isNestedFunction = ";
	OS << isNested;
	OS << ", instruction = ";
	if (instruction)
		instruction->print(OS);
	else
		OS << "NULL";
	OS << ", typeInfo = ";
	if (aTypeInfo)
		OS << aTypeInfo->getDescription();
	else
		OS << "NULL";
	OS << ", allocName = ";
	OS << allocName;
	OS << ", allocParentName = ";
	OS << allocParentName;
	OS << ", typeValue = ";
	if (typeValue)
		typeValue->print(OS);
	else
		OS << "NULL";
	OS << ", allocNameValue = ";
	if (allocNameValue)
		allocNameValue->print(OS);
	else
		OS << "NULL";
	OS << ", allocParentNameValue = ";
	if (allocParentNameValue)
		allocParentNameValue->print(OS);
	else
		OS << "NULL";
	OS << ", instructionDeps = {";
	for (unsigned i = 0; i < instructionDeps.size(); i++) {
		if (i > 0) {
			OS << ", ";
		}
		instructionDeps[i].print(OS);
	}
	OS << "}]";
}

inline const std::string MagicMemFunction::getDescription() const {
	std::string string;
	raw_string_ostream ostream(string);
	printDescription(ostream);
	ostream.flush();
	return string;
}

inline MagicMemFunction::MagicMemFunction(Module &M, Function *function, Function *wrapper, bool isDealloc, bool isNested, int allocFlags) {
	this->module = &M;
	this->function = function;
	this->wrapper = wrapper;
	this->isDealloc = isDealloc;
	this->isNested = isNested;
	this->allocFlags = allocFlags;
	this->instruction = NULL;
	this->aTypeInfo = NULL;
	this->allocName = "";
	this->allocParentName = "";
	this->typeValue = NULL;
	this->allocNameValue = NULL;
	this->allocParentNameValue = NULL;
	assert(function);
	if (wrapper && !isDealloc && !isNested) {
		lastAllocWrapper = wrapper;
	}
	if (isDealloc) {
		assert(!allocFlags);
	}
}

inline Function* MagicMemFunction::getFunction() const {
	return function;
}

inline Function* MagicMemFunction::getWrapper() const {
	return wrapper;
}

inline bool MagicMemFunction::isDeallocFunction() const {
	return isDealloc;
}

inline bool MagicMemFunction::isNestedFunction() const {
	return isNested;
}

inline int MagicMemFunction::getAllocFlags() const {
	return allocFlags;
}

inline Instruction* MagicMemFunction::getInstruction() const {
	return instruction;
}

inline Function* MagicMemFunction::getInstructionParent() const {
	if (!instruction) {
		return NULL;
	}
	return instruction->getParent()->getParent();
}

inline TypeInfo* MagicMemFunction::getInstructionTypeInfo() const {
	return aTypeInfo;
}

inline Value* MagicMemFunction::getInstructionTypeValue() const {
	return typeValue;
}

inline bool MagicMemFunction::hasInstructionType() const {
	return aTypeInfo || typeValue;
}

inline std::vector<MagicMemFunction> MagicMemFunction::getInstructionDeps() const {
	return instructionDeps;
}

inline void MagicMemFunction::setInstruction(Instruction* I) {
	this->instruction = I;
	assert(isa<CallInst>(instruction) || isa<InvokeInst>(instruction));
}

inline void MagicMemFunction::setInstructionTypeInfo(TypeInfo* aTypeInfo, std::string &allocName, std::string &allocParentName) {
	this->aTypeInfo = aTypeInfo;
	this->allocName = allocName;
	this->allocParentName = allocParentName;
}

inline void MagicMemFunction::setInstructionTypeValue(Value* typeValue, Value* allocNameValue, Value* allocParentNameValue) {
	this->typeValue = typeValue;
	this->allocNameValue = allocNameValue;
	this->allocParentNameValue = allocParentNameValue;
}

inline void MagicMemFunction::addInstructionDep(MagicMemFunction &function) {
	assert(wrapper == NULL && "Dependencies are resolved at wrapper building time, so wrapper has to be NULL!");
	instructionDeps.push_back(function);
	allocFlags |= function.getAllocFlags();
}

inline void MagicMemFunction::replaceInstruction(std::map<TypeInfo*, Constant*> &magicArrayTypePtrMap, TypeInfo *voidPtrTypeInfo) {
	Instruction *I = getInstruction();
	assert(I);
	CallSite CS = MagicUtil::getCallSiteFromInstruction(I);
	std::vector<Value*> magicMemArgs;
	unsigned numMagicArgs = 0;
	//if we do not have a wrapper, build one
	if (!wrapper) {
		buildWrapper(magicArrayTypePtrMap, voidPtrTypeInfo);
	}
	//inject magic args
	if (!isDeallocFunction() && !isNestedFunction()) {
		std::map<TypeInfo*, Constant*>::iterator it;
		if (!typeValue) {
			assert(aTypeInfo);
			if (aTypeInfo == voidPtrTypeInfo->getContainedType(0)) {
				typeValue = ConstantPointerNull::get((TYPECONST PointerType*) (wrapper->arg_begin()->getType()));
			} else {
				it = magicArrayTypePtrMap.find(aTypeInfo);
				assert(it != magicArrayTypePtrMap.end());
				typeValue = it->second;
			}
			assert(allocName.compare(""));
			assert(allocParentName.compare(""));
			allocNameValue = MagicUtil::getArrayPtr(*module, MagicUtil::getStringRef(*module, allocName));
			allocParentNameValue = MagicUtil::getArrayPtr(*module, MagicUtil::getStringRef(*module, allocParentName));
		}
		magicMemArgs.push_back(typeValue);
		magicMemArgs.push_back(allocNameValue);
		magicMemArgs.push_back(allocParentNameValue);
		numMagicArgs = NUM_MAGIC_ARGS;
	}
	//push other args
	unsigned arg_size = MagicUtil::getCalledFunctionFromCS(CS)->getFunctionType()->getNumContainedTypes() - 1;
	for (unsigned i = 0; i < arg_size; i++) {
		Value *arg = CS.getArgument(i);
		TYPECONST Type* wArgType = wrapper->getFunctionType()->getContainedType(i + numMagicArgs + 1);
		if (arg->getType() != wArgType) {
			if (arg->getType()->isPointerTy()) {
		            assert(wArgType->isPointerTy());
		            arg = CastInst::CreatePointerCast(arg, wArgType, "WrapperCast", I);
			}
			else {
			    assert(arg->getType()->isIntegerTy());
			    assert(wArgType->isIntegerTy());
			    arg = CastInst::CreateIntegerCast(arg, wArgType, false, "WrapperCast", I);
			}
		}
		magicMemArgs.push_back(arg);
	}
	//replace function with wrapper
	CallInst* newInst = MagicUtil::createCallInstruction(wrapper, magicMemArgs, "", I);
	newInst->takeName(I);
	MagicUtil::replaceCallInst(I, newInst, NUM_MAGIC_ARGS);
}

inline int MagicMemFunction::getMemFunctionPointerParam(Function* function, std::set<Function*> &brkFunctions, TypeInfo *voidPtrTypeInfo) {
	TYPECONST Type *type = function->getReturnType();
	if (type == voidPtrTypeInfo->getType()) {
		return 0;
	} else if (brkFunctions.find(function) != brkFunctions.end()) {
		return 1;
	} else {
		unsigned i;
		for (i = 1; i < function->getFunctionType()->getNumContainedTypes(); i++) {
			type = function->getFunctionType()->getContainedType(i);
			if (type->isPointerTy() && type->getContainedType(0) == voidPtrTypeInfo->getType()) {
				return i;
			}
		}
	}

	return -1;
}

inline void MagicMemFunction::buildWrapper(std::map<TypeInfo*, Constant*> &magicArrayTypePtrMap, TypeInfo *voidPtrTypeInfo) {
	assert(!isDeallocFunction());
	assert(!isNestedFunction());
	assert(lastAllocWrapper);
	std::vector<TYPECONST Type*> ArgTypes;
	VALUE_TO_VALUE_MAP_TY VMap;

	std::map<std::string, Function*>::iterator allocWrapperCacheIt;

	// See if the wrapper is in cache, otherwise create a new wrapper using function cloning
	allocWrapperCacheIt = allocWrapperCache.find(function->getName());
	if (allocWrapperCacheIt != allocWrapperCache.end()) {
		wrapper = allocWrapperCacheIt->second;
		return;
	}

	// Build arg types for wrapper
	Function::const_arg_iterator E = lastAllocWrapper->arg_begin();
	for (unsigned i = 0; i < NUM_MAGIC_ARGS; i++)
		E++;
	for (Function::const_arg_iterator I = lastAllocWrapper->arg_begin(); I != E; ++I)
		ArgTypes.push_back(I->getType());
	E = function->arg_end();
	for (Function::const_arg_iterator I = function->arg_begin(); I != E; ++I)
		ArgTypes.push_back(I->getType());

	// Create a new function type...
	FunctionType *FTy = FunctionType::get(function->getFunctionType()->getReturnType(), ArgTypes, function->getFunctionType()->isVarArg());

	// Create the wrapper
	wrapper = Function::Create(FTy, function->getLinkage(), "magic_" + function->getName(), function->getParent());

	// Loop over the arguments, copying the names of the mapped arguments over...
	Function::arg_iterator DestI = wrapper->arg_begin();
	Value *magicTypeValue = DestI;
	magicTypeValue->setName("magic_type");
	DestI++;
	Value *magicNameValue = DestI;
	magicNameValue->setName("magic_name");
	DestI++;
	Value *magicParentNameValue = DestI;
	magicParentNameValue->setName("magic_parent_name");
	DestI++;
	for (Function::const_arg_iterator I = function->arg_begin(), E = function->arg_end(); I != E; ++I) {
		DestI->setName(I->getName());
		VMap[I] = DestI++;
	}

	SmallVector<ReturnInst*, 8> Returns; // Ignore returns cloned...
	CloneFunctionInto(wrapper, function, VMap, false, Returns, "", NULL);

	allocWrapperCache.insert(std::pair<std::string, Function*>(function->getName(), wrapper));

    // Create a mapping between the function instruction pointers and the wrapper instruction pointers
    std::vector<Instruction *> wrapperInstructionDeps;
    for (unsigned i = 0; i < instructionDeps.size(); i++) {
        Instruction *instruction = instructionDeps[i].getInstruction();
        Instruction *wrapperInstruction = NULL;
        unsigned instructionOffset = 0;
        for (inst_iterator I = inst_begin(function), E = inst_end(function); I != E; ++I, instructionOffset++) {
            if (instruction == &(*I)) {
                break;
            }
        }
        assert(instructionOffset > 0);
        for (inst_iterator I = inst_begin(wrapper), E = inst_end(wrapper); I != E; ++I, instructionOffset--) {
            if (instructionOffset == 0) {
                wrapperInstruction = &(*I);
                break;
            }
        }
        assert(wrapperInstruction);
        wrapperInstructionDeps.push_back(wrapperInstruction);
    }

    // Forward magic type argument to any dependent instruction and replace it
    for (unsigned i = 0; i < wrapperInstructionDeps.size(); i++) {
        instructionDeps[i].setInstruction(wrapperInstructionDeps[i]);
        instructionDeps[i].setInstructionTypeValue(magicTypeValue, magicNameValue, magicParentNameValue);
        instructionDeps[i].replaceInstruction(magicArrayTypePtrMap, voidPtrTypeInfo);
    }
}

inline Function* MagicMemFunction::getCustomWrapper(Function* function, Function* stdFunction, Function* stdWrapper, std::vector<unsigned> argMapping,
		bool isDealloc) {
	Function* wrapper;
	std::vector<TYPECONST Type*> ArgTypes;
	VALUE_TO_VALUE_MAP_TY VMap;

	// Build arg types for wrapper
	// add magic arguments
	if (!isDealloc) {
		Function::const_arg_iterator E = stdWrapper->arg_begin();
		for (unsigned i = 0; i < NUM_MAGIC_ARGS; i++)
			E++;
		for (Function::const_arg_iterator I = stdWrapper->arg_begin(); I != E; ++I) {
			ArgTypes.push_back(I->getType());
		}
	}
	// add original function arguments
	for (Function::const_arg_iterator I = function->arg_begin(), E = function->arg_end(); I != E; ++I) {
		ArgTypes.push_back(I->getType());
	}

	// Create a new function type...
	FunctionType *FTy = FunctionType::get(stdWrapper->getFunctionType()->getReturnType(), ArgTypes, function->getFunctionType()->isVarArg());

	// Create the wrapper
	wrapper = Function::Create(FTy, function->getLinkage(), "magic_" + function->getName(), function->getParent());

	// Loop over the arguments, copying the names of the mapped arguments over...
	Function::arg_iterator DestI = wrapper->arg_begin();
	std::vector<Value*> wrapperArgs;
	if (!isDealloc) {
		std::string magicArgs[] = { "magic_type", "magic_name", "magic_parent_name" };
		for (unsigned i = 0; i < NUM_MAGIC_ARGS; i++) {
			DestI->setName(magicArgs[i]);
			wrapperArgs.push_back(DestI);
			DestI++;
		}
	}
	for (Function::const_arg_iterator I = function->arg_begin(), E = function->arg_end(); I != E; ++I) {
		DestI->setName(I->getName());
		wrapperArgs.push_back(DestI);
		DestI++;
	}

	// map the arguments of the standard wrapper to the arguments of the new custom wrapper
	if ((!isDealloc) || argMapping.size()) {
		Function::const_arg_iterator W = stdWrapper->arg_begin();
		if (!isDealloc) {
			// magic arguments are in the same position
			for (unsigned i = 0; i < NUM_MAGIC_ARGS; i++) {
				VMap[W] = wrapperArgs[i];
				W++;
			}
		}
		// map the selected arguments of the custom wrapper using the mapping provided as input
		unsigned argOffset = isDealloc ? 0 : NUM_MAGIC_ARGS;
		for (unsigned i = 0; i < argMapping.size(); i++) {
			VMap[W] = wrapperArgs[argOffset + argMapping[i] - 1];
			W++;
		}
	}

	SmallVector<ReturnInst*, 8> Returns; // Ignore returns cloned...
	CloneFunctionInto(wrapper, stdWrapper, VMap, false, Returns, "", NULL);

	// check whether some of the arguments of the custom wrapper need to be casted
	// in order to match the basic wrapper implementation
	Instruction *FirstInst = MagicUtil::getFirstNonAllocaInst(wrapper);
	Function::const_arg_iterator W = stdWrapper->arg_begin();
	unsigned argOffset = 0;
	if (!isDealloc) {
		argOffset = NUM_MAGIC_ARGS;
		// skip the magic arguments, they are always the same
		for (unsigned i = 0; i < NUM_MAGIC_ARGS; i++) {
			W++;
		}
	}
	for (unsigned i = 0; i < argMapping.size(); i++) {
		TYPECONST Type* StdParamType = W->getType();
		Value* ParamValue = wrapperArgs[argOffset + argMapping[i] - 1];
		TYPECONST Type* ParamType = ParamValue->getType();
		if (!MagicUtil::isCompatibleType(ParamType, StdParamType)) {
			assert(CastInst::isCastable(ParamType, StdParamType) && "The type of the parameter of the custom wrapper "
			"cannot be casted to the type of the basic wrapper to which it is corresponding.");
			Instruction::CastOps CastOpCode = CastInst::getCastOpcode(ParamValue, false, StdParamType, false);
			Instruction *ParamCastInst = CastInst::Create(CastOpCode, ParamValue, StdParamType, "", FirstInst);

			for (Value::use_iterator it = ParamValue->use_begin(); it != ParamValue->use_end(); it++) {
				if (Constant * C = dyn_cast<Constant>(*it)) {
					if (!isa<GlobalValue>(C)) {
						C->replaceUsesOfWith(ParamValue, ParamCastInst);
						continue;
					}
				}
				Instruction *I = dyn_cast<Instruction>(*it);
				if (I && (I != ParamCastInst)) {
					// replace all uses, except for the calls to the wrapped function
					CallInst *CI = dyn_cast<CallInst>(I);
					if (CI && (CI->getCalledFunction() == function)) {
						continue;
					}
					I->replaceUsesOfWith(ParamValue, ParamCastInst);
				}
			}
		}
		W++;
	}

	// replace the call(s) to the standard function with calls to our function
	for (Function::iterator BI = wrapper->getBasicBlockList().begin(), BE = wrapper->getBasicBlockList().end(); BI != BE; ++BI) {
		unsigned pos = 0;
		unsigned bbSize = BI->getInstList().size();
		while (pos < bbSize) {
			BasicBlock::iterator it = BI->getInstList().begin();
			for (unsigned i = 0; i < pos; i++) {
				it++;
			}
			Instruction *inst = &(*it);
			// find the calls to the standard function
			CallInst *callInst = dyn_cast<CallInst>(inst);
			if (callInst && callInst->getCalledFunction() && (callInst->getCalledFunction()->getFunctionType() == stdFunction->getFunctionType())
					&& (!callInst->getCalledFunction()->getName().compare(stdFunction->getName()))) {
				CallSite CS = MagicUtil::getCallSiteFromInstruction(callInst);
				unsigned numStdParams = stdFunction->getFunctionType()->getNumParams();
				unsigned numParams = function->getFunctionType()->getNumParams();
				// construct the parameter array
				std::vector<Value*> callArgs(numParams, NULL);
				// first add the arguments that are common to the custom and standard function
				// add casts where necessary
				for (unsigned i = 0; i < numStdParams; i++) {
					Value *argValue = CS.getArgument(i);
					TYPECONST Type* paramType = function->getFunctionType()->getParamType(i);
					TYPECONST Type* argType = argValue->getType();
					if (paramType != argType) {
						assert(CastInst::isCastable(argType, paramType) && "The value of the argument cannot be "
						"casted to the parameter type required by the function to be called.");
						Instruction::CastOps opcode = CastInst::getCastOpcode(argValue, false, paramType, false);
						argValue = CastInst::Create(opcode, argValue, paramType, "", callInst);
					}
					callArgs[argMapping[i] - 1] = argValue;
				}
				// the other arguments are just forwarded from the wrapper's argument list
				// skip the magic arguments of the wrapper from the beginning of the argument list
				unsigned argOffset = isDealloc ? 0 : NUM_MAGIC_ARGS;
				for (unsigned i = argOffset; i < wrapper->getFunctionType()->getNumParams(); i++) {
					if (callArgs[i - argOffset] == NULL) {
						Value* arg = wrapperArgs[i];
						callArgs[i - argOffset] = arg;
					}
				}

				CallInst* newCallInst = MagicUtil::createCallInstruction(function, callArgs, "", callInst);
				newCallInst->takeName(callInst);
				MagicUtil::replaceCallInst(callInst, newCallInst, argOffset);
			}
			pos++;
		}
	}

	customWrapperSet.insert(wrapper);
	return wrapper;
}

inline bool MagicMemFunction::isCustomWrapper(Function *function)
{
	return customWrapperSet.find(function) != customWrapperSet.end();
}

}

#endif

