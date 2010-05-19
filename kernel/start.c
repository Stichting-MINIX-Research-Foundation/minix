
/* First C file used by the kernel. */

#include "kernel.h"
#include "proc.h"
#include <stdlib.h>
#include <string.h>
#include "proto.h"

#ifdef CONFIG_WATCHDOG
#include "watchdog.h"
#endif

/*===========================================================================*
 *				cstart					     *
 *===========================================================================*/
PUBLIC void cstart(
   u16_t cs,		/* kernel code segment */
   u16_t ds,		/* kernel data segment */
   u16_t mds,		/* monitor data segment */
   u16_t parmoff,	/* boot parameters offset */
   u16_t parmsize	/* boot parameters length */
)
{
/* Perform system initializations prior to calling main(). Most settings are
 * determined with help of the environment strings passed by MINIX' loader.
 */
  register char *value;				/* value in key=value pair */
  extern int etext, end;
  int h;

  /* Record where the kernel and the monitor are. */
  kinfo.code_base = seg2phys(cs);
  kinfo.code_size = (phys_bytes) &etext;	/* size of code segment */
  kinfo.data_base = seg2phys(ds);
  kinfo.data_size = (phys_bytes) &end;		/* size of data segment */

  /* protection initialization */
  prot_init();

  /* Copy the boot parameters to the local buffer. */
  arch_get_params(params_buffer, sizeof(params_buffer));

  /* determine verbosity */
  if ((value = env_get(VERBOSEBOOTVARNAME)))
	  verboseboot = atoi(value);

  DEBUGEXTRA(("cstart\n"));

  /* Record miscellaneous information for user-space servers. */
  kinfo.nr_procs = NR_PROCS;
  kinfo.nr_tasks = NR_TASKS;
  strncpy(kinfo.release, OS_RELEASE, sizeof(kinfo.release));
  kinfo.release[sizeof(kinfo.release)-1] = '\0';
  strncpy(kinfo.version, OS_VERSION, sizeof(kinfo.version));
  kinfo.version[sizeof(kinfo.version)-1] = '\0';
  kinfo.proc_addr = (vir_bytes) proc;

  /* Load average data initialization. */
  kloadinfo.proc_last_slot = 0;
  for(h = 0; h < _LOAD_HISTORY; h++)
	kloadinfo.proc_load_history[h] = 0;

  /* Processor? Decide if mode is protected for older machines. */
  machine.processor=atoi(env_get("processor")); 

  /* XT, AT or MCA bus? */
  value = env_get("bus");
  if (value == NULL || strcmp(value, "at") == 0) {
      machine.pc_at = TRUE;			/* PC-AT compatible hardware */
  } else if (strcmp(value, "mca") == 0) {
      machine.pc_at = machine.ps_mca = TRUE;	/* PS/2 with micro channel */
  }

  /* Type of VDU: */
  value = env_get("video");	/* EGA or VGA video unit */
  if (strcmp(value, "ega") == 0) machine.vdu_ega = TRUE;
  if (strcmp(value, "vga") == 0) machine.vdu_vga = machine.vdu_ega = TRUE;

  /* Get clock tick frequency. */
  value = env_get("hz");
  if(value)
	system_hz = atoi(value);
  if(!value || system_hz < 2 || system_hz > 50000)	/* sanity check */
	system_hz = DEFAULT_HZ;
  value = env_get(SERVARNAME);
  if(value && atoi(value) == 0)
	do_serial_debug=1;

#ifdef CONFIG_APIC
  value = env_get("no_apic");
  if(value)
	config_no_apic = atoi(value);
  else
	config_no_apic = 1;
#endif

#ifdef CONFIG_WATCHDOG
  value = env_get("watchdog");
  if (value)
	  watchdog_enabled = atoi(value);
#endif

  /* Return to assembler code to switch to protected mode (if 286), 
   * reload selectors and call main().
   */

  DEBUGEXTRA(("intr_init(%d, 0)\n", INTS_MINIX));
  intr_init(INTS_MINIX, 0);
}

/*===========================================================================*
 *				get_value				     *
 *===========================================================================*/

PRIVATE char *get_value(
  const char *params,			/* boot monitor parameters */
  const char *name			/* key to look up */
)
{
/* Get environment value - kernel version of getenv to avoid setting up the
 * usual environment array.
 */
  register const char *namep;
  register char *envp;

  for (envp = (char *) params; *envp != 0;) {
	for (namep = name; *namep != 0 && *namep == *envp; namep++, envp++)
		;
	if (*namep == '\0' && *envp == '=') return(envp + 1);
	while (*envp++ != 0)
		;
  }
  return(NULL);
}

/*===========================================================================*
 *				env_get				     	*
 *===========================================================================*/
PUBLIC char *env_get(const char *name)
{
	return get_value(params_buffer, name);
}

