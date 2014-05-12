//===--------------------------- DwarfParser.hpp --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//
//  Parses DWARF CFIs (FDEs and CIEs).
//
//===----------------------------------------------------------------------===//

#ifndef __DWARF_PARSER_HPP__
#define __DWARF_PARSER_HPP__

#include <cstdint>
#include <cstdlib>

#include "dwarf2.h"
#include "AddressSpace.hpp"

namespace _Unwind {

/// CFI_Parser does basic parsing of a CFI (Call Frame Information) records.
/// See Dwarf Spec for details:
///    http://refspecs.linuxbase.org/LSB_3.1.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
///
template <typename A, typename R> class CFI_Parser {
public:
  typedef typename A::pint_t pint_t;

  /// Information encoded in a CIE (Common Information Entry)
  struct CIE_Info {
    pint_t cieStart;
    pint_t cieLength;
    pint_t cieInstructions;
    pint_t personality;
    uint32_t codeAlignFactor;
    int dataAlignFactor;
    uint8_t pointerEncoding;
    uint8_t lsdaEncoding;
    uint8_t personalityEncoding;
    uint8_t personalityOffsetInCIE;
    bool isSignalFrame;
    bool fdesHaveAugmentationData;
    uint8_t returnAddressRegister;
  };

  /// Information about an FDE (Frame Description Entry)
  struct FDE_Info {
    pint_t fdeStart;
    pint_t fdeLength;
    pint_t fdeInstructions;
    pint_t pcStart;
    pint_t pcEnd;
    pint_t lsda;
  };

  /// Information about a frame layout and registers saved determined
  /// by "running" the DWARF FDE "instructions"
  enum {
    kMaxRegisterNumber = R::LAST_REGISTER + 1
  };
  enum RegisterSavedWhere {
    kRegisterUnused,
    kRegisterInCFA,
    kRegisterOffsetFromCFA,
    kRegisterInRegister,
    kRegisterAtExpression,
    kRegisterIsExpression,
  };
  struct RegisterLocation {
    RegisterSavedWhere location;
    int64_t value;
  };
  struct PrologInfo {
    uint32_t cfaRegister;
    int32_t cfaRegisterOffset; // CFA = (cfaRegister)+cfaRegisterOffset
    int64_t cfaExpression;     // CFA = expression
    uint32_t spExtraArgSize;
    uint32_t codeOffsetAtStackDecrement;
    RegisterLocation savedRegisters[kMaxRegisterNumber];
  };

  struct PrologInfoStackEntry {
    PrologInfoStackEntry(PrologInfoStackEntry *n, const PrologInfo &i)
        : next(n), info(i) {}
    PrologInfoStackEntry *next;
    PrologInfo info;
  };

  static void findPCRange(A &, pint_t, pint_t &, pint_t &);

  static bool decodeFDE(A &, pint_t, FDE_Info *, CIE_Info *,
                        unw_proc_info_t *ctx);
  static bool parseFDEInstructions(A &, const FDE_Info &, const CIE_Info &,
                                   pint_t, PrologInfo *, unw_proc_info_t *ctx);

  static bool parseCIE(A &, pint_t, CIE_Info *);

private:
  static bool parseInstructions(A &, pint_t, pint_t, const CIE_Info &, pint_t,
                                PrologInfoStackEntry *&, PrologInfo *,
                                unw_proc_info_t *ctx);
};

///
/// Parse a FDE and return the last PC it covers.
///
template <typename A, typename R>
void CFI_Parser<A, R>::findPCRange(A &addressSpace, pint_t fde, pint_t &pcStart,
                                   pint_t &pcEnd) {
  pcStart = 0;
  pcEnd = 0;
  pint_t p = fde;
  uint64_t cfiLength = addressSpace.get32(p);
  p += 4;
  if (cfiLength == 0xffffffff) {
    // 0xffffffff means length is really the next 8 Bytes.
    cfiLength = addressSpace.get64(p);
    p += 8;
  }
  if (cfiLength == 0)
    return;
  uint32_t ciePointer = addressSpace.get32(p);
  if (ciePointer == 0)
    return;
  pint_t nextCFI = p + cfiLength;
  pint_t cieStart = p - ciePointer;
  typename CFI_Parser<A, R>::CIE_Info cieInfo;
  if (!parseCIE(addressSpace, cieStart, &cieInfo))
    return;
  p += 4;
  // Parse pc begin and range.
  pcStart = addressSpace.getEncodedP(p, nextCFI, cieInfo.pointerEncoding, NULL);
  pcEnd = pcStart + addressSpace.getEncodedP(
                        p, nextCFI, cieInfo.pointerEncoding & 0x0F, NULL);
}

///
/// Parse a FDE into a CIE_Info and an FDE_Info
///
template <typename A, typename R>
bool CFI_Parser<A, R>::decodeFDE(A &addressSpace, pint_t fdeStart,
                                 FDE_Info *fdeInfo, CIE_Info *cieInfo,
                                 unw_proc_info_t *ctx) {
  pint_t p = fdeStart;
  uint64_t cfiLength = addressSpace.get32(p);
  p += 4;
  if (cfiLength == 0xffffffff) {
    // 0xffffffff means length is really the next 8 Bytes.
    cfiLength = addressSpace.get64(p);
    p += 8;
  }
  if (cfiLength == 0)
    return false;
  uint32_t ciePointer = addressSpace.get32(p);
  if (ciePointer == 0)
    return false;
  pint_t nextCFI = p + cfiLength;
  pint_t cieStart = p - ciePointer;
  if (!parseCIE(addressSpace, cieStart, cieInfo))
    return false;
  p += 4;
  // Parse pc begin and range.
  pint_t pcStart =
      addressSpace.getEncodedP(p, nextCFI, cieInfo->pointerEncoding, ctx);
  pint_t pcRange = addressSpace.getEncodedP(
      p, nextCFI, cieInfo->pointerEncoding & 0x0F, ctx);
  // Parse rest of info.
  fdeInfo->lsda = 0;
  // Check for augmentation length
  if (cieInfo->fdesHaveAugmentationData) {
    uintptr_t augLen = addressSpace.getULEB128(p, nextCFI);
    pint_t endOfAug = p + augLen;
    if (cieInfo->lsdaEncoding != DW_EH_PE_omit) {
      // Peek at value (without indirection).  Zero means no LSDA.
      pint_t lsdaStart = p;
      if (addressSpace.getEncodedP(p, nextCFI, cieInfo->lsdaEncoding & 0x0F,
                                   ctx) != 0) {
        // Reset pointer and re-parse LSDA address.
        p = lsdaStart;
        fdeInfo->lsda =
            addressSpace.getEncodedP(p, nextCFI, cieInfo->lsdaEncoding, ctx);
      }
    }
    p = endOfAug;
  }
  fdeInfo->fdeStart = fdeStart;
  fdeInfo->fdeLength = nextCFI - fdeStart;
  fdeInfo->fdeInstructions = p;
  fdeInfo->pcStart = pcStart;
  fdeInfo->pcEnd = pcStart + pcRange;
  return true;
}

/// Extract info from a CIE
template <typename A, typename R>
bool CFI_Parser<A, R>::parseCIE(A &addressSpace, pint_t cie,
                                CIE_Info *cieInfo) {
  cieInfo->pointerEncoding = 0;
  cieInfo->lsdaEncoding = DW_EH_PE_omit;
  cieInfo->personalityEncoding = 0;
  cieInfo->personalityOffsetInCIE = 0;
  cieInfo->personality = 0;
  cieInfo->codeAlignFactor = 0;
  cieInfo->dataAlignFactor = 0;
  cieInfo->isSignalFrame = false;
  cieInfo->fdesHaveAugmentationData = false;
  cieInfo->cieStart = cie;
  pint_t p = cie;
  uint64_t cieLength = addressSpace.get32(p);
  p += 4;
  pint_t cieContentEnd = p + cieLength;
  if (cieLength == 0xffffffff) {
    // 0xffffffff means length is really the next 8 Bytes.
    cieLength = addressSpace.get64(p);
    p += 8;
    cieContentEnd = p + cieLength;
  }
  if (cieLength == 0)
    return true;
  // CIE ID is always 0
  if (addressSpace.get32(p) != 0)
    return false;
  p += 4;
  // Version is always 1 or 3
  uint8_t version = addressSpace.get8(p);
  if (version != 1 && version != 3)
    return false;
  ++p;
  // Save start of augmentation string and find end.
  pint_t strStart = p;
  while (addressSpace.get8(p) != 0)
    ++p;
  ++p;
  // Parse code aligment factor
  cieInfo->codeAlignFactor = addressSpace.getULEB128(p, cieContentEnd);
  // Parse data alignment factor
  cieInfo->dataAlignFactor = addressSpace.getSLEB128(p, cieContentEnd);
  // Parse return address register
  cieInfo->returnAddressRegister = (uint8_t)addressSpace.getULEB128(p, cieContentEnd);
  // Parse augmentation data based on augmentation string.
  if (addressSpace.get8(strStart) == 'z') {
    // parse augmentation data length
    addressSpace.getULEB128(p, cieContentEnd);
    for (pint_t s = strStart; addressSpace.get8(s) != '\0'; ++s) {
      switch (addressSpace.get8(s)) {
      case 'z':
        cieInfo->fdesHaveAugmentationData = true;
        break;
      case 'P':
        cieInfo->personalityEncoding = addressSpace.get8(p);
        ++p;
        cieInfo->personalityOffsetInCIE = p - cie;
        cieInfo->personality = addressSpace.getEncodedP(
            p, cieContentEnd, cieInfo->personalityEncoding, NULL);
        break;
      case 'L':
        cieInfo->lsdaEncoding = addressSpace.get8(p);
        ++p;
        break;
      case 'R':
        cieInfo->pointerEncoding = addressSpace.get8(p);
        ++p;
        break;
      case 'S':
        cieInfo->isSignalFrame = true;
        break;
      default:
        // ignore unknown letters
        break;
      }
    }
  }
  cieInfo->cieLength = cieContentEnd - cieInfo->cieStart;
  cieInfo->cieInstructions = p;
  return true;
}

/// "Run" the dwarf instructions and create the abstact PrologInfo for an FDE.
template <typename A, typename R>
bool CFI_Parser<A, R>::parseFDEInstructions(A &addressSpace,
                                            const FDE_Info &fdeInfo,
                                            const CIE_Info &cieInfo,
                                            pint_t upToPC, PrologInfo *results,
                                            unw_proc_info_t *ctx) {
  // Clear results.
  memset(results, 0, sizeof(*results));
  PrologInfoStackEntry *rememberStack = NULL;

  // First parse the CIE then FDE instructions.
  if (!parseInstructions(addressSpace, cieInfo.cieInstructions,
                         cieInfo.cieStart + cieInfo.cieLength, cieInfo,
                         (pint_t)(-1), rememberStack, results, ctx))
    return false;
  return parseInstructions(addressSpace, fdeInfo.fdeInstructions,
                           fdeInfo.fdeStart + fdeInfo.fdeLength, cieInfo,
                           upToPC - fdeInfo.pcStart, rememberStack, results,
                           ctx);
}

/// "Run" the DWARF instructions.
template <typename A, typename R>
bool
CFI_Parser<A, R>::parseInstructions(A &addressSpace, pint_t instructions,
                                    pint_t instructionsEnd,
                                    const CIE_Info &cieInfo, pint_t pcoffset,
                                    PrologInfoStackEntry *&rememberStack,
                                    PrologInfo *results, unw_proc_info_t *ctx) {
  pint_t p = instructions;
  uint32_t codeOffset = 0;
  PrologInfo initialState = *results;

  // See Dwarf Spec, section 6.4.2 for details on unwind opcodes.
  while (p < instructionsEnd && codeOffset < pcoffset) {
    uint64_t reg;
    uint64_t reg2;
    int64_t offset;
    uint64_t length;
    uint8_t opcode = addressSpace.get8(p);
    uint8_t operand;
    PrologInfoStackEntry *entry;
    ++p;
    switch (opcode) {
    case DW_CFA_nop:
      break;
    case DW_CFA_set_loc:
      codeOffset = addressSpace.getEncodedP(p, instructionsEnd,
                                            cieInfo.pointerEncoding, ctx);
      break;
    case DW_CFA_advance_loc1:
      codeOffset += (addressSpace.get8(p) * cieInfo.codeAlignFactor);
      p += 1;
      break;
    case DW_CFA_advance_loc2:
      codeOffset += (addressSpace.get16(p) * cieInfo.codeAlignFactor);
      p += 2;
      break;
    case DW_CFA_advance_loc4:
      codeOffset += (addressSpace.get32(p) * cieInfo.codeAlignFactor);
      p += 4;
      break;
    case DW_CFA_offset_extended:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      offset =
          addressSpace.getULEB128(p, instructionsEnd) * cieInfo.dataAlignFactor;
      if (reg > kMaxRegisterNumber)
        return false;
      results->savedRegisters[reg].location = kRegisterInCFA;
      results->savedRegisters[reg].value = offset;
      break;
    case DW_CFA_restore_extended:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      if (reg > kMaxRegisterNumber)
        return false;
      results->savedRegisters[reg] = initialState.savedRegisters[reg];
      break;
    case DW_CFA_undefined:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      if (reg > kMaxRegisterNumber)
        return false;
      results->savedRegisters[reg].location = kRegisterUnused;
      break;
    case DW_CFA_same_value:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      if (reg > kMaxRegisterNumber)
        return false;
      // "same value" means register was stored in frame, but its current
      // value has not changed, so no need to restore from frame.
      // We model this as if the register was never saved.
      results->savedRegisters[reg].location = kRegisterUnused;
      break;
    case DW_CFA_register:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      reg2 = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      if (reg > kMaxRegisterNumber)
        return false;
      if (reg2 > kMaxRegisterNumber)
        return false;
      results->savedRegisters[reg].location = kRegisterInRegister;
      results->savedRegisters[reg].value = reg2;
      break;
    case DW_CFA_remember_state:
      // avoid operator new, because that would be an upward dependency
      entry = (PrologInfoStackEntry *)malloc(sizeof(PrologInfoStackEntry));
      if (entry == NULL)
        return false;

      entry->next = rememberStack;
      entry->info = *results;
      rememberStack = entry;
      break;
    case DW_CFA_restore_state:
      if (rememberStack == NULL)
        return false;
      {
        PrologInfoStackEntry *top = rememberStack;
        *results = top->info;
        rememberStack = top->next;
        free((char *)top);
      }
      break;
    case DW_CFA_def_cfa:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      offset = addressSpace.getULEB128(p, instructionsEnd);
      if (reg > kMaxRegisterNumber)
        return false;
      results->cfaRegister = reg;
      results->cfaRegisterOffset = offset;
      break;
    case DW_CFA_def_cfa_register:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      if (reg > kMaxRegisterNumber)
        return false;
      results->cfaRegister = reg;
      break;
    case DW_CFA_def_cfa_offset:
      results->cfaRegisterOffset = addressSpace.getULEB128(p, instructionsEnd);
      results->codeOffsetAtStackDecrement = codeOffset;
      break;
    case DW_CFA_def_cfa_expression:
      results->cfaRegister = 0;
      results->cfaExpression = p;
      length = addressSpace.getULEB128(p, instructionsEnd);
      p += length;
      break;
    case DW_CFA_expression:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      if (reg > kMaxRegisterNumber)
        return false;
      results->savedRegisters[reg].location = kRegisterAtExpression;
      results->savedRegisters[reg].value = p;
      length = addressSpace.getULEB128(p, instructionsEnd);
      p += length;
      break;
    case DW_CFA_offset_extended_sf:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      if (reg > kMaxRegisterNumber)
        return false;
      offset =
          addressSpace.getSLEB128(p, instructionsEnd) * cieInfo.dataAlignFactor;
      results->savedRegisters[reg].location = kRegisterInCFA;
      results->savedRegisters[reg].value = offset;
      break;
    case DW_CFA_def_cfa_sf:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      offset =
          addressSpace.getSLEB128(p, instructionsEnd) * cieInfo.dataAlignFactor;
      if (reg > kMaxRegisterNumber)
        return false;
      results->cfaRegister = reg;
      results->cfaRegisterOffset = offset;
      break;
    case DW_CFA_def_cfa_offset_sf:
      results->cfaRegisterOffset =
          addressSpace.getSLEB128(p, instructionsEnd) * cieInfo.dataAlignFactor;
      results->codeOffsetAtStackDecrement = codeOffset;
      break;
    case DW_CFA_val_offset:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      offset =
          addressSpace.getULEB128(p, instructionsEnd) * cieInfo.dataAlignFactor;
      if (reg > kMaxRegisterNumber)
        return false;
      results->savedRegisters[reg].location = kRegisterOffsetFromCFA;
      results->savedRegisters[reg].value = offset;
      break;
    case DW_CFA_val_offset_sf:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      if (reg > kMaxRegisterNumber)
        return false;
      offset =
          addressSpace.getSLEB128(p, instructionsEnd) * cieInfo.dataAlignFactor;
      results->savedRegisters[reg].location = kRegisterOffsetFromCFA;
      results->savedRegisters[reg].value = offset;
      break;
    case DW_CFA_val_expression:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      if (reg > kMaxRegisterNumber)
        return false;
      results->savedRegisters[reg].location = kRegisterIsExpression;
      results->savedRegisters[reg].value = p;
      length = addressSpace.getULEB128(p, instructionsEnd);
      p += length;
      break;
    case DW_CFA_GNU_window_save:
#if defined(__sparc__)
      for (reg = 8; reg < 16; ++reg) {
        results->savedRegisters[reg].location = kRegisterInRegister;
        results->savedRegisters[reg].value = reg + 16;
      }
      for (reg = 16; reg < 32; ++reg) {
        results->savedRegisters[reg].location = kRegisterInCFA;
        results->savedRegisters[reg].value = (reg - 16) * sizeof(typename R::reg_t);
      }
      break;
#else
      return false;
#endif
    case DW_CFA_GNU_args_size:
      offset = addressSpace.getULEB128(p, instructionsEnd);
      results->spExtraArgSize = offset;
      break;
    case DW_CFA_GNU_negative_offset_extended:
      reg = R::dwarf2regno(addressSpace.getULEB128(p, instructionsEnd));
      if (reg > kMaxRegisterNumber)
        return false;
      offset =
          addressSpace.getULEB128(p, instructionsEnd) * cieInfo.dataAlignFactor;
      results->savedRegisters[reg].location = kRegisterInCFA;
      results->savedRegisters[reg].value = -offset;
      break;
    default:
      operand = opcode & 0x3F;
      switch (opcode & 0xC0) {
      case DW_CFA_offset:
        reg = R::dwarf2regno(operand);
        if (reg > kMaxRegisterNumber)
          return false;
        offset = addressSpace.getULEB128(p, instructionsEnd) *
                 cieInfo.dataAlignFactor;
        results->savedRegisters[reg].location = kRegisterInCFA;
        results->savedRegisters[reg].value = offset;
        break;
      case DW_CFA_advance_loc:
        codeOffset += operand * cieInfo.codeAlignFactor;
        break;
      case DW_CFA_restore:
        reg = R::dwarf2regno(operand);
        if (reg > kMaxRegisterNumber)
          return false;
        results->savedRegisters[reg] = initialState.savedRegisters[reg];
        break;
      default:
        return false;
      }
    }
  }

  return true;
}

} // namespace _Unwind

#endif // __DWARF_PARSER_HPP__
