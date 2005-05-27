/* This file contains the main program of the process manager and some related
 * procedures.  When MINIX starts up, the kernel runs for a little while,
 * initializing itself and its tasks, and then it runs PM and FS.  Both PM
 * and FS initialize themselves as far as they can.  FS then makes a call to
 * PM, because PM has to wait for FS to acquire a RAM disk.  PM asks the
 * kernel for all free memory and starts serving requests.
 *
 * The entry points into this file are:
 *   main:	starts PM running
 *   setreply:	set the reply to be sent to process making an PM system call
 */

#include "pm.h"
#include <minix/utils.h>
#include <minix/keymap.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioc_memory.h>
#include <string.h>
#include "mproc.h"
#include "param.h"

#include "../../kernel/type.h"

FORWARD _PROTOTYPE( void get_work, (void)				);
FORWARD _PROTOTYPE( void pm_init, (void)				);

#define click_to_round_k(n) \
	((unsigned) ((((unsigned long) (n) << CLICK_SHIFT) + 512) / 1024))

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC void main()
{
/* Main routine of the process manager. */

  int result, s, proc_nr;
  struct mproc *rmp;

  pm_init();			/* initialize process manager tables */

  /* This is PM's main loop-  get work and do it, forever and forever. */
  while (TRUE) {
	get_work();		/* wait for an PM system call */

	/* Check for system notifications first. Special cases. */
	if (call_nr == HARD_STOP) {		/* MINIX is shutting down */
		check_sig(-1, SIGKILL);		/* kill all processes */
		sys_exit(0);
		/* never reached */
	} else if (call_nr == FKEY_PRESSED) {	/* create debug dump */
		(void) do_fkey_pressed();
		result = SUSPEND;		/* don't reply */
	} else if (call_nr == KSIG_PENDING) {	/* signals pending */
		(void) ksig_pending();
		result = SUSPEND;		/* don't reply */
	}
	/* Else, if the system call number is valid, perform the call. */
	else if ((unsigned) call_nr >= NCALLS) {
		result = ENOSYS;
	} else {
		result = (*call_vec[call_nr])();
	}

	/* Send the results back to the user to indicate completion. */
	if (result != SUSPEND) setreply(who, result);

	swap_in();		/* maybe a process can be swapped in? */

	/* Send out all pending reply messages, including the answer to
	 * the call just made above.  The processes must not be swapped out.
	 */
	for (proc_nr=0, rmp=mproc; proc_nr < NR_PROCS; proc_nr++, rmp++) {
		if ((rmp->mp_flags & IN_USE) &&
		    (rmp->mp_flags & (REPLY | ONSWAP)) == REPLY) {
			if ((s=send(proc_nr, &rmp->mp_reply)) != OK) {
				printf("Warning, PM send failed: %d, ", s);
				panic("PM can't reply to", proc_nr);
			}
			rmp->mp_flags &= ~REPLY;
		}
	}
  }
}


/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE void get_work()
{
/* Wait for the next message and extract useful information from it. */
  if (receive(ANY, &m_in) != OK) panic("PM receive error", NO_NUM);
  who = m_in.m_source;		/* who sent the message */
  call_nr = m_in.m_type;	/* system call number */

  /* Process slot of caller. Misuse PM's own process slot if the kernel is
   * calling. The can happen in case of pending kernel signals.
   */
  mp = &mproc[who < 0 ? PM_PROC_NR : who];
}


/*===========================================================================*
 *				setreply				     *
 *===========================================================================*/
PUBLIC void setreply(proc_nr, result)
int proc_nr;			/* process to reply to */
int result;			/* result of call (usually OK or error #) */
{
/* Fill in a reply message to be sent later to a user process.  System calls
 * may occasionally fill in other fields, this is only for the main return
 * value, and for setting the "must send reply" flag.
 */
  register struct mproc *rmp = &mproc[proc_nr];

  rmp->mp_reply.reply_res = result;
  rmp->mp_flags |= REPLY;	/* reply pending */

  if (rmp->mp_flags & ONSWAP)
	swap_inqueue(rmp);	/* must swap this process back in */
}


/*===========================================================================*
 *				pm_init					     *
 *===========================================================================*/
