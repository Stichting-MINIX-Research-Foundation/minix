//===------------------------- AddressSpace.hpp ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//
// Abstracts accessing local vs remote address spaces.
//
//===----------------------------------------------------------------------===//

#ifndef __ADDRESSSPACE_HPP__
#define __ADDRESSSPACE_HPP__

#include <sys/rbtree.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#if !defined(__minix)
#include <pthread.h>
#else
#define pthread_rwlock_init(l, d) /* nothing */
#define pthread_rwlock_rdlock(l) /* nothing */
#define pthread_rwlock_wrlock(l) /* nothing */
#define pthread_rwlock_unlock(l) /* nothing */
#endif /* !defined(__minix) */

#include "dwarf2.h"

namespace _Unwind {

static int rangeCmp(void *, const void *, const void *);
static int rangeCmpKey(void *, const void *, const void *);
static int dsoTableCmp(void *, const void *, const void *);
static int dsoTableCmpKey(void *, const void *, const void *);
static int phdr_callback(struct dl_phdr_info *, size_t, void *);

struct unw_proc_info_t {
  uintptr_t data_base;       // Base address for data-relative relocations
  uintptr_t start_ip;        // Start address of function
  uintptr_t end_ip;          // First address after end of function
  uintptr_t lsda;            // Address of Language Specific Data Area
  uintptr_t handler;         // Personality routine
  uintptr_t extra_args;      // Extra stack space for frameless routines
  uintptr_t unwind_info;     // Address of DWARF unwind info
};

/// LocalAddressSpace is used as a template parameter to UnwindCursor when
/// unwinding a thread in the same process.  The wrappers compile away,
/// making local unwinds fast.
class LocalAddressSpace {
public:
  typedef uintptr_t pint_t;
  typedef intptr_t sint_t;

  typedef void (*findPCRange_t)(LocalAddressSpace &, pint_t, pint_t &pcStart,
                                pint_t &pcEnd);

  LocalAddressSpace(findPCRange_t findPCRange_)
      : findPCRange(findPCRange_), needsReload(true) {
    static const rb_tree_ops_t segmentTreeOps = {
      rangeCmp, rangeCmpKey, offsetof(Range, range_link), NULL
    };
    static const rb_tree_ops_t dsoTreeOps = {
      dsoTableCmp, dsoTableCmpKey, offsetof(Range, dso_link), NULL
    };
    rb_tree_init(&segmentTree, &segmentTreeOps);
    rb_tree_init(&dsoTree, &dsoTreeOps);
    pthread_rwlock_init(&fdeTreeLock, NULL);
  }

  uint8_t get8(pint_t addr) {
    uint8_t val;
    memcpy(&val, (void *)addr, sizeof(val));
    return val;
  }

  uint16_t get16(pint_t addr) {
    uint16_t val;
    memcpy(&val, (void *)addr, sizeof(val));
    return val;
  }

  uint32_t get32(pint_t addr) {
    uint32_t val;
    memcpy(&val, (void *)addr, sizeof(val));
    return val;
  }

  uint64_t get64(pint_t addr) {
    uint64_t val;
    memcpy(&val, (void *)addr, sizeof(val));
    return val;
  }

  uintptr_t getP(pint_t addr) {
    if (sizeof(uintptr_t) == sizeof(uint32_t))
      return get32(addr);
    else
      return get64(addr);
  }

  uint64_t getULEB128(pint_t &addr, pint_t end) {
    uint64_t result = 0;
    uint8_t byte;
    int bit = 0;
    do {
      uint64_t b;

      assert(addr != end);

      byte = get8(addr++);
      b = byte & 0x7f;

      assert(bit < 64);
      assert(b << bit >> bit == b);

      result |= b << bit;
      bit += 7;
    } while (byte >= 0x80);
    return result;
  }

