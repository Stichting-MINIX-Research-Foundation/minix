#include <pass.h>
#include <stdlib.h>

using namespace llvm;

#define DBG(M) M
#define helloPassLog(M) DBG(errs() << "HelloPass: " << M << "\n")
#define MSG   "Hello world!"
#define ERROR_FAILURE   1

namespace {

  class HelloPass : public ModulePass {

  public:
    static char ID;

    
    HelloPass() : ModulePass(ID) { }
    

    virtual bool runOnModule(Module &M) {

    Function* printfFunction = NULL;
    Function* mainFunction = NULL;      

    mainFunction = M.getFunction("main");
    if (NULL == mainFunction)
    {
      helloPassLog("Info: main() not found. Skipping instrumentation.");
      return false;
    }

    /* Prepare the string arguments for printf */
    std::string printFuncName = "printf" ;
    const std::string msg = MSG;
    const std::string fmt = "%s\n";
    Constant* strConstMsg = NULL;
    Constant* strConstFmt = NULL;
    std::vector<Value*> args(0);
    Instruction *I = NULL;

    PassUtil::getStringGlobalVariable(M, fmt, ".fmtStr", "", &strConstFmt, false);
    PassUtil::getStringGlobalVariable(M, msg, ".helloworld", "", &strConstMsg, false);

    if (NULL == strConstFmt || NULL == strConstMsg)
    {
  	  helloPassLog("Error: Prepared string contants point to NULL");
  	  exitOnError(ERROR_FAILURE);
    }

    args.push_back(strConstFmt);
    args.push_back(strConstMsg);
    
    /* Look for printf declaration */
    std::vector<TYPECONST Type*> functionTyArgs;
    FunctionType* printfFuncType;

    functionTyArgs.push_back(PointerType::get(IntegerType::get(M.getContext(), 8), 0));
    
    printfFuncType = PassUtil::getFunctionType(IntegerType::get(M.getContext(), 32), functionTyArgs, true);
    if (NULL == printfFuncType)
    {
      helloPassLog("Error: Couldn't make function-type for printf.");
      exitOnError(ERROR_FAILURE);
    }

	  printfFunction = (Function *) M.getOrInsertFunction(printFuncName, printfFuncType);
	  if (NULL == printfFunction)
	  {
      helloPassLog("Error: Couldnt find printf function declaration.");
      exitOnError(ERROR_FAILURE);
    }
    
    /* Insert call instruction in main() to call printf */
	  I = mainFunction->getBasicBlockList().begin()->begin();
	  if (NULL != I)
	  {
		  if (args.empty())
		  {
			  helloPassLog("Warning: args to printf is empty.");
		  }

		  helloPassLog("Info: Inserting printf call instruction");

		  CallInst* callInst = PassUtil::createCallInstruction(printfFunction, args, "", I);

		  if (NULL == callInst )
		  {
			  helloPassLog("Error: callInstr is null.");
			  exitOnError(ERROR_FAILURE);
		  }

		  helloPassLog("Info: Inserting call instruction successful.");

		  return true;
	  }
	  
      return false;
    }

    private:
    void exitOnError(int errCode)
    {
      helloPassLog("Aborting instrumentation.");
    	exit(errCode);
    }

  };

}

char HelloPass::ID = 0;
RegisterPass<HelloPass> HP("hello", "Hello Pass", false, false);
