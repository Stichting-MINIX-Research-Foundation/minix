/*
 * fault-model.c -- fault injection code for drivers
 *
 * Copyright (C) 2003 Mike Swift
 * Copyright (c) 1999 Wee Teck Ng
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  No warranty
 * is attached; we cannot take responsibility for errors or
 * fitness for use.
 *
 */


/*
 * Fault injector for testing the usefulness of NOOKS
 *
 * Adapted from the SWIFI tools used by Wee Teck Ng to evaluate the RIO
 * file cache at the University of Michigan
 *
 */

/*
 * This tool can inject faults into modules, whether they are loaded into a
 * nook or loaded into the kernel (for comparison testing).
 *
 * There are several classes of faults emulated:
 * - Corruption of text
 *    - corruption
 *    - simulated programming faults
 *         - skip initialization (immediate write to EBP-x)
 *         - remove instruction (replace with NOP)
 *	   - incorrect source/destination (corrupted)
 *         - remove jmp or rep instruction
 *         - change address computation for memory access (not stack)
 *	   - change termination condition for loop (change repeat to repeat
 *           while equal, change condition to !condition)
 *         - remove instructions loading registers from arguments (ebp+x)
 */

#include <stdio.h>
#include <assert.h>

#include "ddb.h"
#include "db_sym.h"
#include "swifi.h"

#include "extra.h"

#define PDEBUG(args) /* (printf args) */

#define NOP		0x90

static int text_fault(int type, unsigned long btext, unsigned long text_size);

static int randomFaults[] = {
  TEXT_FAULT,
  NOP_FAULT,
  SRC_FAULT,
  DST_FAULT,
  PTR_FAULT,
  LOOP_FAULT,
  INTERFACE_FAULT
};

void
swifi_inject_fault(char * module_name,
		 unsigned long faultType,
		 unsigned long randomSeed,
		 unsigned long numFaults)
{
  unsigned long btext, etext, text_size;
  int type;

  /* default number of faults is 5 */
  if (numFaults == 0) numFaults = 5;

  srandom(randomSeed);

  load_nlist(module_name, &btext, &etext);

  text_size = etext - btext;

  PDEBUG(("text=%lx-%lx, size=%lx\n", btext, etext, text_size));

  while (numFaults) {
    if ((type = faultType) == RANDOM_FAULT)
      type = randomFaults[random() %
        (sizeof(randomFaults) / sizeof(randomFaults[0]))];

    if (text_fault(type, btext, text_size))
      numFaults--;
  }
}