  int64_t getSLEB128(pint_t &addr, pint_t end) {
    uint64_t result = 0;
    uint8_t byte;
    int bit = 0;
    do {
      uint64_t b;

      assert(addr != end);

      byte = get8(addr++);
      b = byte & 0x7f;

      assert(bit < 64);
      assert(b << bit >> bit == b);

      result |= b << bit;
      bit += 7;
    } while (byte >= 0x80);
    // sign extend negative numbers
    if ((byte & 0x40) != 0)
      result |= (-1LL) << bit;
    return result;
  }

  pint_t getEncodedP(pint_t &addr, pint_t end, uint8_t encoding,
                     const unw_proc_info_t *ctx) {
    pint_t startAddr = addr;
    const uint8_t *p = (uint8_t *)addr;
    pint_t result;

    if (encoding == DW_EH_PE_omit)
      return 0;
    if (encoding == DW_EH_PE_aligned) {
      addr = (addr + sizeof(pint_t) - 1) & sizeof(pint_t);
      return getP(addr);
    }

    // first get value
    switch (encoding & 0x0F) {
    case DW_EH_PE_ptr:
      result = getP(addr);
      p += sizeof(pint_t);
      addr = (pint_t)p;
      break;
    case DW_EH_PE_uleb128:
      result = getULEB128(addr, end);
      break;
    case DW_EH_PE_udata2:
      result = get16(addr);
      p += 2;
      addr = (pint_t)p;
      break;
    case DW_EH_PE_udata4:
      result = get32(addr);
      p += 4;
      addr = (pint_t)p;
      break;
    case DW_EH_PE_udata8:
      result = get64(addr);
      p += 8;
      addr = (pint_t)p;
      break;
    case DW_EH_PE_sleb128:
      result = getSLEB128(addr, end);
      break;
    case DW_EH_PE_sdata2:
      result = (int16_t)get16(addr);
      p += 2;
      addr = (pint_t)p;
      break;
    case DW_EH_PE_sdata4:
      result = (int32_t)get32(addr);
      p += 4;
      addr = (pint_t)p;
      break;
    case DW_EH_PE_sdata8:
      result = get64(addr);
      p += 8;
      addr = (pint_t)p;
      break;
    case DW_EH_PE_omit:
      result = 0;
      break;
    default:
      assert(0 && "unknown pointer encoding");
    }

    // then add relative offset
    switch (encoding & 0x70) {
    case DW_EH_PE_absptr:
      // do nothing
      break;
    case DW_EH_PE_pcrel:
      result += startAddr;
      break;
    case DW_EH_PE_textrel:
      assert(0 && "DW_EH_PE_textrel pointer encoding not supported");
      break;
    case DW_EH_PE_datarel:
      assert(ctx != NULL && "DW_EH_PE_datarel without context");
      if (ctx)
        result += ctx->data_base;
      break;
    case DW_EH_PE_funcrel:
      assert(ctx != NULL && "DW_EH_PE_funcrel without context");
      if (ctx)
        result += ctx->start_ip;
      break;
    case DW_EH_PE_aligned:
      __builtin_unreachable();
    default:
      assert(0 && "unknown pointer encoding");
      break;
    }

    if (encoding & DW_EH_PE_indirect)
      result = getP(result);

    return result;
  }

  bool findFDE(pint_t pc, pint_t &fdeStart, pint_t &data_base) {
    Range *n;
    for (;;) {
      pthread_rwlock_rdlock(&fdeTreeLock);
      n = (Range *)rb_tree_find_node(&segmentTree, &pc);
      pthread_rwlock_unlock(&fdeTreeLock);
      if (n != NULL)
        break;
      if (!needsReload)
        break;
      lazyReload();
    }
    if (n == NULL)
      return false;
    if (n->hdr_start == 0) {
      fdeStart = n->hdr_base;
      data_base = n->data_base;
      return true;
    }

    pint_t base = n->hdr_base;
    pint_t first = n->hdr_start;
    for (pint_t len = n->hdr_entries; len > 1; ) {
      pint_t next = first + (len / 2) * 8;
      pint_t nextPC = base + (int32_t)get32(next);
      if (nextPC == pc) {
        first = next;
        break;
      }
      if (nextPC < pc) {
        first = next;
        len -= (len / 2);
      } else {
        len /= 2;
      }
    }
    fdeStart = base + (int32_t)get32(first + 4);
    data_base = n->data_base;
    return true;
  }

