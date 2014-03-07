
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Regex.h"

using namespace llvm;

//#define passLog(M) (errs() << "WeakAliasModuleOverride: " << M << "\n")
#define passLog(M) /* nothing */

namespace {
  class WeakAliasModuleOverride : public ModulePass {

  public:
    static char ID;
    WeakAliasModuleOverride() : ModulePass(ID) {
    }

    virtual bool runOnModule(Module &M) {
      const Module::FunctionListType &listFcts = M.getFunctionList();
      const Module::GlobalListType &listGlobalVars = M.getGlobalList();

      std::string Asm = M.getModuleInlineAsm();

      passLog("ASM START\n"
	  << Asm
	  << "ASM END\n");

      // Filter out Function symbols
      for(Module::const_iterator it = listFcts.begin(), end=listFcts.end(); it!= end; ++it)
      {
	if (it->isDeclaration())
	  continue;

	// Filter out the assembly weak symbol as well as its default value
        std::string symbolName = it->getName();
        std::string matchWeak = "(^.weak.* " + symbolName + "\n)";
        std::string matchAssignement = "(^" + symbolName + " .*=.*\n)";

        Regex filterWeak(matchWeak, Regex::Newline);
        Regex filterAssignement(matchAssignement, Regex::Newline);

        while(filterWeak.match(Asm))
          Asm = filterWeak.sub("", Asm);

        while(filterAssignement.match(Asm))
          Asm = filterAssignement.sub("", Asm);
      }

      // Filter out GlobalVariable symbols
      for(Module::const_global_iterator it = listGlobalVars.begin(), end=listGlobalVars.end(); it!= end; ++it)
      {
	if (! it->hasInitializer())
	  continue;

	// Filter out the assembly weak symbol as well as its default value
        std::string symbolName = it->getName();
        std::string matchWeak = "(^.weak.* " + symbolName + "\n)";
        std::string matchAssignement = "(^" + symbolName + " .*=.*\n)";

        Regex filterWeak(matchWeak, Regex::Newline);
        Regex filterAssignement(matchAssignement, Regex::Newline);

        while(filterWeak.match(Asm))
          Asm = filterWeak.sub("", Asm);

        while(filterAssignement.match(Asm))
          Asm = filterAssignement.sub("", Asm);
      }

      M.setModuleInlineAsm(Asm);

      passLog("ASM START - registered\n"
	  << M.getModuleInlineAsm()
	  << "ASM END\n");

      return true;
    }
  };
}

char WeakAliasModuleOverride::ID = 0;
RegisterPass<WeakAliasModuleOverride> WEAK_ALIAS_MODULE_OVERRIDE("weak-alias-module-override", "Fix Weak Alias overrides");