static int text_fault(int type, unsigned long btext, unsigned long text_size)
{
  unsigned long *addr, taddr;
  int j, flip_bit, len, prefix;
  unsigned char *c;

  /* inject faults into text space */

  addr = (unsigned long *)
    (btext + ((unsigned long) (random()&~0xf) % text_size));

  /* now the tricky part */

  taddr=(unsigned long) addr;
  if (type != TEXT_FAULT) {
    addr = (unsigned long *) find_faulty_instr(taddr, type, &len);
    /* do it over again if we can't find the right instruction */
    if (!addr || !len)
      return FALSE;
  }

  PDEBUG(("target addr=%lx, instr addr=%p, %lx=>", taddr, addr,
    text_read_ul(addr)));

  switch (type) {
  case TEXT_FAULT:
    flip_bit = random() & 0x1f;
    PDEBUG(("flip bit %d => ", flip_bit));
    flip_bit = 1 << flip_bit;

    text_write_ul(addr, text_read_ul(addr)^flip_bit);

    break;

  case NOP_FAULT:
  case INIT_FAULT:
  case BRANCH_FAULT:
  case INTERFACE_FAULT:
  case IRQ_FAULT:
    c = (unsigned char *) addr;

    for (j = 0; j < len; j++) {
      /* replace these bytes with NOP (*c=NOP) */
      text_write_ub(c, NOP);

      c++;
    }

    break;

  case DST_FAULT:
  case SRC_FAULT:
    /* skip thru the prefix and opcode, and flip bits in following bytes */
    c=(unsigned char *) addr;
    do {
      switch (text_read_ub(c)) {
      case 0x66: case 0x67: case 0x26: case 0x36:
      case 0x2e: case 0x3e: case 0x64: case 0x65:
      case 0xf0: case 0xf2: case 0xf3:
        prefix = 1;
        break;
      default:
        prefix = 0;
        break;
      }
      if (prefix) {
        c++;
      }
    } while (prefix);
    if(text_read_ub(c)>=0xd8 && text_read_ub(c)<=0xdf) {
      /* don't mess with fp instruction, yet */
      PDEBUG(("floating point instruction, bailing out\n"));
      return FALSE;
    }
    if(text_read_ub(c)==0x0f) {
      c++;
    }
    if(text_read_ub(c)==0x0f) {
      c++;
    }
    c++;
    len = len-((long) c - (long) addr);
    if (len == 0)
    {
      PDEBUG(("text_fault: len = %d\n", len));
      return FALSE;
    }
    flip_bit = random() % (len*8);
    PDEBUG(("flip bit %d (len=%d) => ", flip_bit, len));
    for(j=0; j<len; j++) {
      /* go to the right byte */
      if(flip_bit<8) {
        flip_bit = 1 << flip_bit;

        text_write_ub(c, (text_read_ub(c)^flip_bit));

        j=len;
      }
      c++;
      flip_bit = flip_bit-8;
    }

    break;

  case PTR_FAULT:
    /* 5f) ptr: if instruction has regmodrm byte (i_has_modrm),
     *     flip 1 bit in lower byte (0x0f) or any bit in following
     *     bytes (sib, imm or disp).
     */
    c=(unsigned char *) addr;
    do {
      switch (text_read_ub(c)) {
      case 0x66: case 0x67: case 0x26: case 0x36:
      case 0x2e: case 0x3e: case 0x64: case 0x65:
      case 0xf0: case 0xf2: case 0xf3:
        prefix = 1;
        break;
      default:
        prefix = 0;
        break;
      }
      if (prefix) {
        c++;
      }
    } while (prefix);
    if(text_read_ub(c)>=0xd8 && text_read_ub(c)<=0xdf) {
      /* don't mess with fp instruction, yet */
      PDEBUG(("floating point instruction, bailing out\n"));
      return FALSE;
    }
    if(text_read_ub(c)==0x0f) {
      c++;
    }
    if(text_read_ub(c)==0x0f) {
      c++;
    }
    c++;
    len = len-((long) c - (long) addr);
    flip_bit = random() % (len*8-4);
    PDEBUG(("flip bit %d (len=%d) => ", flip_bit, len));

    /* mod/rm byte is special */

    if (flip_bit < 4) {
      flip_bit = 1 << flip_bit;

      text_write_ub(c, text_read_ub(c)^flip_bit);
    }
    c++;
    flip_bit=flip_bit-4;

    for(j=1; j<len; j++) {
      /* go to the right byte */
      if (flip_bit<8) {
        flip_bit = 1 << flip_bit;

        text_write_ub(c, text_read_ub(c)^flip_bit);

        j=len;
      }
      c++;
      flip_bit = flip_bit-8;
    }

    break;

  case LOOP_FAULT:
    c=(unsigned char *) addr;
    /* replace rep with repe, and vice versa */
    if(text_read_ub(c)==0xf3) {
      text_write_ub(c, 0xf2);
    } else if(text_read_ub(c)==0xf2) {
      text_write_ub(c, 0xf3);
    } else if( (text_read_ub(c)&0xf0)==0x70 ) {
      /* if we've jxx imm8 instruction,
       * incl even byte instruction, eg jo (70) to jno (71)
       * decl odd byte instruction,  eg jnle (7f) to jle (7e)
       */
      if(text_read_ub(c)%2 == 0) {
               text_write_ub(c, text_read_ub(c)+1);
      } else {
        text_write_ub(c, text_read_ub(c)-1);
      }
    } else if(text_read_ub(c)==0x66 || text_read_ub(c)==0x67) {
	    /* override prefix */
      c++;
    } else if(text_read_ub(c++)==0xf && (text_read_ub(c)&0xf0)==0x80 ) {
      /* if we've jxx imm16/32 instruction,
       * incl even byte instruction, eg jo (80) to jno (81)
       * decl odd byte instruction,  eg jnle (8f) to jle (8e)
       */
      if(text_read_ub(c)%2 == 0) {
        text_write_ub(c, text_read_ub(c)+1);
      } else {
        text_write_ub(c, text_read_ub(c)-1);
      }
    }

    break;

  case STOP_FAULT:
    text_write_ub(addr, BKPT_INST);

    break;

  default:
    assert(0);
  }

  return TRUE;
}
