/* This file contains the C startup code for Minix on Intel processors.
 * It cooperates with mpx.s to set up a good environment for main().
 *
 * This code runs in real mode for a 16 bit kernel and may have to switch
 * to protected mode for a 286.
 * For a 32 bit kernel this already runs in protected mode, but the selectors
 * are still those given by the BIOS with interrupts disabled, so the
 * descriptors need to be reloaded and interrupt descriptors made.
 */

#include "kernel.h"
#include "protect.h"
#include "proc.h"

FORWARD _PROTOTYPE( void mem_init, (_CONST char *params));
FORWARD _PROTOTYPE( char *get_value, (_CONST char *params, _CONST char *key));

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
  char params[128*sizeof(char *)];		/* boot monitor parameters */
  register char *value;				/* value in key=value pair */
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
  kinfo.params_size = MAX(parmsize,sizeof(params)-2);
  phys_copy(kinfo.params_base, vir2phys(params), kinfo.params_size);

  /* Record miscellaneous information for user-space servers. */
  kinfo.nr_procs = NR_PROCS;
  kinfo.nr_tasks = NR_TASKS;
  kstrncpy(kinfo.version, OS_RELEASE "." OS_VERSION, 6);
  kinfo.proc_addr = (vir_bytes) proc;
  kinfo.kmem_base = vir2phys(0);
  kinfo.kmem_size = (phys_bytes) &end;	

  /* Processor?  86, 186, 286, 386, ... */
  machine.processor=katoi(get_value(params, "processor")); 

  /* Decide if mode is protected for older machines. */
#if _WORD_SIZE == 2
  machine.protected = machine.processor >= 286;		
#endif
  if (! machine.protected) mon_return = 0;

  /* XT, AT or MCA bus? */
  value = get_value(params, "bus");
  if (value == NIL_PTR || kstrcmp(value, "at") == 0) {
      machine.pc_at = TRUE;			/* PC-AT compatible hardware */
  } else if (kstrcmp(value, "mca") == 0) {
      machine.pc_at = machine.ps_mca = TRUE;	/* PS/2 with micro channel */
  }

  /* Type of VDU: */
  value = get_value(params, "video");			/* EGA or VGA video unit */
  if (kstrcmp(value, "ega") == 0) machine.vdu_ega = TRUE;
  if (kstrcmp(value, "vga") == 0) machine.vdu_vga = machine.vdu_ega = TRUE;

  /* Initialize free memory list from size passed by boot monitor. */
  mem_init(params);

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
PRIVATE void mem_init(params)
_CONST char *params;				/* boot monitor parameters */
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
  s = get_value(params, "memory");	/* get memory boot variable */
  for (i = 0; i < NR_MEMS; i++) {
	memp = &mem[i];			/* next mem chunk is stored here */
	base = size = 0;		/* initialize next base:size pair */
	if (*s != 0) {			/* get fresh data, unless at end */	

	    /* Read fresh base and expect colon as next char. */ 
	    base = kstrtoul(s, &end, 0x10);		/* get number */
	    if (end != s && *end == ':') s = ++end;	/* skip ':' */ 
	    else *s=0;			/* terminate, should not happen */

	    /* Read fresh size and expect comma or assume end. */ 
	    size = kstrtoul(s, &end, 0x10);		/* get number */
	    if (end != s && *end == ',') s = ++end;	/* skip ',' */
	    else *s=0;					/* found end */
	}
	limit = base + size;	/* limit is used for validity check */	 
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
 *				get_value					    *
 *==========================================================================*/
PRIVATE char *get_value(params, name)
_CONST char *params;				/* boot monitor parameters */
_CONST char *name;				/* key to look up */
{
/* Get environment value - kernel version of getenv to avoid setting up the
 * usual environment array.
 */
  register _CONST char *namep;
  register char *envp;

  for (envp = (char *) params; *envp != 0;) {
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
		;
	if (*namep == '\0' && *envp == '=') return(envp + 1);
	while (*envp++ != 0)
		;
  }
  return(NIL_PTR);
}
