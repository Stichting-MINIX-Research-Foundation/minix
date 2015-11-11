#include <asr/ASRPass.h>
#include <magic_common.h>
#include <magic/support/MagicUtil.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#define MAGIC_IS_MAGIC_FUNC(M, F) (!StringRef((F)->getSection()).compare(MAGIC_STATIC_FUNCTIONS_SECTION))

using namespace llvm;


// command-line arguments

static cl::opt<int>
seed("asr-seed",
    cl::desc("Random seed integer value for ASRPass. '0' will use current time as seed"),
    cl::init(DEFAULT_SEED), cl::NotHidden, cl::ValueRequired);


static cl::opt<int>
gv_max_offset("asr-gv-max-offset",
    cl::desc(""),
    cl::init(GV_DEFAULT_MAX_OFFSET), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
gv_max_padding("asr-gv-max-padding",
    cl::desc(""),
    cl::init(GV_DEFAULT_MAX_PADDING), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
gv_do_permutate("asr-gv-do-permutate",
    cl::desc(""),
    cl::init(GV_DEFAULT_DO_PERMUTATE), cl::NotHidden, cl::ValueRequired);


static cl::opt<int>
func_max_offset("asr-func-max-offset",
    cl::desc(""),
    cl::init(FUNC_DEFAULT_MAX_OFFSET), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
func_max_padding("asr-func-max-padding",
    cl::desc(""),
    cl::init(FUNC_DEFAULT_MAX_PADDING), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
func_max_bb_shift("asr-func-max-bb-shift",
    cl::desc(""),
    cl::init(FUNC_DEFAULT_MAX_BB_SHIFT), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
func_do_permutate("asr-func-do-permutate",
    cl::desc(""),
    cl::init(FUNC_DEFAULT_DO_PERMUTATE), cl::NotHidden, cl::ValueRequired);


static cl::opt<int>
stack_do_offset("asr-stack-do-offset",
    cl::desc(""),
    cl::init(STACK_DEFAULT_DO_OFFSET), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
stack_max_offset("asr-stack-max-offset",
    cl::desc(""),
    cl::init(STACK_DEFAULT_MAX_OFFSET), cl::NotHidden, cl::ValueRequired);


static cl::opt<int>
stackframe_do_offset("asr-stackframe-do-offset",
    cl::desc(""),
    cl::init(STACKFRAME_DEFAULT_DO_OFFSET), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
stackframe_max_offset("asr-stackframe-max-offset",
    cl::desc(""),
    cl::init(STACKFRAME_DEFAULT_MAX_OFFSET), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
stackframe_max_padding("asr-stackframe-max-padding",
    cl::desc(""),
    cl::init(STACKFRAME_DEFAULT_MAX_PADDING), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
stackframe_do_permutate("asr-stackframe-do-permutate",
    cl::desc(""),
    cl::init(STACKFRAME_DEFAULT_DO_PERMUTATE), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
stackframe_static_padding("asr-stackframe-static-padding",
    cl::desc(""),
    cl::init(STACKFRAME_DEFAULT_STATIC_PADDING), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
stackframe_caller_padding("asr-stackframe-caller-padding",
    cl::desc(""),
    cl::init(STACKFRAME_DEFAULT_CALLER_PADDING), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
heap_map_do_permutate("asr-heap-map-do-permutate",
    cl::desc(""),
    cl::init(HEAP_MAP_DEFAULT_DO_PERMUTATE), cl::NotHidden, cl::ValueRequired);


static cl::opt<int>
heap_max_offset("asr-heap-max-offset",
    cl::desc(""),
    cl::init(HEAP_DEFAULT_MAX_OFFSET), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
heap_max_padding("asr-heap-max-padding",
    cl::desc(""),
    cl::init(HEAP_DEFAULT_MAX_PADDING), cl::NotHidden, cl::ValueRequired);


static cl::opt<int>
map_max_offset_pages("asr-map-max-offset-pages",
    cl::desc(""),
    cl::init(MAP_DEFAULT_MAX_OFFSET_PAGES), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
map_max_padding_pages("asr-map-max-padding-pages",
    cl::desc(""),
    cl::init(MAP_DEFAULT_MAX_PADDING_PAGES), cl::NotHidden, cl::ValueRequired);


#define __X(P) #P
        std::string magicMemFuncNames[] = { MAGIC_MEM_FUNC_NAMES };
#undef __X

namespace llvm {

PASS_COMMON_INIT_ONCE();

//===----------------------------------------------------------------------===//
// Constructors, destructor, and operators
//===----------------------------------------------------------------------===//

ASRPass::ASRPass() : ModulePass(ID) {}
//===----------------------------------------------------------------------===//
// Public methods
//===----------------------------------------------------------------------===//

void fillPermutationGenerator(std::vector<unsigned> &permutationGenerator){
    // This function returns a list of indices. In order to create a permutation of a list of elements, for each index, remove that element and place it at the end of the list.
    unsigned size = permutationGenerator.size();
    for (unsigned i = 0; i < size; ++i) {
        unsigned j = rand() % (size - i);
        permutationGenerator[i] = j;
    }
}

Function* getCalledFunctionFromCS(const CallSite &CS) {
    assert(CS.getInstruction());
    Function *function = CS.getCalledFunction();
    if(function) {
        return function;
    }

    //handle the weird case of bitcasted function call
    ConstantExpr *CE = dyn_cast<ConstantExpr>(CS.getCalledValue());
    if(!CE) {
        return NULL;
    }
    assert(CE && CE->getOpcode() == Instruction::BitCast && "Bitcast expected, something else found!");
    function = dyn_cast<Function>(CE->getOperand(0));
    assert(function);

    return function;
}

#define ADVANCE_ITERATOR(IT, N_POS) for(unsigned __adv_it_count=0; __adv_it_count< N_POS; __adv_it_count++){ IT++;}

GlobalVariable *create_padding_gv(Module &M, GlobalVariable *InsertBefore, int n_bytes){

    ArrayType* ArrayTy = ArrayType::get(IntegerType::get(M.getContext(), 8), n_bytes);

    GlobalVariable* padding_char_arr = new GlobalVariable(/*Module=*/M,
            /*Type=*/ArrayTy,
            /*isConstant=*/false,
            /*Linkage=*/GlobalValue::InternalLinkage,
            /*Initializer=*/ConstantAggregateZero::get(ArrayTy),
            /*Name=*/"magic_asr_padding_gv",
            /*InsertBefore=*/InsertBefore);
    padding_char_arr->setAlignment(1);
    padding_char_arr->setSection(InsertBefore->getSection());
    return padding_char_arr;

}

AllocaInst *create_padding_lv(Module &M, Instruction *InsertBefore, int n_bytes){

    ArrayType* ArrayTy = ArrayType::get(IntegerType::get(M.getContext(), 8), n_bytes);
    AllocaInst* ptr_x = new AllocaInst(ArrayTy, "magic_asr_padding_lv", InsertBefore);
    ptr_x->setAlignment(16);

    /* Seems not to be necessary

    ConstantInt* const_int64_0 = ConstantInt::get(M.getContext(), APInt(64, StringRef("0"), 10));
    ConstantInt* const_int8_0 = ConstantInt::get(M.getContext(), APInt(8, StringRef("97"), 10));

    std::vector<Value*> ptr_indices;
    ptr_indices.push_back(const_int64_0);
    ptr_indices.push_back(const_int64_0);

    Instruction* ptr_8 = GetElementPtrInst::Create(ptr_x, ptr_indices.begin(), ptr_indices.end(), "", ptr_x->getParent());
    ptr_8->removeFromParent();
    ptr_8->insertAfter(ptr_x);

    StoreInst* void_9 = new StoreInst(const_int8_0, ptr_8, true, ptr_x->getParent());
    void_9->setAlignment(16);
    void_9->removeFromParent();
    void_9->insertAfter(ptr_8);

    */

    return ptr_x;

}

Function *create_padding_func(Module &M, int n_ops){
    /* Places a padding function at the end of the function list */

    std::vector<TYPECONST Type*>FuncTy_0_args;
    TYPECONST FunctionType* FuncTy_0 = FunctionType::get(Type::getVoidTy(M.getContext()), FuncTy_0_args, false);

    Function* func_padding_func = Function::Create(FuncTy_0, GlobalValue::ExternalLinkage, "magic_asr_padding_func", &M);
    func_padding_func->setCallingConv(CallingConv::C);
    BasicBlock* bb = BasicBlock::Create(M.getContext(), "",func_padding_func,0);

    ConstantInt* const_int32_0 = ConstantInt::get(M.getContext(), APInt(32, StringRef("0"), 10));
    ConstantInt* const_int32_1 = ConstantInt::get(M.getContext(), APInt(32, StringRef("1"), 10));

    AllocaInst* ptr_x = new AllocaInst(IntegerType::get(M.getContext(), 32), "x", bb);
    ptr_x->setAlignment(4);

    StoreInst* void_1 = new StoreInst(const_int32_0, ptr_x, true, bb);
    void_1->setAlignment(4);

    for(int i=0; i< n_ops; i++){
        LoadInst* load_x = new LoadInst(ptr_x, "", true, bb);
        load_x->setAlignment(4);

        BinaryOperator* add_x = BinaryOperator::Create(Instruction::Add, load_x, const_int32_1, "", bb);

        StoreInst* void_2 = new StoreInst(add_x, ptr_x, true, bb);
        void_2->setAlignment(4);
    }

    ReturnInst::Create(M.getContext(), bb);

    return func_padding_func;
}

StringRef getStringRefFromInt(int i){
    std::stringstream stm;
    stm << i;
    return StringRef(*new std::string(stm.str()));
}

bool ASRPass::runOnModule(Module &M) {

    Module::GlobalListType &globalList = M.getGlobalList();
    Module::FunctionListType &functionList = M.getFunctionList();
    int runtime_seed = seed;

    Function *magicEntryPointFunc = M.getFunction(MAGIC_ENTRY_POINT);
    if( !magicEntryPointFunc ){
        //if no valid entry point, we are not compiling a valid program, skip pass
        return false;
    }

    Function *magicInitFunc = M.getFunction(MAGIC_INIT_FUNC_NAME);
    if( !magicInitFunc ){
        outs() << "Error: no " << MAGIC_INIT_FUNC_NAME << "() found";
        exit(1);
    }

    {
        // get random seed number, or use the current time if the seed number is set to 0.
        if(!seed){
            seed = time(NULL);
        }
        srand(seed);

    }{

        /* Randomly offset and permutate list of global variables, and insert random padding between neighbouring global variables */

        std::vector<unsigned> pg(globalList.size());
        fillPermutationGenerator(pg);

        for(unsigned i=0; i < pg.size(); i++){
            Module::global_iterator it = globalList.begin();
            // get the next random global variable
            ADVANCE_ITERATOR(it, pg[i]);
            // skip certain variables
            if(it->getName().startswith("llvm.")
                || it->getLinkage() == GlobalValue::ExternalWeakLinkage){
                continue;
            }
            if(it->getLinkage() != GlobalValue::ExternalLinkage && it->getName().compare("environ")){
                // This prevents most public global variables (common linkage, but not external linkage) to be kept in the same order
                it->setLinkage(GlobalValue::InternalLinkage);
            }
            if(gv_do_permutate){
                // randomize the order of variables, by removing the global variable, and putting it at the end of globalList
                GlobalVariable *gv = globalList.remove(it);
                globalList.push_back(gv);
                it = --globalList.end();
            }
            // put a padding variable between each two adjacent global variables, and place a big offset before the first global variable
            int max_padding = i == 0 ? gv_max_offset : gv_max_padding;
            if(max_padding > 0){
                create_padding_gv(M, it, (rand () % max_padding) + 1);
            }
        }

    }{

        /* Randomly offset and permutate function list, and insert random padding between neighbouring functions. */

        std::vector<unsigned> pg(functionList.size());
        fillPermutationGenerator(pg);

        for(unsigned i=0; i < pg.size(); i++){
            Module::iterator it = functionList.begin();
            if(func_do_permutate){
                /* randomize the order of functions, just like we did with the global variables if permutions is disabled, we end up with the same order of functions */
                ADVANCE_ITERATOR(it, pg[i]);
            }
            Function *F = functionList.remove(it);
            functionList.push_back(F);
            /* place a padding function at the end of the function list, behind the current function */
            int max_padding = i == 0 ? func_max_offset : func_max_padding;
            if(max_padding > 0){
                create_padding_func(M, (rand () % (max_padding/2)) + (max_padding/2));
            }
        }

    }{


        /* permutate and pad local function variables, and create dynamically randomized stack and stack frame offsets */

        for (Module::iterator it = functionList.begin(); it != functionList.end(); ++it) {
            Function *F = it;

            /* skip certain functions */
            if(F->getBasicBlockList().size() == 0){
                continue;
            }
            if(MAGIC_IS_MAGIC_FUNC(M, F)){
                continue;
            }
            if(!F->getName().compare("rand")){
                continue;
            }


            /* find all allocation instructions in order to pad them. */

            /* Helper vectors to store all alloca instructions temporarily.
             * Make two collections, depending on whether the address of the variable is taken and used as a pointer.
             * (Because pointer dereferencing, buffer overflow, etc. add extra risks to those variables that have their addresses taken)
             * We order the allocation instructions as follows:
             * - First, we allocate the ones that don't have their address taken, only permutated.
             * - Then, we allocate an stack frame offset (dynamically randomly sized).
             * - After the stack frame offset, we allocate those that have their address taken, with permutation and padding.
             * Because the majority doesn't have its address taken, most variables are allocated in the first basic block, before the stack frame offset allocation.
             * This gives the extra advantages that those allocations are folded into the prolog/epilog code by the code generator, for extra performance.
             * (See AllocaInst::isStaticAlloca() in llvm/Instructions.h)
             * */
            std::vector<Instruction *> allocaAddressTaken, allocaNoAddressTaken;

            /* Only the first basic block contains alloca instructions */
            BasicBlock *BB =  F->getBasicBlockList().begin();

            /* with each iteration, one of these integers will be incremented/decremented */
            unsigned bb_size = BB->getInstList().size();
            unsigned pos = 0;
            while(pos < bb_size){

                /* check if instruction at position <pos> is an allocation instruction.
                 * If, so remove and put in one of the helper vectors
                 * */

                BasicBlock::iterator it = BB->getInstList().begin();
                /* move to current position in instruction list */
                ADVANCE_ITERATOR(it, pos);
                Instruction *inst = &(*it);
                if (AllocaInst *allocaInst = dyn_cast<AllocaInst>(inst)){
                    /* this is an allocation instruction. insert it at the front of of the right helper vector
                     * (last found allocation instruction will be at the front), and remove it from the basic block.
                     * */
                    int hasAddressTaken = 0;
                    for (Value::user_iterator UI = allocaInst->user_begin(), E = allocaInst->user_end(); UI != E; ++UI) {

                        /* Loop through all the Uses of this allocation function. */

                        User *U = *UI;
                        if(dyn_cast<LoadInst>(U) || dyn_cast<StoreInst>(U)){
                            /* This is a load or store instruction, which does not
                             * indicate that a pointer of this variable is generated
                             * */
                            continue;
                        }else if(CallInst *cInst = dyn_cast<CallInst>(U)){
                            if(cInst->getCalledFunction() && MAGIC_IS_MAGIC_FUNC(M, cInst->getCalledFunction())){
                                /* This is a function call instruction, but this
                                 * concerns a magic library function, so it does not count as a generated pointer.
                                 * Any other functions calls would have set hasAddressTaken to 1 */
                                continue;
                            }
                        }
                        /* This instruction will (likely) create a pointer, because it is not a load, store or magic-function-call instruction */
                        hasAddressTaken = 1;
                        break;
                    }

                    /* Put the alloca instruction in the right helper vector, and remove from the basic block. */
                    if(hasAddressTaken){
                        allocaAddressTaken.insert(allocaAddressTaken.begin(), it);
                    }else{
                        allocaNoAddressTaken.insert(allocaNoAddressTaken.begin(), it);
                    }
                    it->removeFromParent();
                    bb_size--;
                }else{
                    pos++;
                }
            }

            /* Permutate and pad the alloca instructions whose addresses are taken. */

            std::vector<unsigned> pg(allocaAddressTaken.size());
            fillPermutationGenerator(pg);
            for(unsigned i=0; i<pg.size(); i++){
                /* get the iterator for the first element of the helper vector */
                std::vector<Instruction *>::iterator it = allocaAddressTaken.begin();
                if(stackframe_do_permutate){
                    /* get the iterator for the next random element. When permutation is disabled, it keeps pointing to the first element */
                    ADVANCE_ITERATOR(it, pg[i]);
                }
                /* put the variable at the front of the basic block, and remove it from the helper vector.
                 * This way, the variable that is added last will be at the front
                 * */
                BB->getInstList().push_front(*it);
                allocaAddressTaken.erase(it);

                /* put a padding variable between each two adjacent local variables
                 * this is done by inserting a padding var at the front each time a
                 * var has been put at the front with push_front().
                 * */
                int max_padding = (i==pg.size()-1 ? 0 : stackframe_max_padding);
                if(max_padding > 0){
                    create_padding_lv(M, BB->getInstList().begin(), (rand () % max_padding) + 1);
                }
            }


            /* Create a global stack offset, and an offset for each stack frame. Both have a dynamic random size */

            /* Determine if we must pad or offset, and how much */
            int max_offset, do_offset=1;
            if(F->getName().equals(MAGIC_ENTRY_POINT)){
                if(!stack_do_offset){
                    do_offset=0;
                }
                /* give the entry function (first function) a large offset instead of an padding */
                max_offset = stack_max_offset;
            }else{
                if(!stackframe_do_offset){
                    do_offset=0;
                }
                max_offset = stackframe_max_offset;
            }

            /* Create a new block before the first block. Now, all the variable allocations whose addresses are taken are no longer
             * in the first block, so CallInst::isStaticAlloca() does no longer apply to them.
             * When isStaticAlloca() == true, the code generator will fold it into the prolog/epilog code, so it is basically free.
             * This means that we now get less efficient code.
             * This is necessary to prevent the variables whose address is taken from being allocated before the stack frame offset is allocated.
             * Alternatively, we could allocate before the function call, instead of after. */

            BasicBlock *OldFirstBB = F->getBasicBlockList().begin();
            BasicBlock *NewFirstBB = BasicBlock::Create(M.getContext(), "newBB", F, OldFirstBB);


            /* Permutate and insert the allocation instructions whose addresses are NOT taken into the new first block (dont apply padding).
             * These must be allocated before the stack frame offset is allocated. */

            pg = std::vector<unsigned>(allocaNoAddressTaken.size());
            fillPermutationGenerator(pg);
            for(unsigned i=0; i<pg.size(); i++){
                /* get the iterator for the first element of the helper vector */
                std::vector<Instruction *>::iterator it = allocaNoAddressTaken.begin();
                if(stackframe_do_permutate){
                    /* get the iterator for the next random element. When permutation is disabled, it keeps pointing to the first element */
                    ADVANCE_ITERATOR(it, pg[i]);
                }
                /* put the variable at the front of the basic block, and remove it from the helper vector.
                 * This way, the variable that is added last will be at the front
                 * */
                NewFirstBB->getInstList().push_front(*it);
                allocaNoAddressTaken.erase(it);
            }

            if(do_offset){
                if(stackframe_static_padding) {
                    if(max_offset > 0) {
                        new AllocaInst(IntegerType::get(M.getContext(), 8), ConstantInt::get(M.getContext(), APInt(64, (rand() % max_offset) + 1, 10)), "", NewFirstBB);
                    }
                }
                else {
                    /* Now insert a dynamically randomized stackframe offset */
                    Function *RandFunc = M.getFunction("rand");
                    assert(RandFunc != NULL);

                    /* Call rand() */
                    std::vector<Value*> args;
                    CallInst* RandFuncCall = PassUtil::createCallInstruction(RandFunc, args, "", NewFirstBB);
                    Instruction *nextInst = RandFuncCall;

                    if(max_offset > 0){
                        /* limit the rand value: rand() % max_offet */
                        ConstantInt* max_offset_const = ConstantInt::get(M.getContext(), APInt(32, max_offset, 10));
                        BinaryOperator *Remainder = BinaryOperator::Create(Instruction::SRem, RandFuncCall, max_offset_const, "", NewFirstBB);
                        Remainder->removeFromParent();
                        Remainder->insertAfter(RandFuncCall);
                        nextInst = Remainder;
                    }

                    /* Minimum rand value must be 1, so increment it. */
                    ConstantInt* One = ConstantInt::get(M.getContext(), APInt(32, StringRef("1"), 10));
                    BinaryOperator* AddOne = BinaryOperator::Create(Instruction::Add, nextInst, One, "", NewFirstBB);
                    AddOne->removeFromParent();
                    AddOne->insertAfter(nextInst);

                    /* Allocate the offset/padding */
                    AllocaInst* allocaInstruction = new AllocaInst(IntegerType::get(M.getContext(), 8), AddOne, "", NewFirstBB);
                    allocaInstruction->removeFromParent();
                    allocaInstruction->insertAfter(AddOne);

                    /* Inline the rand() call. */
                    InlineFunctionInfo IFI;
                    InlineFunction(RandFuncCall, IFI);
                }
            }

            /* Go to the old first block */
            BranchInst *br =  BranchInst::Create (OldFirstBB, NewFirstBB);
            br->setSuccessor(0, OldFirstBB);

            /* Static stack frame padding does not really need 2 basic blocks, but it may need call site instrumentation. */
            if(stackframe_static_padding) {
                bool ret = MergeBlockIntoPredecessor(OldFirstBB, this);
                assert(ret);

                if(stackframe_caller_padding && max_offset > 0) {
                    std::vector<User*> Users(F->user_begin(), F->user_end());
                    while (!Users.empty()) {
                        User *U = Users.back();
                        Users.pop_back();
                        if (Instruction *I = dyn_cast<Instruction>(U)) {
                            Function *parent = I->getParent()->getParent();
                            /* XXX Skipping MAGIC_ENTRY_POINT shouldn't be necessary. Check why. */
                            /* ..the reason is that main() typically contains the message loop, which loops
                             * forever making calls. These calls are getting padded, and AllocaInst causes a
                             * stack pointer adjustment every time a call is made. This stack memory is never
                             * released, since the function never returns. The result is that we eventually
                             * run out of stack. Since MINIX3 also uses user-level threads these days, the
                             * problem is not limited to main(), and for this reason I have disabled caller
                             * padding by default. -dcvmoole
                             */
                            if(MAGIC_IS_MAGIC_FUNC(M, parent) || parent->getName().equals(MAGIC_ENTRY_POINT)) {
                                continue;
                            }
                            CallSite CS = PassUtil::getCallSiteFromInstruction(I);
                            if(!CS.getInstruction()) {
                                continue;
                            }
                            Function *calledFunction = getCalledFunctionFromCS(CS);
                            if (CS.getInstruction() && !CS.arg_empty() && (calledFunction == F || calledFunction == NULL)) {
                                new AllocaInst(IntegerType::get(M.getContext(), 8), ConstantInt::get(M.getContext(), APInt(64, (rand() % max_offset) + 1, 10)), "", I);
                            }
                        }
                    }
                }
            }

            /* Basic block shifting. */
            if(func_max_bb_shift > 0) {
                Instruction *I;
                PassUtil::getAllocaInfo(F, NULL, &I);
                BasicBlock *firstBB = F->getBasicBlockList().begin();
                BasicBlock *splitBB = firstBB->splitBasicBlock(I, "split");
                BasicBlock *dummyBB = BasicBlock::Create(M.getContext(), "dummy", F, splitBB);
                if(!stackframe_caller_padding) {
                    firstBB = NewFirstBB;
                }

                /* Fill the dummy basic block with dummy instructions (using the prefetch intrinsic to emulate nop instructions), to shift the next basic block. */
                Function *prefetchIntrinsic = PassUtil::getIntrinsicFunction(M, Intrinsic::prefetch);
                std::vector<Value*> args;
                args.push_back(ConstantPointerNull::get(PointerType::get(IntegerType::get(M.getContext(), 8), 0)));
                args.push_back(ConstantInt::get(M.getContext(), APInt(32, 0)));
                args.push_back(ConstantInt::get(M.getContext(), APInt(32, 0)));
#if LLVM_VERSION >= 30
                args.push_back(ConstantInt::get(M.getContext(), APInt(32, 0)));
#endif
                unsigned shift = (rand() % func_max_bb_shift) + 1;
                do {
                    PassUtil::createCallInstruction(prefetchIntrinsic, args, "", dummyBB);
                    shift--;
                } while(shift > 0);
                BranchInst *br =  BranchInst::Create (splitBB, dummyBB);
                br->setSuccessor(0, splitBB);

                /* Place an opaque conditional branch (always unconditionally skips the dummy basic block). */
                Function *frameAddrIntrinsic = PassUtil::getIntrinsicFunction(M, Intrinsic::frameaddress);
                std::vector<Value*> frameAddrArgs;
                frameAddrArgs.push_back(ConstantInt::get(M.getContext(), APInt(32, 0)));
                Value *frameAddr = PassUtil::createCallInstruction(frameAddrIntrinsic, frameAddrArgs, "", firstBB->getTerminator());
                TerminatorInst *OldTI = firstBB->getTerminator();
                IRBuilder<> Builder(firstBB);
                ICmpInst* ExtraCase = new ICmpInst(OldTI, ICmpInst::ICMP_EQ, frameAddr, ConstantPointerNull::get(PointerType::get(IntegerType::get(M.getContext(), 8), 0)), "");
                Builder.CreateCondBr(ExtraCase, dummyBB, splitBB);
                OldTI->eraseFromParent();
            }
        }

    }{


#define __X(VAR) __XX(VAR)
#define __XX(VAR) #VAR

        /* heap and map padding */

        {

            /* Inject magic init call at the beginning of magic entry point function (before any allocaInsts).
             * Magic_init will return immediately if called for the second time, so both the magic pass and
             * this pass can insert call instructions into main
             * */
            std::vector<Value*> args;
            PassUtil::createCallInstruction(magicInitFunc, args, "", magicEntryPointFunc->getBasicBlockList().begin()->begin());

        }{

            /* set the global variables */

            Function *magicDataInitFunc = M.getFunction(MAGIC_DATA_INIT_FUNC_NAME);
            if(!magicDataInitFunc){
                outs() <<"Error: no " << MAGIC_DATA_INIT_FUNC_NAME << "() found";
                exit(1);
            }
            Instruction *magicArrayBuildFuncInst = magicDataInitFunc->back().getTerminator();

            GlobalVariable* magicRootVar = M.getNamedGlobal(MAGIC_ROOT_VAR_NAME);
            if(!magicRootVar) {
                outs() << "Error: no " << MAGIC_ROOT_VAR_NAME << " variable found";
                exit(1);
            }

            Value *seedValue = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_ASR_SEED);
            if(!seedValue) {
                outs() << "Error: no " << MAGIC_RSTRUCT_FIELD_ASR_SEED << " field found";
                exit(1);
            }
            new StoreInst(ConstantInt::get(M.getContext(), APInt(32, runtime_seed)), seedValue, false, magicArrayBuildFuncInst);

            Value *heapMapPermutateValue = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAP_DO_PERMUTATE);
            if(!heapMapPermutateValue) {
                outs() << "Error: no " << MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAP_DO_PERMUTATE << " field found";
                exit(1);
            }
            new StoreInst(ConstantInt::get(M.getContext(), APInt(32, heap_map_do_permutate)), heapMapPermutateValue, false, magicArrayBuildFuncInst);


            Value *heapOffsetValue = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAX_OFFSET);
            if(!heapOffsetValue) {
                outs() << "Error: no " << MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAX_OFFSET << " field found";
                exit(1);
            }
            new StoreInst(ConstantInt::get(M.getContext(), APInt(32, heap_max_offset)), heapOffsetValue, false, magicArrayBuildFuncInst);

            Value *heapPaddingValue = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAX_PADDING);
            if(!heapPaddingValue) {
                outs() << "Error: no " << MAGIC_RSTRUCT_FIELD_ASR_HEAP_MAX_PADDING << " field found";
                exit(1);
            }
            new StoreInst(ConstantInt::get(M.getContext(), APInt(32, heap_max_padding)), heapPaddingValue, false, magicArrayBuildFuncInst);


            Value *mapOffsetValue = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_ASR_MAP_MAX_OFFSET_PAGES);
            if(!mapOffsetValue) {
                outs() << "Error: no " << MAGIC_RSTRUCT_FIELD_ASR_MAP_MAX_OFFSET_PAGES << " field found";
                exit(1);
            }
            new StoreInst(ConstantInt::get(M.getContext(), APInt(32, map_max_offset_pages)), mapOffsetValue, false, magicArrayBuildFuncInst);

            Value *mapPaddingValue = MagicUtil::getMagicRStructFieldPtr(M, magicArrayBuildFuncInst, magicRootVar, MAGIC_RSTRUCT_FIELD_ASR_MAP_MAX_PADDING_PAGES);
            if(!mapPaddingValue) {
                outs() << "Error: no " << MAGIC_RSTRUCT_FIELD_ASR_MAP_MAX_PADDING_PAGES << " field found";
                exit(1);
            }
            new StoreInst(ConstantInt::get(M.getContext(), APInt(32, map_max_padding_pages)), mapPaddingValue, false, magicArrayBuildFuncInst);



        }

    }

    return true;
}

} // end namespace

char ASRPass::ID = 1;
static RegisterPass<ASRPass> AP("asr", "Address Space Randomization Pass");
