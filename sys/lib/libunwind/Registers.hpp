//===----------------------------- Registers.hpp --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//
//  Models register sets for supported processors.
//
//===----------------------------------------------------------------------===//
#ifndef __REGISTERS_HPP__
#define __REGISTERS_HPP__

#include <cassert>
#include <cstdint>

namespace _Unwind {

enum {
  REGNO_X86_EAX = 0,
  REGNO_X86_ECX = 1,
  REGNO_X86_EDX = 2,
  REGNO_X86_EBX = 3,
  REGNO_X86_ESP = 4,
  REGNO_X86_EBP = 5,
  REGNO_X86_ESI = 6,
  REGNO_X86_EDI = 7,
  REGNO_X86_EIP = 8,
};

class Registers_x86 {
public:
  enum {
    LAST_RESTORE_REG = REGNO_X86_EIP,
    IP_PSEUDO_REG = REGNO_X86_EIP,
    LAST_REGISTER = REGNO_X86_EIP,
  };

  __dso_hidden Registers_x86();

  static int dwarf2regno(int num) { return num; }

  bool validRegister(int num) const {
    return num >= REGNO_X86_EAX && num <= REGNO_X86_EDI;
  }

  uint32_t getRegister(int num) const {
    assert(validRegister(num));
    return reg[num];
  }

  void setRegister(int num, uint32_t value) {
    assert(validRegister(num));
    reg[num] = value;
  }

  uint32_t getIP() const { return reg[REGNO_X86_EIP]; }

  void setIP(uint32_t value) { reg[REGNO_X86_EIP] = value; }

  uint32_t getSP() const { return reg[REGNO_X86_ESP]; }

  void setSP(uint32_t value) { reg[REGNO_X86_ESP] = value; }

  bool validFloatVectorRegister(int num) const { return false; }

  void copyFloatVectorRegister(int num, uint32_t addr) {
  }

  __dso_hidden void jumpto() const __dead;

private:
  uint32_t reg[REGNO_X86_EIP + 1];
};

enum {
  REGNO_X86_64_RAX = 0,
  REGNO_X86_64_RDX = 1,
  REGNO_X86_64_RCX = 2,
  REGNO_X86_64_RBX = 3,
  REGNO_X86_64_RSI = 4,
  REGNO_X86_64_RDI = 5,
  REGNO_X86_64_RBP = 6,
  REGNO_X86_64_RSP = 7,
  REGNO_X86_64_R8 = 8,
  REGNO_X86_64_R9 = 9,
  REGNO_X86_64_R10 = 10,
  REGNO_X86_64_R11 = 11,
  REGNO_X86_64_R12 = 12,
  REGNO_X86_64_R13 = 13,
  REGNO_X86_64_R14 = 14,
  REGNO_X86_64_R15 = 15,
  REGNO_X86_64_RIP = 16,
};

class Registers_x86_64 {
public:
  enum {
    LAST_RESTORE_REG = REGNO_X86_64_RIP,
    IP_PSEUDO_REG = REGNO_X86_64_RIP,
    LAST_REGISTER = REGNO_X86_64_RIP,
  };

  __dso_hidden Registers_x86_64();

  static int dwarf2regno(int num) { return num; }

  bool validRegister(int num) const {
    return num >= REGNO_X86_64_RAX && num <= REGNO_X86_64_R15;
  }

  uint64_t getRegister(int num) const {
    assert(validRegister(num));
    return reg[num];
  }

  void setRegister(int num, uint64_t value) {
    assert(validRegister(num));
    reg[num] = value;
  }

  uint64_t getIP() const { return reg[REGNO_X86_64_RIP]; }

  void setIP(uint64_t value) { reg[REGNO_X86_64_RIP] = value; }

  uint64_t getSP() const { return reg[REGNO_X86_64_RSP]; }

  void setSP(uint64_t value) { reg[REGNO_X86_64_RSP] = value; }

  bool validFloatVectorRegister(int num) const { return false; }

