//===-------------------------- DwarfInstructions.hpp ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//
//  Processor specific interpretation of DWARF unwind info.
//
//===----------------------------------------------------------------------===//

#ifndef __DWARF_INSTRUCTIONS_HPP__
#define __DWARF_INSTRUCTIONS_HPP__

#include <cstdint>
#include <cstdlib>

#include "dwarf2.h"
#include "AddressSpace.hpp"
#include "Registers.hpp"
#include "DwarfParser.hpp"

namespace _Unwind {

enum step_result {
  UNW_STEP_SUCCESS,
  UNW_STEP_END,
  UNW_STEP_FAILED
};

/// DwarfInstructions maps abtract dwarf unwind instructions to a particular
/// architecture
template <typename A, typename R> class DwarfInstructions {
public:
  typedef typename A::pint_t pint_t;
  typedef typename A::sint_t sint_t;

  static step_result stepWithDwarf(A &, pint_t, pint_t, R &, unw_proc_info_t *);

private:
  static pint_t evaluateExpression(pint_t, A &, const R &, pint_t);
  static pint_t
  getSavedRegister(A &, const R &, pint_t,
                   const typename CFI_Parser<A, R>::RegisterLocation &);
  static pint_t
  computeRegisterLocation(A &, const R &, pint_t,
                          const typename CFI_Parser<A, R>::RegisterLocation &);

  static int lastRestoreReg(const R &) { return R::LAST_RESTORE_REG; }

  static pint_t getCFA(A &addressSpace,
                       const typename CFI_Parser<A, R>::PrologInfo &prolog,
                       const R &registers) {
    if (prolog.cfaRegister != 0)
      return registers.getRegister(prolog.cfaRegister) +
             prolog.cfaRegisterOffset;
    if (prolog.cfaExpression != 0)
      return evaluateExpression(prolog.cfaExpression, addressSpace, registers,
                                0);
    assert(0 && "getCFA(): unknown location");
    __builtin_unreachable();
  }
};

template <typename A, typename R>
typename A::pint_t DwarfInstructions<A, R>::getSavedRegister(
    A &addressSpace, const R &registers, pint_t cfa,
    const typename CFI_Parser<A, R>::RegisterLocation &savedReg) {
  switch (savedReg.location) {
  case CFI_Parser<A, R>::kRegisterInCFA:
    return addressSpace.getP(cfa + savedReg.value);

  case CFI_Parser<A, R>::kRegisterAtExpression:
    return addressSpace.getP(
        evaluateExpression(savedReg.value, addressSpace, registers, cfa));

  case CFI_Parser<A, R>::kRegisterIsExpression:
    return evaluateExpression(savedReg.value, addressSpace, registers, cfa);

  case CFI_Parser<A, R>::kRegisterInRegister:
    return registers.getRegister(savedReg.value);

  case CFI_Parser<A, R>::kRegisterUnused:
  case CFI_Parser<A, R>::kRegisterOffsetFromCFA:
    assert(0 && "unsupported restore location for register");
  }
  __builtin_unreachable();
}

template <typename A, typename R>
typename DwarfInstructions<A, R>::pint_t
DwarfInstructions<A, R>::computeRegisterLocation(
    A &addressSpace, const R &registers, pint_t cfa,
    const typename CFI_Parser<A, R>::RegisterLocation &savedReg) {
  switch (savedReg.location) {
  case CFI_Parser<A, R>::kRegisterInCFA:
    return cfa + savedReg.value;

  case CFI_Parser<A, R>::kRegisterAtExpression:
    return evaluateExpression(savedReg.value, addressSpace, registers, cfa);

  case CFI_Parser<A, R>::kRegisterIsExpression:
  case CFI_Parser<A, R>::kRegisterUnused:
  case CFI_Parser<A, R>::kRegisterOffsetFromCFA:
  case CFI_Parser<A, R>::kRegisterInRegister:
    assert(0 && "unsupported restore location for float/vector register");
  }
  __builtin_unreachable();
}

template <typename A, typename R>
step_result DwarfInstructions<A, R>::stepWithDwarf(A &addressSpace, pint_t pc,
                                                   pint_t fdeStart,
                                                   R &registers,
                                                   unw_proc_info_t *ctx) {
  typename CFI_Parser<A, R>::FDE_Info fdeInfo;
  typename CFI_Parser<A, R>::CIE_Info cieInfo;
  if (!CFI_Parser<A, R>::decodeFDE(addressSpace, fdeStart, &fdeInfo, &cieInfo,
                                   ctx))
    return UNW_STEP_FAILED;

  typename CFI_Parser<A, R>::PrologInfo prolog;
  if (!CFI_Parser<A, R>::parseFDEInstructions(addressSpace, fdeInfo, cieInfo,
                                              pc, &prolog, ctx))
    return UNW_STEP_FAILED;

  // Create working copy of the register set.
  R newRegisters = registers;

  // Get pointer to CFA by the architecture-specific code.
  pint_t cfa = getCFA(addressSpace, prolog, registers);

  // Restore registers according to DWARF instructions
  pint_t returnAddress = 0;
  for (int i = 0; i <= lastRestoreReg(newRegisters); ++i) {
    if (prolog.savedRegisters[i].location == CFI_Parser<A, R>::kRegisterUnused)
      continue;
    if (i == (int)cieInfo.returnAddressRegister)
      returnAddress = getSavedRegister(addressSpace, registers, cfa,
                                       prolog.savedRegisters[i]);
    else if (registers.validRegister(i))
      newRegisters.setRegister(i, getSavedRegister(addressSpace, registers, cfa,
                                                   prolog.savedRegisters[i]));
    else if (registers.validFloatVectorRegister(i))
      newRegisters.copyFloatVectorRegister(
          i, computeRegisterLocation(addressSpace, registers, cfa,
                                     prolog.savedRegisters[i]));
    else
      return UNW_STEP_FAILED;
  }

  // The CFA is defined as the stack pointer at the call site.
  // Therefore the SP is restored by setting it to the CFA.
  newRegisters.setSP(cfa);
  newRegisters.setIP(returnAddress + R::RETURN_OFFSET);

  // Now replace register set with the working copy.
  registers = newRegisters;

  return UNW_STEP_SUCCESS;
}

template <typename A, typename R>
typename A::pint_t
DwarfInstructions<A, R>::evaluateExpression(pint_t expression, A &addressSpace,
                                            const R &registers,
                                            pint_t initialStackValue) {
  pint_t p = expression;
  pint_t expressionEnd = expression + 20; // Rough estimate
  uint64_t length = addressSpace.getULEB128(p, expressionEnd);
  expressionEnd = p + length;
  pint_t stack[100];
  pint_t *sp = stack;
  *(++sp) = initialStackValue;

  while (p < expressionEnd) {
    uint8_t opcode = addressSpace.get8(p++);
    sint_t svalue;
    pint_t value;
    uint32_t reg;
    switch (opcode) {
    case DW_OP_addr:
      // push immediate address sized value
      value = addressSpace.getP(p);
      p += sizeof(pint_t);
      *(++sp) = value;
      break;

    case DW_OP_deref:
      // pop stack, dereference, push result
      value = *sp--;
      *(++sp) = addressSpace.getP(value);
      break;

    case DW_OP_const1u:
      // push immediate 1 byte value
      value = addressSpace.get8(p);
      p += 1;
      *(++sp) = value;
      break;

    case DW_OP_const1s:
      // push immediate 1 byte signed value
      svalue = (int8_t)addressSpace.get8(p);
      p += 1;
      *(++sp) = svalue;
      break;

    case DW_OP_const2u:
      // push immediate 2 byte value
      value = addressSpace.get16(p);
      p += 2;
      *(++sp) = value;
      break;

    case DW_OP_const2s:
      // push immediate 2 byte signed value
      svalue = (int16_t)addressSpace.get16(p);
      p += 2;
      *(++sp) = svalue;
      break;

    case DW_OP_const4u:
      // push immediate 4 byte value
      value = addressSpace.get32(p);
      p += 4;
      *(++sp) = value;
      break;

    case DW_OP_const4s:
      // push immediate 4 byte signed value
      svalue = (int32_t)addressSpace.get32(p);
      p += 4;
      *(++sp) = svalue;
      break;

    case DW_OP_const8u:
      // push immediate 8 byte value
      value = addressSpace.get64(p);
      p += 8;
      *(++sp) = value;
      break;

    case DW_OP_const8s:
      // push immediate 8 byte signed value
      value = (int32_t)addressSpace.get64(p);
      p += 8;
      *(++sp) = value;
      break;

    case DW_OP_constu:
      // push immediate ULEB128 value
      value = addressSpace.getULEB128(p, expressionEnd);
      *(++sp) = value;
      break;

    case DW_OP_consts:
      // push immediate SLEB128 value
      svalue = addressSpace.getSLEB128(p, expressionEnd);
      *(++sp) = svalue;
      break;

    case DW_OP_dup:
      // push top of stack
      value = *sp;
      *(++sp) = value;
      break;

    case DW_OP_drop:
      // pop
      --sp;
      break;

    case DW_OP_over:
      // dup second
      value = sp[-1];
      *(++sp) = value;
      break;

    case DW_OP_pick:
      // pick from
      reg = addressSpace.get8(p);
      p += 1;
      value = sp[-reg];
      *(++sp) = value;
      break;

    case DW_OP_swap:
      // swap top two
      value = sp[0];
      sp[0] = sp[-1];
      sp[-1] = value;
      break;

    case DW_OP_rot:
      // rotate top three
      value = sp[0];
      sp[0] = sp[-1];
      sp[-1] = sp[-2];
      sp[-2] = value;
      break;

    case DW_OP_xderef:
      // pop stack, dereference, push result
      value = *sp--;
      *sp = *((uint64_t *)value);
      break;

    case DW_OP_abs:
      svalue = *sp;
      if (svalue < 0)
        *sp = -svalue;
      break;

    case DW_OP_and:
      value = *sp--;
      *sp &= value;
      break;

    case DW_OP_div:
      svalue = *sp--;
      *sp = *sp / svalue;
      break;

    case DW_OP_minus:
      svalue = *sp--;
      *sp = *sp - svalue;
      break;

    case DW_OP_mod:
      svalue = *sp--;
      *sp = *sp % svalue;
      break;

    case DW_OP_mul:
      svalue = *sp--;
      *sp = *sp * svalue;
      break;

    case DW_OP_neg:
      *sp = 0 - *sp;
      break;

    case DW_OP_not:
      svalue = *sp;
      *sp = ~svalue;
      break;

    case DW_OP_or:
      value = *sp--;
      *sp |= value;
      break;

    case DW_OP_plus:
      value = *sp--;
      *sp += value;
      break;

    case DW_OP_plus_uconst:
      // pop stack, add uelb128 constant, push result
      *sp += addressSpace.getULEB128(p, expressionEnd);
      break;

    case DW_OP_shl:
      value = *sp--;
      *sp = *sp << value;
      break;

    case DW_OP_shr:
      value = *sp--;
      *sp = *sp >> value;
      break;

    case DW_OP_shra:
      value = *sp--;
      svalue = *sp;
      *sp = svalue >> value;
      break;

    case DW_OP_xor:
      value = *sp--;
      *sp ^= value;
      break;

    case DW_OP_skip:
      svalue = (int16_t)addressSpace.get16(p);
      p += 2;
      p += svalue;
      break;

    case DW_OP_bra:
      svalue = (int16_t)addressSpace.get16(p);
      p += 2;
      if (*sp--)
        p += svalue;
      break;

    case DW_OP_eq:
      value = *sp--;
      *sp = (*sp == value);
      break;

    case DW_OP_ge:
      value = *sp--;
      *sp = (*sp >= value);
      break;

    case DW_OP_gt:
      value = *sp--;
      *sp = (*sp > value);
      break;

    case DW_OP_le:
      value = *sp--;
      *sp = (*sp <= value);
      break;

    case DW_OP_lt:
      value = *sp--;
      *sp = (*sp < value);
      break;

    case DW_OP_ne:
      value = *sp--;
      *sp = (*sp != value);
      break;

    case DW_OP_lit0:
    case DW_OP_lit1:
    case DW_OP_lit2:
    case DW_OP_lit3:
    case DW_OP_lit4:
    case DW_OP_lit5:
    case DW_OP_lit6:
    case DW_OP_lit7:
    case DW_OP_lit8:
    case DW_OP_lit9:
    case DW_OP_lit10:
    case DW_OP_lit11:
    case DW_OP_lit12:
    case DW_OP_lit13:
    case DW_OP_lit14:
    case DW_OP_lit15:
    case DW_OP_lit16:
    case DW_OP_lit17:
    case DW_OP_lit18:
    case DW_OP_lit19:
    case DW_OP_lit20:
    case DW_OP_lit21:
    case DW_OP_lit22:
    case DW_OP_lit23:
    case DW_OP_lit24:
    case DW_OP_lit25:
    case DW_OP_lit26:
    case DW_OP_lit27:
    case DW_OP_lit28:
    case DW_OP_lit29:
    case DW_OP_lit30:
    case DW_OP_lit31:
      value = opcode - DW_OP_lit0;
      *(++sp) = value;
      break;

    case DW_OP_reg0:
    case DW_OP_reg1:
    case DW_OP_reg2:
    case DW_OP_reg3:
    case DW_OP_reg4:
    case DW_OP_reg5:
    case DW_OP_reg6:
    case DW_OP_reg7:
    case DW_OP_reg8:
    case DW_OP_reg9:
    case DW_OP_reg10:
    case DW_OP_reg11:
    case DW_OP_reg12:
    case DW_OP_reg13:
    case DW_OP_reg14:
    case DW_OP_reg15:
    case DW_OP_reg16:
    case DW_OP_reg17:
    case DW_OP_reg18:
    case DW_OP_reg19:
    case DW_OP_reg20:
    case DW_OP_reg21:
    case DW_OP_reg22:
    case DW_OP_reg23:
    case DW_OP_reg24:
    case DW_OP_reg25:
    case DW_OP_reg26:
    case DW_OP_reg27:
    case DW_OP_reg28:
    case DW_OP_reg29:
    case DW_OP_reg30:
    case DW_OP_reg31:
      reg = opcode - DW_OP_reg0;
      *(++sp) = registers.getRegister(reg);
      break;

    case DW_OP_regx:
      reg = addressSpace.getULEB128(p, expressionEnd);
      *(++sp) = registers.getRegister(reg);
      break;

    case DW_OP_breg0:
    case DW_OP_breg1:
    case DW_OP_breg2:
    case DW_OP_breg3:
    case DW_OP_breg4:
    case DW_OP_breg5:
    case DW_OP_breg6:
    case DW_OP_breg7:
    case DW_OP_breg8:
    case DW_OP_breg9:
    case DW_OP_breg10:
    case DW_OP_breg11:
    case DW_OP_breg12:
    case DW_OP_breg13:
    case DW_OP_breg14:
    case DW_OP_breg15:
    case DW_OP_breg16:
    case DW_OP_breg17:
    case DW_OP_breg18:
    case DW_OP_breg19:
    case DW_OP_breg20:
    case DW_OP_breg21:
    case DW_OP_breg22:
    case DW_OP_breg23:
    case DW_OP_breg24:
    case DW_OP_breg25:
    case DW_OP_breg26:
    case DW_OP_breg27:
    case DW_OP_breg28:
    case DW_OP_breg29:
    case DW_OP_breg30:
    case DW_OP_breg31:
      reg = opcode - DW_OP_breg0;
      svalue = addressSpace.getSLEB128(p, expressionEnd);
      *(++sp) = registers.getRegister(reg) + svalue;
      break;

    case DW_OP_bregx:
      reg = addressSpace.getULEB128(p, expressionEnd);
      svalue = addressSpace.getSLEB128(p, expressionEnd);
      *(++sp) = registers.getRegister(reg) + svalue;
      break;

    case DW_OP_deref_size:
      // pop stack, dereference, push result
      value = *sp--;
      switch (addressSpace.get8(p++)) {
      case 1:
        value = addressSpace.get8(value);
        break;
      case 2:
        value = addressSpace.get16(value);
        break;
      case 4:
        value = addressSpace.get32(value);
        break;
      case 8:
        value = addressSpace.get64(value);
        break;
      default:
        assert(0 && "DW_OP_deref_size with bad size");
      }
      *(++sp) = value;
      break;

    case DW_OP_fbreg:
    case DW_OP_piece:
    case DW_OP_xderef_size:
    case DW_OP_nop:
    case DW_OP_push_object_addres:
    case DW_OP_call2:
    case DW_OP_call4:
    case DW_OP_call_ref:
    default:
      assert(0 && "dwarf opcode not implemented");
    }
  }
  return *sp;
}

} // namespace _Unwind

#endif // __DWARF_INSTRUCTIONS_HPP__
