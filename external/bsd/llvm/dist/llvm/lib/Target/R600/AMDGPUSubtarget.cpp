//===-- AMDGPUSubtarget.cpp - AMDGPU Subtarget Information ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
/// \file
/// \brief Implements the AMDGPU specific subclass of TargetSubtarget.
//
//===----------------------------------------------------------------------===//

#include "AMDGPUSubtarget.h"
#include "R600ISelLowering.h"
#include "R600InstrInfo.h"
#include "R600MachineScheduler.h"
#include "SIISelLowering.h"
#include "SIInstrInfo.h"
#include "SIMachineFunctionInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/CodeGen/MachineScheduler.h"

using namespace llvm;

#define DEBUG_TYPE "amdgpu-subtarget"

#define GET_SUBTARGETINFO_ENUM
#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "AMDGPUGenSubtargetInfo.inc"

static std::string computeDataLayout(const AMDGPUSubtarget &ST) {
  std::string Ret = "e-p:32:32";

  if (ST.is64bit()) {
    // 32-bit private, local, and region pointers. 64-bit global and constant.
    Ret += "-p1:64:64-p2:64:64-p3:32:32-p4:64:64-p5:32:32-p24:64:64";
  }

  Ret += "-i64:64-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256"
         "-v512:512-v1024:1024-v2048:2048-n32:64";

  return Ret;
}

AMDGPUSubtarget &
AMDGPUSubtarget::initializeSubtargetDependencies(StringRef GPU, StringRef FS) {
  // Determine default and user-specified characteristics
  // On SI+, we want FP64 denormals to be on by default. FP32 denormals can be
  // enabled, but some instructions do not respect them and they run at the
  // double precision rate, so don't enable by default.
  //
  // We want to be able to turn these off, but making this a subtarget feature
  // for SI has the unhelpful behavior that it unsets everything else if you
  // disable it.

  SmallString<256> FullFS("+promote-alloca,+fp64-denormals,");
  FullFS += FS;

  ParseSubtargetFeatures(GPU, FullFS);

  // FIXME: I don't think think Evergreen has any useful support for
  // denormals, but should be checked. Should we issue a warning somewhere
  // if someone tries to enable these?
  if (getGeneration() <= AMDGPUSubtarget::NORTHERN_ISLANDS) {
    FP32Denormals = false;
    FP64Denormals = false;
  }
  return *this;
}

AMDGPUSubtarget::AMDGPUSubtarget(StringRef TT, StringRef GPU, StringRef FS,
                                 TargetMachine &TM)
    : AMDGPUGenSubtargetInfo(TT, GPU, FS), DevName(GPU), Is64bit(false),
      DumpCode(false), R600ALUInst(false), HasVertexCache(false),
      TexVTXClauseSize(0), Gen(AMDGPUSubtarget::R600), FP64(false),
      FP64Denormals(false), FP32Denormals(false), CaymanISA(false),
      FlatAddressSpace(false), EnableIRStructurizer(true),
      EnablePromoteAlloca(false), EnableIfCvt(true),
      EnableLoadStoreOpt(false), WavefrontSize(0), CFALUBug(false), LocalMemorySize(0),
      EnableVGPRSpilling(false),SGPRInitBug(false),
      DL(computeDataLayout(initializeSubtargetDependencies(GPU, FS))),
      FrameLowering(TargetFrameLowering::StackGrowsUp,
                    64 * 16, // Maximum stack alignment (long16)
                    0),
      InstrItins(getInstrItineraryForCPU(GPU)),
      TargetTriple(TT) {
  if (getGeneration() <= AMDGPUSubtarget::NORTHERN_ISLANDS) {
    InstrInfo.reset(new R600InstrInfo(*this));
    TLInfo.reset(new R600TargetLowering(TM));
  } else {
    InstrInfo.reset(new SIInstrInfo(*this));
    TLInfo.reset(new SITargetLowering(TM));
  }
}

unsigned AMDGPUSubtarget::getStackEntrySize() const {
  assert(getGeneration() <= NORTHERN_ISLANDS);
  switch(getWavefrontSize()) {
  case 16:
    return 8;
  case 32:
    return hasCaymanISA() ? 4 : 8;
  case 64:
    return 4;
  default:
    llvm_unreachable("Illegal wavefront size.");
  }
}

unsigned AMDGPUSubtarget::getAmdKernelCodeChipID() const {
  switch(getGeneration()) {
  default: llvm_unreachable("ChipID unknown");
  case SEA_ISLANDS: return 12;
  }
}

bool AMDGPUSubtarget::isVGPRSpillingEnabled(
                                       const SIMachineFunctionInfo *MFI) const {
  return MFI->getShaderType() == ShaderType::COMPUTE || EnableVGPRSpilling;
}

void AMDGPUSubtarget::overrideSchedPolicy(MachineSchedPolicy &Policy,
                                          MachineInstr *begin,
                                          MachineInstr *end,
                                          unsigned NumRegionInstrs) const {
  if (getGeneration() >= SOUTHERN_ISLANDS) {

    // Track register pressure so the scheduler can try to decrease
    // pressure once register usage is above the threshold defined by
    // SIRegisterInfo::getRegPressureSetLimit()
    Policy.ShouldTrackPressure = true;

    // Enabling both top down and bottom up scheduling seems to give us less
    // register spills than just using one of these approaches on its own.
    Policy.OnlyTopDown = false;
    Policy.OnlyBottomUp = false;
  }
}
