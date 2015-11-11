#ifndef MAGIC_DEBUG_FUNCTION_H_
#define MAGIC_DEBUG_FUNCTION_H_

#include <pass.h>

#define NUM_DEBUG_ARGS 1

using namespace llvm;

namespace llvm {

class MagicDebugFunction {
public:
	MagicDebugFunction(Function *function);

	Function* getFunction() const;
	void addHooks(std::pair<Function*, Function*> hooks, unsigned flags, std::vector<unsigned> argsMapping, std::vector<Value*> trailingArgs);
	void fixCalls(Module &M, const std::string &baseDir="");

	void print(raw_ostream &OS) const;
	void printDescription(raw_ostream &OS) const;
	const std::string getDescription() const;
	static bool inlineHookCalls(Function* function, std::pair<Function*, Function*> hooks, unsigned flags, std::vector<unsigned> argsMapping,
			std::vector<Value*> trailingArgs);
	Function* getDebugFunction(Module &M);

private:
	Function *function;

	std::pair<Function*, Function*> hooks;
	unsigned flags;
	std::vector<unsigned> argsMapping;
	std::vector<Value*> trailingArgs;

	Function* getDebugClone(Function* function, const Twine wrapperName, TYPECONST Type* debugArgType);
};

inline raw_ostream &operator<<(raw_ostream &OS, const MagicDebugFunction &aMagicDebugFunction) {
	aMagicDebugFunction.print(OS);
	return OS;
}

inline void MagicDebugFunction::print(raw_ostream &OS) const {
	OS << getDescription();
}

inline void MagicDebugFunction::printDescription(raw_ostream &OS) const {
	OS << "[ function = ";
	OS << function->getName() << "(" << TypeUtil::getDescription(function->getFunctionType()) << ") ]";
}

inline const std::string MagicDebugFunction::getDescription() const {
	std::string string;
	raw_string_ostream ostream(string);
	printDescription(ostream);
	ostream.flush();
	return string;
}

inline Function* MagicDebugFunction::getDebugClone(Function* function, const Twine wrapperName, TYPECONST Type* debugArgType) {
	Function* wrapper;
	std::vector<TYPECONST Type*> ArgTypes;
	VALUE_TO_VALUE_MAP_TY VMap;

	// Build arg types for wrapper
	ArgTypes.push_back(debugArgType);
	Function::const_arg_iterator E = function->arg_end();
	for (Function::const_arg_iterator I = function->arg_begin(); I != E; ++I)
		ArgTypes.push_back(I->getType());

	// Create a new function type...
	FunctionType *FTy = FunctionType::get(function->getFunctionType()->getReturnType(), ArgTypes, function->getFunctionType()->isVarArg());

	// Create the wrapper
	wrapper = Function::Create(FTy, function->getLinkage(), wrapperName, function->getParent());

	// Loop over the arguments, copying the names of the mapped arguments over...
	Function::arg_iterator DestI = wrapper->arg_begin();
	Value *magicTypeValue = DestI;
	magicTypeValue->setName("cs_info");
	DestI++;
	for (Function::const_arg_iterator I = function->arg_begin(), E = function->arg_end(); I != E; ++I) {
		DestI->setName(I->getName());
		VMap[I] = DestI++;
	}

	SmallVector<ReturnInst*, 8> Returns; // Ignore returns cloned...
	CloneFunctionInto(wrapper, function, VMap, false, Returns, "", NULL);
	return wrapper;
}

inline MagicDebugFunction::MagicDebugFunction(Function *function) {
	this->function = function;
}

inline void MagicDebugFunction::addHooks(std::pair<Function*, Function*> aHooks, unsigned aFlags, std::vector<unsigned> aArgsMapping,
		std::vector<Value*> aTrailingArgs) {
	hooks = aHooks;
	flags = aFlags;
	trailingArgs = aTrailingArgs;
	argsMapping = aArgsMapping;
}

inline Function* MagicDebugFunction::getFunction() const {
	return function;
}

inline Function* MagicDebugFunction::getDebugFunction(Module &M) {
	PointerType* PointerTy = PointerType::get(IntegerType::get((&M)->getContext(), 8), 0);
	Function* debugFunction = MagicDebugFunction::getDebugClone(function, "debug_magic_" + function->getName(), PointerTy);
	bool ret = MagicDebugFunction::inlineHookCalls(debugFunction, hooks, flags, argsMapping, trailingArgs);
	if (ret) {
		return debugFunction;
	} else {
		return NULL;
	}
}

inline void MagicDebugFunction::fixCalls(Module &M, const std::string &baseDir) {
	PointerType* PointerTy = PointerType::get(IntegerType::get((&M)->getContext(), 8), 0);
	Function* debugFunction = MagicDebugFunction::getDebugClone(function, "debug_magic_" + function->getName(), PointerTy);
	bool ret = MagicDebugFunction::inlineHookCalls(debugFunction, hooks, flags, argsMapping, trailingArgs);
	assert(ret && "Unable to inline the calls to the hook functions.");

	std::vector<User*> Users(function->user_begin(), function->user_end());
	std::vector<Value*> EqPointers;
	while (!Users.empty()) {
		User *U = Users.back();
		Users.pop_back();

		if (Instruction * I = dyn_cast<Instruction>(U)) {
			CallSite CS = MagicUtil::getCallSiteFromInstruction(I);
			if (CS.getInstruction()
					&& (MagicUtil::getCalledFunctionFromCS(CS) == function
							|| std::find(EqPointers.begin(), EqPointers.end(), CS.getCalledValue()) != EqPointers.end())) {
				Function *parentFunction = CS.getInstruction()->getParent()->getParent();
				StringRef callParentName = MagicUtil::getFunctionSourceName(M, parentFunction, NULL, baseDir);
				//extend function name with debug information
				if (MDNode *N = I->getMetadata("dbg")) {
					DILocation Loc(N);
					std::string string;
					raw_string_ostream ostream(string);
					ostream << callParentName << MAGIC_ALLOC_NAME_SEP << Loc.getFilename() << MAGIC_ALLOC_NAME_SEP << Loc.getLineNumber();
					ostream.flush();
					callParentName = string;
				}
				Value* callParentNameValue = MagicUtil::getArrayPtr(M, MagicUtil::getStringRef(M, callParentName));
				std::vector<Value*> debugArgs;
				debugArgs.push_back(callParentNameValue);
				debugArgs.insert(debugArgs.end(), CS.arg_begin(), CS.arg_end());
				CallInst* newInst = MagicUtil::createCallInstruction(debugFunction, debugArgs, "", I);
				newInst->takeName(I);
				MagicUtil::replaceCallInst(I, newInst, 1);
			}
		} else if (GlobalValue * GV = dyn_cast<GlobalValue>(U)) {
			Users.insert(Users.end(), GV->user_begin(), GV->user_end());
			EqPointers.push_back(GV);
		} else if (ConstantExpr * CE = dyn_cast<ConstantExpr>(U)) {
			if (CE->isCast()) {
				Users.insert(Users.end(), CE->user_begin(), CE->user_end());
				EqPointers.push_back(CE);
			}
		}
	}
}

}

