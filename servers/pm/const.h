/* Constants used by the Process Manager. */

#define NO_MEM ((phys_clicks) 0)  /* returned by alloc_mem() with mem is up */

#if (CHIP == INTEL && _WORD_SIZE == 2)
/* These definitions are used in size_ok and are not needed for 386.
 * The 386 segment granularity is 1 for segments smaller than 1M and 4096
 * above that.  
 */
#define PAGE_SIZE	  16	/* how many bytes in a page (s.b.HCLICK_SIZE)*/
#define MAX_PAGES       4096	/* how many pages in the virtual addr space */
#endif

#define NR_PIDS	       30000	/* process ids range from 0 to NR_PIDS-1.
				 * (magic constant: some old applications use
				 * a 'short' instead of pid_t.)
				 */

#define PM_PID	           0	/* PM's process id number */
#define INIT_PID	   1	/* INIT's process id number */

