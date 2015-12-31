//===-- X86TargetMachine.cpp - Define TargetMachine for the X86 -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the X86 specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#include "X86TargetMachine.h"
#include "X86.h"
#include "X86TargetObjectFile.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/Function.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetOptions.h"
using namespace llvm;

extern "C" void LLVMInitializeX86Target() {
  // Register the target.
  RegisterTargetMachine<X86TargetMachine> X(TheX86_32Target);
  RegisterTargetMachine<X86TargetMachine> Y(TheX86_64Target);
}

static std::unique_ptr<TargetLoweringObjectFile> createTLOF(const Triple &TT) {
  if (TT.isOSBinFormatMachO()) {
    if (TT.getArch() == Triple::x86_64)
      return make_unique<X86_64MachoTargetObjectFile>();
    return make_unique<TargetLoweringObjectFileMachO>();
  }

  if (TT.isOSLinux())
    return make_unique<X86LinuxTargetObjectFile>();
  if (TT.isOSBinFormatELF())
    return make_unique<TargetLoweringObjectFileELF>();
  if (TT.isKnownWindowsMSVCEnvironment())
    return make_unique<X86WindowsTargetObjectFile>();
  if (TT.isOSBinFormatCOFF())
    return make_unique<TargetLoweringObjectFileCOFF>();
  llvm_unreachable("unknown subtarget type");
}

/// X86TargetMachine ctor - Create an X86 target.
///
X86TargetMachine::X86TargetMachine(const Target &T, StringRef TT, StringRef CPU,
                                   StringRef FS, const TargetOptions &Options,
                                   Reloc::Model RM, CodeModel::Model CM,
                                   CodeGenOpt::Level OL)
    : LLVMTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL),
      TLOF(createTLOF(Triple(getTargetTriple()))),
      Subtarget(TT, CPU, FS, *this, Options.StackAlignmentOverride) {
  // default to hard float ABI
  if (Options.FloatABIType == FloatABI::Default)
    this->Options.FloatABIType = FloatABI::Hard;

  // Windows stack unwinder gets confused when execution flow "falls through"
  // after a call to 'noreturn' function.
  // To prevent that, we emit a trap for 'unreachable' IR instructions.
  // (which on X86, happens to be the 'ud2' instruction)
  if (Subtarget.isTargetWin64())
    this->Options.TrapUnreachable = true;

  initAsmInfo();
}

X86TargetMachine::~X86TargetMachine() {}

const X86Subtarget *
X86TargetMachine::getSubtargetImpl(const Function &F) const {
  AttributeSet FnAttrs = F.getAttributes();
  Attribute CPUAttr =
      FnAttrs.getAttribute(AttributeSet::FunctionIndex, "target-cpu");
  Attribute FSAttr =
      FnAttrs.getAttribute(AttributeSet::FunctionIndex, "target-features");

  std::string CPU = !CPUAttr.hasAttribute(Attribute::None)
                        ? CPUAttr.getValueAsString().str()
                        : TargetCPU;
  std::string FS = !FSAttr.hasAttribute(Attribute::None)
                       ? FSAttr.getValueAsString().str()
                       : TargetFS;

  // FIXME: This is related to the code below to reset the target options,
  // we need to know whether or not the soft float flag is set on the
  // function before we can generate a subtarget. We also need to use
  // it as a key for the subtarget since that can be the only difference
  // between two functions.
  Attribute SFAttr =
      FnAttrs.getAttribute(AttributeSet::FunctionIndex, "use-soft-float");
  bool SoftFloat = !SFAttr.hasAttribute(Attribute::None)
                       ? SFAttr.getValueAsString() == "true"
                       : Options.UseSoftFloat;

  auto &I = SubtargetMap[CPU + FS + (SoftFloat ? "use-soft-float=true"
                                               : "use-soft-float=false")];
  if (!I) {
    // This needs to be done before we create a new subtarget since any
    // creation will depend on the TM and the code generation flags on the
    // function that reside in TargetOptions.
    resetTargetOptions(F);
    I = llvm::make_unique<X86Subtarget>(TargetTriple, CPU, FS, *this,
                                        Options.StackAlignmentOverride);
  }
  return I.get();
}

//===----------------------------------------------------------------------===//
// Command line options for x86
//===----------------------------------------------------------------------===//
static cl::opt<bool>
UseVZeroUpper("x86-use-vzeroupper", cl::Hidden,
  cl::desc("Minimize AVX to SSE transition penalty"),
  cl::init(true));

//===----------------------------------------------------------------------===//
// X86 Analysis Pass Setup
//===----------------------------------------------------------------------===//

void X86TargetMachine::addAnalysisPasses(PassManagerBase &PM) {
  // Add first the target-independent BasicTTI pass, then our X86 pass. This
  // allows the X86 pass to delegate to the target independent layer when
  // appropriate.
  PM.add(createBasicTargetTransformInfoPass(this));
  PM.add(createX86TargetTransformInfoPass(this));
}


//===----------------------------------------------------------------------===//
// Pass Pipeline Configuration
//===----------------------------------------------------------------------===//

namespace {
/// X86 Code Generator Pass Configuration Options.
class X86PassConfig : public TargetPassConfig {
public:
  X86PassConfig(X86TargetMachine *TM, PassManagerBase &PM)
    : TargetPassConfig(TM, PM) {}

  X86TargetMachine &getX86TargetMachine() const {
    return getTM<X86TargetMachine>();
  }

  const X86Subtarget &getX86Subtarget() const {
    return *getX86TargetMachine().getSubtargetImpl();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  bool addILPOpts() override;
  void addPostRegAlloc() override;
  void addPreEmitPass() override;
};
} // namespace

TargetPassConfig *X86TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new X86PassConfig(this, PM);
}

void X86PassConfig::addIRPasses() {
  addPass(createAtomicExpandPass(&getX86TargetMachine()));

  TargetPassConfig::addIRPasses();
}

bool X86PassConfig::addInstSelector() {
  // Install an instruction selector.
  addPass(createX86ISelDag(getX86TargetMachine(), getOptLevel()));

  // For ELF, cleanup any local-dynamic TLS accesses.
  if (getX86Subtarget().isTargetELF() && getOptLevel() != CodeGenOpt::None)
    addPass(createCleanupLocalDynamicTLSPass());

  addPass(createX86GlobalBaseRegPass());

  return false;
}

bool X86PassConfig::addILPOpts() {
  addPass(&EarlyIfConverterID);
  return true;
}

void X86PassConfig::addPostRegAlloc() {
  addPass(createX86FloatingPointStackifierPass());
}

void X86PassConfig::addPreEmitPass() {
  if (getOptLevel() != CodeGenOpt::None && getX86Subtarget().hasSSE2())
    addPass(createExecutionDependencyFixPass(&X86::VR128RegClass));

  if (UseVZeroUpper)
    addPass(createX86IssueVZeroUpperPass());

  if (getOptLevel() != CodeGenOpt::None) {
    addPass(createX86PadShortFunctions());
    addPass(createX86FixupLEAs());
  }
}