// inlines calls to the pre and post hooks and returns true if the inlining succeeded, false otherwise
inline bool MagicDebugFunction::inlineHookCalls(Function* function, std::pair<Function*, Function*> hooks, unsigned flags,
		std::vector<unsigned> argsMapping, std::vector<Value*> trailingArgs) {
	std::vector<Value*> emptyArgs;
	std::vector<unsigned> emptyMapping;
	std::vector<unsigned> debugEmptyMapping;
	std::vector<unsigned> debugArgsMapping;

	// debug version of the function, argument mapping has to be adjusted
	if (flags & MAGIC_HOOK_DEBUG_MASK) {
		// re-adjusted the index of the arguments (do not re-adjust return value)
		for (unsigned i = 0; i < argsMapping.size(); i++) {
			if (argsMapping[i] > 0) {
				argsMapping[i] += NUM_DEBUG_ARGS;
			}
		}
		// first come the debug argument
		for (unsigned i = 1; i <= NUM_DEBUG_ARGS; i++) {
			debugEmptyMapping.push_back(i);
			debugArgsMapping.push_back(i);
		}
		debugArgsMapping.insert(debugArgsMapping.end(), argsMapping.begin(), argsMapping.end());
	}

	if (hooks.first != NULL) {
		// inline first hook call at the beginning of the function, according to the flag
		switch (flags & MAGIC_PRE_HOOK_FLAGS_MASK) {
		case MAGIC_PRE_HOOK_SIMPLE_CALL:
			MagicUtil::inlinePreHookForwardingCall(function, hooks.first, (flags & MAGIC_PRE_HOOK_DEBUG) ? debugEmptyMapping : emptyMapping,
					emptyArgs);
			break;
		case MAGIC_PRE_HOOK_FORWARDING_CALL:
			MagicUtil::inlinePreHookForwardingCall(function, hooks.first, (flags & MAGIC_PRE_HOOK_DEBUG) ? debugArgsMapping : argsMapping,
					trailingArgs);
			break;
		default:
			// unknown flag
			return false;
		}
	}

	if (hooks.second != NULL) {
		// inline the second wrapper call at the end of the function, according to the flag
		switch (flags & MAGIC_POST_HOOK_FLAGS_MASK) {
		case MAGIC_POST_HOOK_SIMPLE_CALL:
			MagicUtil::inlinePostHookForwardingCall(function, hooks.second, (flags & MAGIC_POST_HOOK_DEBUG) ? debugEmptyMapping : emptyMapping,
					emptyArgs);
			break;
		case MAGIC_POST_HOOK_FORWARDING_CALL:
			MagicUtil::inlinePostHookForwardingCall(function, hooks.second, (flags & MAGIC_POST_HOOK_DEBUG) ? debugArgsMapping : argsMapping,
					trailingArgs);
			break;
		default:
			// unknown flag
			return false;
		}
	}

	return true;
}

#endif /* MAGIC_DEBUG_FUNCTION_H_ */