PRIVATE void pm_init()
{
/* Initialize the process manager. */
  int key, i, s;
  static struct system_image image[IMAGE_SIZE];
  register struct system_image *ip;
  static char core_sigs[] = { SIGQUIT, SIGILL, SIGTRAP, SIGABRT,
			SIGEMT, SIGFPE, SIGUSR1, SIGSEGV, SIGUSR2 };
  static char ign_sigs[] = { SIGCHLD };
  register int proc_nr;
  register struct mproc *rmp;
  register char *sig_ptr;
  phys_clicks ram_clicks, total_clicks, minix_clicks, free_clicks;
  message mess;
  struct mem_map kernel_map[NR_LOCAL_SEGS];
  int mem;

  /* Build the set of signals which cause core dumps, and the set of signals
   * that are by default ignored.
   */
  sigemptyset(&core_sset);
  for (sig_ptr = core_sigs; sig_ptr < core_sigs+sizeof(core_sigs); sig_ptr++)
	sigaddset(&core_sset, *sig_ptr);
  sigemptyset(&ign_sset);
  for (sig_ptr = ign_sigs; sig_ptr < ign_sigs+sizeof(ign_sigs); sig_ptr++)
	sigaddset(&ign_sset, *sig_ptr);

  /* Get the memory map of the kernel to see how much memory it uses. */
  if ((s=get_mem_map(SYSTASK, kernel_map)) != OK)
  	panic("PM couldn't get proc entry of SYSTASK",s);
  minix_clicks = (kernel_map[S].mem_phys + kernel_map[S].mem_len)
				- kernel_map[T].mem_phys;

  /* Initialize PM's tables. Request a copy of the system image table that
   * is defined at the kernel level to see which slots to fill in.
   */
  if (OK != (s=sys_getimage(&image))) {
  	printf("PM: warning, couldn't get system image table: %d\n", s);
  }
  procs_in_use = 0;				/* start populating table */
  for (ip = &image[0]; ip < &image[IMAGE_SIZE]; ip++) {		
  	if (ip->proc_nr >= 0) {			/* task have negative nrs */
  		procs_in_use += 1;		/* found user process */

		/* Set process details. */
		rmp = &mproc[ip->proc_nr];
		rmp->mp_flags |= IN_USE | DONT_SWAP; 
  		rmp->mp_pid = (ip->proc_nr == INIT_PROC_NR) ?
  			INIT_PID : get_free_pid();
  		strncpy(rmp->mp_name, ip->proc_name, PROC_NAME_LEN); 

  		/* Change signal handling behaviour. */
  		sigfillset(&rmp->mp_ignore);
  		sigfillset(&rmp->mp_sigmask);
  		sigemptyset(&rmp->mp_catch);

  		/* Get memory map for this process from the kernel. */
		if ((s=get_mem_map(ip->proc_nr, rmp->mp_seg)) != OK)
  			panic("couldn't get process entry",s);
		if (rmp->mp_seg[T].mem_len != 0) rmp->mp_flags |= SEPARATE;
		minix_clicks += rmp->mp_seg[S].mem_phys + 
			rmp->mp_seg[S].mem_len - rmp->mp_seg[T].mem_phys;

		/* Tell FS about this system process. */
		mess.PR_PROC_NR = ip->proc_nr;
		mess.PR_PID = rmp->mp_pid;
  		if (OK != (s=send(FS_PROC_NR, &mess)))
			panic("PM can't sync up with FS", s);
  	}
  }

  /* Tell FS no more SYSTEM processes follow and synchronize. */
  mess.PR_PROC_NR = NONE;
  if (sendrec(FS_PROC_NR, &mess) != OK || mess.m_type != OK)
	panic("PM can't sync up with FS", NO_NUM);

  /* INIT process is somewhat special. */
  sigemptyset(&mproc[INIT_PROC_NR].mp_ignore);
  sigemptyset(&mproc[INIT_PROC_NR].mp_sigmask);
  sigemptyset(&mproc[INIT_PROC_NR].mp_catch);

  /* Initialize tables to all physical memory. */
  mem_init(&free_clicks);
  total_clicks = minix_clicks + free_clicks;

  /* Print memory information. */
  printf("Memory size=%uK   ", click_to_round_k(total_clicks));
  printf("System services=%uK   ", click_to_round_k(minix_clicks));
  printf("Available=%uK\n\n", click_to_round_k(free_clicks));

  /* Register function keys with TTY for debug dumps. */
  for (key=SF7; key<=SF8; key++) {
  	if ((i=fkey_enable(key))!=OK) {
  		printf("Warning: PM couldn't register Shift+F%d key: %d\n",
  			key-SF1+1, i);
  	}
  }
}