  bool addFDE(pint_t pcStart, pint_t pcEnd, pint_t fde) {
    pthread_rwlock_wrlock(&fdeTreeLock);
    Range *n = (Range *)malloc(sizeof(*n));
    n->hdr_base = fde;
    n->hdr_start = 0;
    n->hdr_entries = 0;
    n->first_pc = pcStart;
    n->last_pc = pcEnd;
    n->data_base = 0;
    n->ehframe_base = 0;
    if (static_cast<Range *>(rb_tree_insert_node(&segmentTree, n)) == n) {
      pthread_rwlock_unlock(&fdeTreeLock);
      return true;
    }
    free(n);
    pthread_rwlock_unlock(&fdeTreeLock);
    return false;
  }

  bool removeFDE(pint_t pcStart, pint_t pcEnd, pint_t fde) {
    pthread_rwlock_wrlock(&fdeTreeLock);
    Range *n = static_cast<Range *>(rb_tree_find_node(&segmentTree, &pcStart));
    if (n == NULL) {
      pthread_rwlock_unlock(&fdeTreeLock);
      return false;
    }
    assert(n->first_pc == pcStart);
    assert(n->last_pc == pcEnd);
    assert(n->hdr_base == fde);
    assert(n->hdr_start == 0);
    assert(n->hdr_entries == 0);
    assert(n->data_base == 0);
    assert(n->ehframe_base == 0);
    rb_tree_remove_node(&segmentTree, n);
    free(n);
    pthread_rwlock_unlock(&fdeTreeLock);
    return true;
  }

  void removeDSO(pint_t ehFrameBase) {
    pthread_rwlock_wrlock(&fdeTreeLock);
    Range *n;
    n = (Range *)rb_tree_find_node(&dsoTree, &ehFrameBase);
    if (n == NULL) {
      pthread_rwlock_unlock(&fdeTreeLock);
      return;
    }
    rb_tree_remove_node(&dsoTree, n);
    rb_tree_remove_node(&segmentTree, n);
    free(n);
    pthread_rwlock_unlock(&fdeTreeLock);
  }

  void setLazyReload() {
    pthread_rwlock_wrlock(&fdeTreeLock);
    needsReload = true;
    pthread_rwlock_unlock(&fdeTreeLock);
  }

private:
  findPCRange_t findPCRange;
  bool needsReload;
#if !defined(__minix)
  pthread_rwlock_t fdeTreeLock;
#endif /* !defined(__minix) */
  rb_tree_t segmentTree;
  rb_tree_t dsoTree;

  friend int phdr_callback(struct dl_phdr_info *, size_t, void *);
  friend int rangeCmp(void *, const void *, const void *);
  friend int rangeCmpKey(void *, const void *, const void *);
  friend int dsoTableCmp(void *, const void *, const void *);
  friend int dsoTableCmpKey(void *, const void *, const void *);

  void updateRange();

  struct Range {
    rb_node_t range_link;
    rb_node_t dso_link;
    pint_t hdr_base; // Pointer to FDE if hdr_start == 0
    pint_t hdr_start;
    pint_t hdr_entries;
    pint_t first_pc;
    pint_t last_pc;
    pint_t data_base;
    pint_t ehframe_base;
  };

  void lazyReload() {
    pthread_rwlock_wrlock(&fdeTreeLock);
    dl_iterate_phdr(phdr_callback, this);
    needsReload = false;
    pthread_rwlock_unlock(&fdeTreeLock);
  }

