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

/* Environment strings passed by loader. */
PRIVATE char k_environ[128*sizeof(char *)];


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

  /* Record where the kernel and the monitor are. */
  code_base = seg2phys(cs);
  data_base = seg2phys(ds);

  /* Initialize protected mode descriptors. */
  prot_init();

  /* Copy the boot parameters to kernel memory. */
  mon_params = seg2phys(mds) + parmoff;
  mon_parmsize = MAX(parmsize,sizeof(k_environ));
  if (parmsize > sizeof k_environ - 2) parmsize = sizeof k_environ - 2;
  phys_copy(mon_params, vir2phys(k_environ), (phys_bytes) parmsize);

  /* Type of VDU: */
  envp = getkenv("video");
  if (kstrcmp(envp, "ega") == 0) ega = TRUE;
  if (kstrcmp(envp, "vga") == 0) vga = ega = TRUE;

  /* Processor? */
  processor = katoi(getkenv("processor"));	/* 86, 186, 286, 386, ... */

  /* XT, AT or MCA bus? */
  envp = getkenv("bus");
  if (envp == NIL_PTR || kstrcmp(envp, "at") == 0) {
	pc_at = TRUE;
  } else
  if (kstrcmp(envp, "mca") == 0) {
	pc_at = ps_mca = TRUE;
  }

  /* Decide if mode is protected. */
#if _WORD_SIZE == 2
  protected_mode = processor >= 286;
  if (!protected_mode) mon_return = 0;
#endif

  /* Return to assembler code to switch to protected mode (if 286), reload
   * selectors and call main().
   */
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
