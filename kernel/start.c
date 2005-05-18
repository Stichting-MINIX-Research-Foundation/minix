/* This file contains the C startup code for Minix on Intel processors.
 * It cooperates with mpx.s to set up a good environment for main().
 *
 * This code runs in real mode for a 16 bit kernel and may have to switch
 * to protected mode for a 286.
 *
 * For a 32 bit kernel this already runs in protected mode, but the selectors
 * are still those given by the BIOS with interrupts disabled, so the
 * descriptors need to be reloaded and interrupt descriptors made.
 */

#include "kernel.h"
#include "protect.h"
#include "proc.h"

/* Environment strings passed by loader. */
PRIVATE char k_environ[128*sizeof(char *)];

FORWARD _PROTOTYPE( void mem_init, (void) );

/*==========================================================================*
 *				cstart					    *
 *==========================================================================*/
PUBLIC void cstart(cs, ds, mds, parmoff, parmsize)
U16_t cs, ds;			/* kernel code and data segment */
U16_t mds;			/* monitor data segment */
U16_t parmoff, parmsize;	/* boot parameters offset and length */
{
/* Perform system initializations prior to calling main(). Most settings are
 * determined with help of the environment strings passed by MINIX' loader.
 */
  register char *envp;
  unsigned mon_start;
  extern int etext, end;

  /* Decide if mode is protected; 386 or higher implies protected mode.
   * This must be done first, because it is needed for, e.g., seg2phys().
   * For 286 machines we cannot decide on protected mode, yet. This is 
   * done below. 
   */
#if _WORD_SIZE != 2
  machine.protected = 1;	
#endif

  /* Record where the kernel and the monitor are. */
  kinfo.code_base = seg2phys(cs);
  kinfo.code_size = (phys_bytes) &etext;	/* size of code segment */
  kinfo.data_base = seg2phys(ds);
  kinfo.data_size = (phys_bytes) &end;		/* size of data segment */

  /* Initialize protected mode descriptors. */
  prot_init();

  /* Copy the boot parameters to kernel memory. */
  kinfo.params_base = seg2phys(mds) + parmoff;
  kinfo.params_size = MAX(parmsize,sizeof(k_environ)-2);
  phys_copy(kinfo.params_base, vir2phys(k_environ), kinfo.params_size);

  /* Record miscellaneous information for user-space servers. */
  kinfo.nr_procs = NR_PROCS;
  kinfo.nr_tasks = NR_TASKS;
  kstrncpy(kinfo.version, OS_RELEASE "." OS_VERSION, 6);
  kinfo.proc_addr = (vir_bytes) proc;
  kinfo.kmem_base = vir2phys(0);
  kinfo.kmem_size = (phys_bytes) &end;	

  /* Processor? */
  machine.processor=katoi(getkenv("processor"));  /* 86, 186, 286, 386, ... */

  /* Decide if mode is protected for older machines. */
#if _WORD_SIZE == 2
  machine.protected = machine.processor >= 286;		
#endif
  if (! machine.protected) mon_return = 0;

  /* XT, AT or MCA bus? */
  envp = getkenv("bus");
  if (envp == NIL_PTR || kstrcmp(envp, "at") == 0) {
      machine.pc_at = TRUE;			/* PC-AT compatible hardware */
  } else if (kstrcmp(envp, "mca") == 0) {
      machine.pc_at = machine.ps_mca = TRUE;	/* PS/2 with micro channel */
  }

  /* Type of VDU: */
  envp = getkenv("video");			/* EGA or VGA video unit */
  if (kstrcmp(envp, "ega") == 0) machine.vdu_ega = TRUE;
  if (kstrcmp(envp, "vga") == 0) machine.vdu_vga = machine.vdu_ega = TRUE;

  /* Initialize free memory list from size passed by boot monitor. */
  mem_init();

  /* Return to assembler code to switch to protected mode (if 286), 
   * reload selectors and call main().
   */
}



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
PRIVATE void mem_init()
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
	max_address = kinfo.protected ? MAX_16BIT : MAX_REAL;
	if (limit > max_address) limit = max_address;
#endif
	base = (base + CLICK_SIZE-1) & ~(long)(CLICK_SIZE-1);
	limit &= ~(long)(CLICK_SIZE-1);
	if (limit <= base) continue;
	memp->base = base >> CLICK_SHIFT;
	memp->size = (limit - base) >> CLICK_SHIFT;
  }
}


/*==========================================================================*
 *				getkenv					    *
 *==========================================================================*/
PUBLIC char *getkenv(name)
_CONST char *name;
{
/* Get environment value - kernel version of getenv to avoid setting up the
 * usual environment array.
 */
  register _CONST char *namep;
  register char *envp;

  for (envp = k_environ; *envp != 0;) {
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
		;
	if (*namep == '\0' && *envp == '=') return(envp + 1);
	while (*envp++ != 0)
		;
  }
  return(NIL_PTR);
}
