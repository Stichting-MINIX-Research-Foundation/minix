#ifndef ASR_PASS_H

#define ASR_PASS_H

#include <pass.h>

#define DEFAULT_SEED                    0

#define GV_DEFAULT_MAX_OFFSET           10000
#define GV_DEFAULT_MAX_PADDING          50
#define GV_DEFAULT_DO_PERMUTATE         1

#define FUNC_DEFAULT_MAX_OFFSET         10000
#define FUNC_DEFAULT_MAX_PADDING        100
#define FUNC_DEFAULT_MAX_BB_SHIFT       50
#define FUNC_DEFAULT_DO_PERMUTATE       1

#define STACK_DEFAULT_DO_OFFSET         1
#define STACK_DEFAULT_MAX_OFFSET        50

#define STACKFRAME_DEFAULT_DO_OFFSET    1
#define STACKFRAME_DEFAULT_MAX_OFFSET   50
#define STACKFRAME_DEFAULT_MAX_PADDING  5000
#define STACKFRAME_DEFAULT_DO_PERMUTATE 1
#define STACKFRAME_DEFAULT_STATIC_PADDING 1
#define STACKFRAME_DEFAULT_CALLER_PADDING 0 // broken, disabled (see note in code)

#define HEAP_MAP_DEFAULT_DO_PERMUTATE   1

#define HEAP_DEFAULT_MAX_OFFSET         10000
#define HEAP_DEFAULT_MAX_PADDING        100

#define MAP_DEFAULT_MAX_OFFSET_PAGES    10
#define MAP_DEFAULT_MAX_PADDING_PAGES   3

using namespace llvm;

namespace llvm {

class ASRPass : public ModulePass {

  public:
      static char ID;

      ASRPass();

      virtual bool runOnModule(Module &M);

};

}

#endif
