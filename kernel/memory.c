/* Entry points into this file:
 *   mem_init:		create a list a free memory 
 *   alloc_segments:	allocate segments for 8088 or higher processor
 */

#include "kernel.h"
#include "protect.h"
#include "proc.h"


#if (CHIP == INTEL)

/* In real mode only 1M can be addressed, and in 16-bit protected we can go
 * no further than we can count in clicks.  (The 286 is further limited by
 * its 24 bit address bus, but we can assume in that case that no more than
 * 16M memory is reported by the BIOS.)
 */
#define MAX_REAL	0x00100000L
#define MAX_16BIT	(0xFFF0L << CLICK_SHIFT)

/*=========================================================================*
 *				mem_init				   *
 *=========================================================================*/
PUBLIC void mem_init()
{
/* Initialize the free memory list from the 'memory' boot variable.  Translate
 * the byte offsets and sizes in this list to clicks, properly truncated. Also
 * make sure that we don't exceed the maximum address space of the 286 or the
 * 8086, i.e. when running in 16-bit protected mode or real mode.
 */
  long base, size, limit;
  char *s, *end;			/* use to parse boot variable */ 
  int i;
  struct memory *memp;
#if _WORD_SIZE == 2
  unsigned long max_address;
#endif

  /* The available memory is determined by MINIX' boot loader as a list of 
   * (base:size)-pairs in boothead.s. The 'memory' boot variable is set in
   * in boot.s.  The format is "b0:s0,b1:s1,b2:s2", where b0:s0 is low mem,
   * b1:s1 is mem between 1M and 16M, b2:s2 is mem above 16M. Pairs b1:s1 
   * and b2:s2 are combined if the memory is adjacent. 
   */
  s = getkenv("memory");		/* get memory boot variable */
  tot_mem_size = 0;
  for (i = 0; i < NR_MEMS; i++) {
	memp = &mem[i];			/* result is stored here */
	base = size = 0;
	if (*s != 0) {			/* end of boot variable */	
	    /* Expect base to be read (end != s) and ':' as next char. */ 
	    base = kstrtoul(s, &end, 0x10);		/* get number */
	    if (end != s && *end == ':') s = ++end;	/* skip ':' */ 
	    else *s=0;		/* fake end for next; should not happen */
	    /* Expect size to be read and skip ',', unless at end. */ 
	    size = kstrtoul(s, &end, 0x10);		/* get number */
	    if (end != s && *end == ',') s = ++end;	/* skip ',' */
	    else if (end != s && *end == 0) s = end;	/* end found */
	    else *s=0;		/* fake end for next; should not happen */
	}
	limit = base + size;
#if _WORD_SIZE == 2
	max_address = protected_mode ? MAX_16BIT : MAX_REAL;
	if (limit > max_address) limit = max_address;
#endif
	base = (base + CLICK_SIZE-1) & ~(long)(CLICK_SIZE-1);
	limit &= ~(long)(CLICK_SIZE-1);
	if (limit <= base) continue;
	memp->base = base >> CLICK_SHIFT;
	memp->size = (limit - base) >> CLICK_SHIFT;
	tot_mem_size += memp->size;
  }
}

/*==========================================================================*
 *				alloc_segments				    *
 *==========================================================================*/
PUBLIC void alloc_segments(rp)
register struct proc *rp;
{
/* This is called at system initialization from main() and by do_newmap(). 
 * The code has a separate function because of all hardware-dependencies.
 * Note that IDLE is part of the kernel and gets TASK_PRIVILEGE here.
 */
  phys_bytes code_bytes;
  phys_bytes data_bytes;
  int privilege;

  if (protected_mode) {
      data_bytes = (phys_bytes) (rp->p_memmap[S].mem_vir + 
          rp->p_memmap[S].mem_len) << CLICK_SHIFT;
      if (rp->p_memmap[T].mem_len == 0)
          code_bytes = data_bytes;	/* common I&D, poor protect */
      else
          code_bytes = (phys_bytes) rp->p_memmap[T].mem_len << CLICK_SHIFT;
      privilege = (isidlep(rp) || istaskp(rp)) ? 
          TASK_PRIVILEGE : USER_PRIVILEGE;
      init_codeseg(&rp->p_ldt[CS_LDT_INDEX],
          (phys_bytes) rp->p_memmap[T].mem_phys << CLICK_SHIFT,
          code_bytes, privilege);
      init_dataseg(&rp->p_ldt[DS_LDT_INDEX],
          (phys_bytes) rp->p_memmap[D].mem_phys << CLICK_SHIFT,
          data_bytes, privilege);
      rp->p_reg.cs = (CS_LDT_INDEX * DESC_SIZE) | TI | privilege;
#if _WORD_SIZE == 4
      rp->p_reg.gs =
      rp->p_reg.fs =
#endif
      rp->p_reg.ss =
      rp->p_reg.es =
      rp->p_reg.ds = (DS_LDT_INDEX*DESC_SIZE) | TI | privilege;
  } else {
      rp->p_reg.cs = click_to_hclick(rp->p_memmap[T].mem_phys);
      rp->p_reg.ss =
      rp->p_reg.es =
      rp->p_reg.ds = click_to_hclick(rp->p_memmap[D].mem_phys);
  }
}
#endif /* (CHIP == INTEL) */

