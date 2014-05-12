//===------------------------- UnwindCursor.hpp ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//
// C++ interface to lower levels of libuwind
//===----------------------------------------------------------------------===//

#ifndef __UNWINDCURSOR_HPP__
#define __UNWINDCURSOR_HPP__

#include <stdint.h>
#include <stdlib.h>
#if !defined(__minix)
#include <pthread.h>
#endif /* !defined(__minix) */

#include "AddressSpace.hpp"
#include "DwarfInstructions.hpp"
#include "Registers.hpp"

namespace _Unwind {

template <typename A, typename R> class UnwindCursor {
public:
  UnwindCursor(R &regs, A &as)
      : fRegisters(regs), fAddressSpace(as), fUnwindInfoMissing(false),
        fIsSignalFrame(false) {
    memset(&fInfo, 0, sizeof(fInfo));
  }

  uint64_t getIP() const { return fRegisters.getIP(); }

  void setIP(uint64_t value) { return fRegisters.setIP(value); }

  uint64_t getSP() const { return fRegisters.getSP(); }

  void setSP(uint64_t value) { return fRegisters.setSP(value); }

  bool validReg(int regNum) { return fRegisters.validRegister(regNum); }

  uint64_t getReg(int regNum) { return fRegisters.getRegister(regNum); }

  void setReg(int regNum, uint64_t value) {
    fRegisters.setRegister(regNum, value);
  }

  step_result step() {
    // Bottom of stack is defined as having no more unwind info.
    if (fUnwindInfoMissing)
      return UNW_STEP_END;

    // Apply unwinding to register set.
    switch (this->stepWithDwarfFDE()) {
    case UNW_STEP_FAILED:
      return UNW_STEP_FAILED;
    case UNW_STEP_END:
      return UNW_STEP_END;
    case UNW_STEP_SUCCESS:
      this->setInfoBasedOnIPRegister(true);
      if (fUnwindInfoMissing)
        return UNW_STEP_END;

      if (fInfo.extra_args)
        setSP(getSP() + fInfo.extra_args);
      return UNW_STEP_SUCCESS;
    }
    __builtin_unreachable();
  }

  void getInfo(unw_proc_info_t *info) { *info = fInfo; }

  bool isSignalFrame() { return fIsSignalFrame; }
  void setInfoBasedOnIPRegister(bool isReturnAddress = false);

  void jumpto() { fRegisters.jumpto(); }

private:
  typedef typename A::pint_t pint_t;
  typedef uint32_t EncodedUnwindInfo;

  bool getInfoFromDwarfSection(pint_t, pint_t, uint32_t, uint32_t);

  step_result stepWithDwarfFDE() {
    return DwarfInstructions<A, R>::stepWithDwarf(
        fAddressSpace, this->getIP(), fInfo.unwind_info, fRegisters, &fInfo);
  }

  unw_proc_info_t fInfo;
  R fRegisters;
  A &fAddressSpace;
  bool fUnwindInfoMissing;
  bool fIsSignalFrame;
};

template <typename A, typename R>
void UnwindCursor<A, R>::setInfoBasedOnIPRegister(bool isReturnAddress) {
  pint_t pc = this->getIP();

  // If the last line of a function is a "throw", the compiler sometimes
  // emits no instructions after the call to __cxa_throw.  This means
  // the return address is actually the start of the next function.
  // To disambiguate this, back up the PC when we know it is a return
  // address.
  if (isReturnAddress)
    --pc;

  pint_t fdeStart, data_base;
  if (!fAddressSpace.findFDE(pc, fdeStart, data_base)) {
    fUnwindInfoMissing = true;
    return;
  }
  fInfo.data_base = data_base;

  typename CFI_Parser<A, R>::FDE_Info fdeInfo;
  typename CFI_Parser<A, R>::CIE_Info cieInfo;
  CFI_Parser<A, R>::decodeFDE(fAddressSpace, fdeStart, &fdeInfo, &cieInfo,
                              &fInfo);
  if (pc < fdeInfo.pcStart || pc > fdeInfo.pcEnd) {
    fUnwindInfoMissing = true;
    return;
  }
  fInfo.start_ip = fdeInfo.pcStart;

  typename CFI_Parser<A, R>::PrologInfo prolog;
  if (!CFI_Parser<A, R>::parseFDEInstructions(fAddressSpace, fdeInfo, cieInfo,
                                              pc, &prolog, &fInfo)) {
    fUnwindInfoMissing = true;
    return;
  }
  // Save off parsed FDE info
  fInfo.end_ip = fdeInfo.pcEnd;
  fInfo.lsda = fdeInfo.lsda;
  fInfo.handler = cieInfo.personality;
  fInfo.extra_args = prolog.spExtraArgSize;
  fInfo.unwind_info = fdeInfo.fdeStart;
}

}; // namespace _Unwind

#endif // __UNWINDCURSOR_HPP__