  void copyFloatVectorRegister(int num, uint64_t addr) {
  }

  __dso_hidden void jumpto() const __dead;

private:
  uint64_t reg[REGNO_X86_64_RIP + 1];
};

enum {
  DWARF_PPC32_R0 = 0,
  DWARF_PPC32_R31 = 31,
  DWARF_PPC32_F0 = 32,
  DWARF_PPC32_F31 = 63,
  DWARF_PPC32_V0 = 1124,
  DWARF_PPC32_V31 = 1155,
  DWARF_PPC32_LR = 65,
  DWARF_PPC32_CTR = 66,
  DWARF_PPC32_XER = 76,
  REGNO_PPC32_R0 = 0,
  REGNO_PPC32_R1 = 0,
  REGNO_PPC32_R31 = 31,
  REGNO_PPC32_CR = 32,
  REGNO_PPC32_LR = 33,
  REGNO_PPC32_CTR = 34,
  REGNO_PPC32_XER = 35,
  REGNO_PPC32_SRR0 = 36,
  REGNO_PPC32_F0 = REGNO_PPC32_SRR0 + 1,
  REGNO_PPC32_F31 = REGNO_PPC32_F0 + 31,
  REGNO_PPC32_V0 = REGNO_PPC32_F31 + 1,
  REGNO_PPC32_V31 = REGNO_PPC32_V0 + 31,
};

class Registers_ppc32 {
public:
  enum {
    LAST_RESTORE_REG = REGNO_PPC32_V31,
    IP_PSEUDO_REG = REGNO_PPC32_SRR0,
    LAST_REGISTER = REGNO_PPC32_V31,
  };

  __dso_hidden Registers_ppc32();

  static int dwarf2regno(int num) {
    if (num >= DWARF_PPC32_R0 && num <= DWARF_PPC32_R31)
      return REGNO_PPC32_R0 + (num - DWARF_PPC32_R0);
    if (num >= DWARF_PPC32_F0 && num <= DWARF_PPC32_F31)
      return REGNO_PPC32_F0 + (num - DWARF_PPC32_F0);
    if (num >= DWARF_PPC32_V0 && num <= DWARF_PPC32_V31)
      return REGNO_PPC32_V0 + (num - DWARF_PPC32_V0);
    return LAST_REGISTER + 1;
  }

  bool validRegister(int num) const {
    return num >= 0 && num <= LAST_RESTORE_REG;
  }

  uint64_t getRegister(int num) const {
    assert(validRegister(num));
    return reg[num];
  }

  void setRegister(int num, uint64_t value) {
    assert(validRegister(num));
    reg[num] = value;
  }

  uint64_t getIP() const { return reg[REGNO_PPC32_SRR0]; }

  void setIP(uint64_t value) { reg[REGNO_PPC32_SRR0] = value; }

  uint64_t getSP() const { return reg[REGNO_PPC32_R1]; }

  void setSP(uint64_t value) { reg[REGNO_PPC32_R1] = value; }

  bool validFloatVectorRegister(int num) const {
    return (num >= REGNO_PPC32_F0 && num <= REGNO_PPC32_F31) ||
           (num >= REGNO_PPC32_V0 && num <= REGNO_PPC32_V31);
  }

  void copyFloatVectorRegister(int num, uint64_t addr_) {
    const void *addr = reinterpret_cast<const void *>(addr_);
    if (num >= REGNO_PPC32_F0 && num <= REGNO_PPC32_F31)
      memcpy(fpreg + (num - REGNO_PPC32_F0), addr, sizeof(fpreg[0]));
    else
      memcpy(vecreg + (num - REGNO_PPC32_V0), addr, sizeof(vecreg[0]));
  }

  __dso_hidden void jumpto() const __dead;

private:
  struct vecreg_t {
    uint64_t low, high;
  };
  uint32_t reg[REGNO_PPC32_SRR0 + 1];
  uint64_t fpreg[32];
  vecreg_t vecreg[64];
};

} // namespace _Unwind

#endif // __REGISTERS_HPP__