  void addDSO(pint_t header, pint_t data_base) {
    if (header == 0)
      return;
    if (get8(header) != 1)
      return;
    if (get8(header + 3) != (DW_EH_PE_datarel | DW_EH_PE_sdata4))
      return;
    pint_t end = header + 4;
    pint_t ehframe_base = getEncodedP(end, 0, get8(header + 1), NULL);
    pint_t entries = getEncodedP(end, 0, get8(header + 2), NULL);
    pint_t start = (end + 3) & ~pint_t(3);
    if (entries == 0)
      return;
    Range *n = (Range *)malloc(sizeof(*n));
    n->hdr_base = header;
    n->hdr_start = start;
    n->hdr_entries = entries;
    n->first_pc = header + (int32_t)get32(n->hdr_start);
    pint_t tmp;
    (*findPCRange)(
        *this, header + (int32_t)get32(n->hdr_start + (entries - 1) * 8 + 4),
        tmp, n->last_pc);
    n->data_base = data_base;
    n->ehframe_base = ehframe_base;

    if (static_cast<Range *>(rb_tree_insert_node(&segmentTree, n)) != n) {
      free(n);
      return;
    }
    rb_tree_insert_node(&dsoTree, n);
  }
};

static int phdr_callback(struct dl_phdr_info *info, size_t size, void *data_) {
  LocalAddressSpace *data = (LocalAddressSpace *)data_;
  size_t eh_frame = 0, data_base = 0;
  const Elf_Phdr *hdr = info->dlpi_phdr;
  const Elf_Phdr *last_hdr = hdr + info->dlpi_phnum;
  const Elf_Dyn *dyn;

  for (; hdr != last_hdr; ++hdr) {
    switch (hdr->p_type) {
    case PT_GNU_EH_FRAME:
      eh_frame = info->dlpi_addr + hdr->p_vaddr;
      break;
    case PT_DYNAMIC:
      dyn = (const Elf_Dyn *)(info->dlpi_addr + hdr->p_vaddr);
      while (dyn->d_tag != DT_NULL) {
        if (dyn->d_tag == DT_PLTGOT) {
          data_base = info->dlpi_addr + dyn->d_un.d_ptr;
          break;
        }
        ++dyn;
      }
    }
  }

  if (eh_frame)
    data->addDSO(eh_frame, data_base);

  return 0;
}

static int rangeCmp(void *context, const void *n1_, const void *n2_) {
  const LocalAddressSpace::Range *n1 = (const LocalAddressSpace::Range *)n1_;
  const LocalAddressSpace::Range *n2 = (const LocalAddressSpace::Range *)n2_;

  if (n1->first_pc < n2->first_pc)
    return -1;
  if (n1->first_pc > n2->first_pc)
    return 1;
  assert(n1->last_pc == n2->last_pc);
  return 0;
}

static int rangeCmpKey(void *context, const void *n_, const void *pc_) {
  const LocalAddressSpace::Range *n = (const LocalAddressSpace::Range *)n_;
  const LocalAddressSpace::pint_t *pc = (const LocalAddressSpace::pint_t *)pc_;
  if (n->last_pc < *pc)
    return -1;
  if (n->first_pc > *pc)
    return 1;
  return 0;
}

static int dsoTableCmp(void *context, const void *n1_, const void *n2_) {
  const LocalAddressSpace::Range *n1 = (const LocalAddressSpace::Range *)n1_;
  const LocalAddressSpace::Range *n2 = (const LocalAddressSpace::Range *)n2_;

  if (n1->ehframe_base < n2->ehframe_base)
    return -1;
  if (n1->ehframe_base > n2->ehframe_base)
    return 1;
  return 0;
}

static int dsoTableCmpKey(void *context, const void *n_, const void *ptr_) {
  const LocalAddressSpace::Range *n = (const LocalAddressSpace::Range *)n_;
  const LocalAddressSpace::pint_t *ptr = (const LocalAddressSpace::pint_t *)ptr_;
  if (n->ehframe_base < *ptr)
    return -1;
  if (n->ehframe_base > *ptr)
    return 1;
  return 0;
}

} // namespace _Unwind

#endif // __ADDRESSSPACE_HPP__
