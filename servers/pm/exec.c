/* This file handles the EXEC system call.  It performs the work as follows:
 *    - see if the permissions allow the file to be executed
 *    - read the header and extract the sizes
 *    - fetch the initial args and environment from the user space
 *    - allocate the memory for the new process
 *    - copy the initial stack from PM to the process
 *    - read in the text and data segments and copy to the process
 *    - take care of setuid and setgid bits
 *    - fix up 'mproc' table
 *    - tell kernel about EXEC
 *    - save offset to initial argc (for ps)
 *
 * The entry points into this file are:
 *   do_exec:	 perform the EXEC system call
 *   exec_newmem: allocate new memory map for a process that tries to exec
 *   do_execrestart: finish the special exec call for RS
 *   exec_restart: finish a regular exec call
 *   find_share: find a process whose text segment can be shared
 */

#include "pm.h"
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <a.out.h>
#include <signal.h>
#include <string.h>
#include "mproc.h"
#include "param.h"

FORWARD _PROTOTYPE( int new_mem, (struct mproc *rmp, struct mproc *sh_mp,
	vir_bytes text_bytes, vir_bytes data_bytes, vir_bytes bss_bytes,
	vir_bytes stk_bytes, phys_bytes tot_bytes)			);

#define ESCRIPT	(-2000)	/* Returned by read_header for a #! script. */
#define PTRSIZE	sizeof(char *) /* Size of pointers in argv[] and envp[]. */

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
PUBLIC int do_exec()
{
	int r;

	/* Save parameters */
	mp->mp_exec_path= m_in.exec_name;
	mp->mp_exec_path_len= m_in.exec_len;
	mp->mp_exec_frame= m_in.stack_ptr;
	mp->mp_exec_frame_len= m_in.stack_bytes;

	/* Forward call to FS */
	if (mp->mp_fs_call != PM_IDLE)
	{
		panic(__FILE__, "do_exec: not idle", mp->mp_fs_call);
	}
	mp->mp_fs_call= PM_EXEC;
	r= notify(FS_PROC_NR);
	if (r != OK)
		panic(__FILE__, "do_getset: unable to notify FS", r);

	/* Do not reply */
	return SUSPEND;
}


/*===========================================================================*
 *				exec_newmem				     *
 *===========================================================================*/
PUBLIC int exec_newmem()
{
	int r, proc_e, proc_n, allow_setuid;
	vir_bytes stack_top;
	vir_clicks tc, dc, sc, totc, dvir, s_vir;
	struct mproc *rmp, *sh_mp;
	char *ptr;
	struct exec_newmem args;

	if (who_e != FS_PROC_NR && who_e != RS_PROC_NR)
		return EPERM;

	proc_e= m_in.EXC_NM_PROC;
	if (pm_isokendpt(proc_e, &proc_n) != OK)
	{
		panic(__FILE__, "exec_newmem: got bad endpoint",
			proc_e);
	}
	rmp= &mproc[proc_n];

	ptr= m_in.EXC_NM_PTR;
	r= sys_datacopy(who_e, (vir_bytes)ptr,
		SELF, (vir_bytes)&args, sizeof(args));
	if (r != OK)
		panic(__FILE__, "exec_newmem: sys_datacopy failed", r);

	/* Check to see if segment sizes are feasible. */
	tc = ((unsigned long) args.text_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
	dc = (args.data_bytes+args.bss_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
	totc = (args.tot_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
	sc = (args.args_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
	if (dc >= totc) return(ENOEXEC); /* stack must be at least 1 click */

	dvir = (args.sep_id ? 0 : tc);
	s_vir = dvir + (totc - sc);
#if (CHIP == INTEL && _WORD_SIZE == 2)
	r = size_ok(*ft, tc, dc, sc, dvir, s_vir);
#else
	r = (dvir + dc > s_vir) ? ENOMEM : OK;
#endif
	if (r != OK)
		return r;

	/* Can the process' text be shared with that of one already running? */
	sh_mp = find_share(rmp, args.st_ino, args.st_dev, args.st_ctime);

	/* Allocate new memory and release old memory.  Fix map and tell
	 * kernel.
	 */
	r = new_mem(rmp, sh_mp, args.text_bytes, args.data_bytes,
		args.bss_bytes, args.args_bytes, args.tot_bytes);
	if (r != OK) return(r);

	rmp->mp_flags |= PARTIAL_EXEC;	/* Kill process if something goes
					 * wrong after this point.
					 */

	/* Save file identification to allow it to be shared. */
	rmp->mp_ino = args.st_ino;
	rmp->mp_dev = args.st_dev;
	rmp->mp_ctime = args.st_ctime;

	stack_top= ((vir_bytes)rmp->mp_seg[S].mem_vir << CLICK_SHIFT) +
		((vir_bytes)rmp->mp_seg[S].mem_len << CLICK_SHIFT);

	/* Save offset to initial argc (for ps) */
	rmp->mp_procargs = stack_top - args.args_bytes;

	/* set/clear separate I&D flag */
	if (args.sep_id)
		rmp->mp_flags |= SEPARATE;	
	else
		rmp->mp_flags &= ~SEPARATE;

	allow_setuid= 0;		/* Do not allow setuid execution */
	if ((rmp->mp_flags & TRACED) == 0) {
		/* Okay, setuid execution is allowed */
		allow_setuid= 1;
		rmp->mp_effuid = args.new_uid;
		rmp->mp_effgid = args.new_gid;
	}

	/* System will save command line for debugging, ps(1) output, etc. */
	strncpy(rmp->mp_name, args.progname, PROC_NAME_LEN-1);
	rmp->mp_name[PROC_NAME_LEN-1] = '\0';

	mp->mp_reply.reply_res2= stack_top;
	mp->mp_reply.reply_res3= 0;
	if (!sh_mp)			 /* Load text if sh_mp = NULL */
		mp->mp_reply.reply_res3 |= EXC_NM_RF_LOAD_TEXT;
	if (allow_setuid)
		mp->mp_reply.reply_res3 |= EXC_NM_RF_ALLOW_SETUID;

	return OK;
}


/*===========================================================================*
 *				do_execrestart				     *
 *===========================================================================*/
PUBLIC int do_execrestart()
{
	int proc_e, proc_n, result;
	struct mproc *rmp;

	if (who_e != RS_PROC_NR)
		return EPERM;

	proc_e= m_in.EXC_RS_PROC;
	if (pm_isokendpt(proc_e, &proc_n) != OK)
	{
		panic(__FILE__, "do_execrestart: got bad endpoint",
			proc_e);
	}
	rmp= &mproc[proc_n];
	result= m_in.EXC_RS_RESULT;

	exec_restart(rmp, result);

	return OK;
}


/*===========================================================================*
 *				exec_restart				     *
 *===========================================================================*/
PUBLIC void exec_restart(rmp, result)
struct mproc *rmp;
int result;
{
	int r, sn;
	vir_bytes pc;
	char *new_sp;

	if (result != OK)
	{
		if (rmp->mp_flags & PARTIAL_EXEC)
		{
			printf("partial exec; killing process\n");

			/* Use SIGILL signal that something went wrong */
			rmp->mp_sigstatus = SIGILL;
			pm_exit(rmp, 0, FALSE /*!for_trace*/);
			return;
		}
		setreply(rmp-mproc, result);
		return;
	}

	rmp->mp_flags &= ~PARTIAL_EXEC;

	/* Fix 'mproc' fields, tell kernel that exec is done, reset caught
	 * sigs.
	 */
	for (sn = 1; sn <= _NSIG; sn++) {
		if (sigismember(&rmp->mp_catch, sn)) {
			sigdelset(&rmp->mp_catch, sn);
			rmp->mp_sigact[sn].sa_handler = SIG_DFL;
			sigemptyset(&rmp->mp_sigact[sn].sa_mask);
		}
	}


	new_sp= (char *)rmp->mp_procargs;
	pc= 0;	/* for now */
	r= sys_exec(rmp->mp_endpoint, new_sp, rmp->mp_name, pc);
	if (r != OK) panic(__FILE__, "sys_exec failed", r);

	/* Cause a signal if this process is traced. */
	if (rmp->mp_flags & TRACED) check_sig(rmp->mp_pid, SIGTRAP);
}

/*===========================================================================*
 *				find_share				     *
 *===========================================================================*/
PUBLIC struct mproc *find_share(mp_ign, ino, dev, ctime)
struct mproc *mp_ign;		/* process that should not be looked at */
ino_t ino;			/* parameters that uniquely identify a file */
dev_t dev;
time_t ctime;
{
/* Look for a process that is the file <ino, dev, ctime> in execution.  Don't
 * accidentally "find" mp_ign, because it is the process on whose behalf this
 * call is made.
 */
  struct mproc *sh_mp;
  for (sh_mp = &mproc[0]; sh_mp < &mproc[NR_PROCS]; sh_mp++) {

	if (!(sh_mp->mp_flags & SEPARATE)) continue;
	if (sh_mp == mp_ign) continue;
	if (sh_mp->mp_ino != ino) continue;
	if (sh_mp->mp_dev != dev) continue;
	if (sh_mp->mp_ctime != ctime) continue;
	return sh_mp;
  }
  return(NULL);
}

/*===========================================================================*
 *				new_mem					     *
 *===========================================================================*/
PRIVATE int new_mem(rmp, sh_mp, text_bytes, data_bytes,
	bss_bytes,stk_bytes,tot_bytes)
struct mproc *rmp;		/* process to get a new memory map */
struct mproc *sh_mp;		/* text can be shared with this process */
vir_bytes text_bytes;		/* text segment size in bytes */
vir_bytes data_bytes;		/* size of initialized data in bytes */
vir_bytes bss_bytes;		/* size of bss in bytes */
vir_bytes stk_bytes;		/* size of initial stack segment in bytes */
phys_bytes tot_bytes;		/* total memory to allocate, including gap */
{
/* Allocate new memory and release the old memory.  Change the map and report
 * the new map to the kernel.  Zero the new core image's bss, gap and stack.
 */

  vir_clicks text_clicks, data_clicks, gap_clicks, stack_clicks, tot_clicks;
  phys_clicks new_base;
  phys_bytes bytes, base, bss_offset;
  int s, r2;

  /* No need to allocate text if it can be shared. */
  if (sh_mp != NULL) text_bytes = 0;

  /* Allow the old data to be swapped out to make room.  (Which is really a
   * waste of time, because we are going to throw it away anyway.)
   */
  rmp->mp_flags |= WAITING;

  /* Acquire the new memory.  Each of the 4 parts: text, (data+bss), gap,
   * and stack occupies an integral number of clicks, starting at click
   * boundary.  The data and bss parts are run together with no space.
   */
  text_clicks = ((unsigned long) text_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  data_clicks = (data_bytes + bss_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  stack_clicks = (stk_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  tot_clicks = (tot_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  gap_clicks = tot_clicks - data_clicks - stack_clicks;
  if ( (int) gap_clicks < 0) return(ENOMEM);

  /* Try to allocate memory for the new process. */
  new_base = alloc_mem(text_clicks + tot_clicks);
  if (new_base == NO_MEM) return(ENOMEM);

  /* We've got memory for the new core image.  Release the old one. */
  if (find_share(rmp, rmp->mp_ino, rmp->mp_dev, rmp->mp_ctime) == NULL) {
	/* No other process shares the text segment, so free it. */
	free_mem(rmp->mp_seg[T].mem_phys, rmp->mp_seg[T].mem_len);
  }
  /* Free the data and stack segments. */
  free_mem(rmp->mp_seg[D].mem_phys,
   rmp->mp_seg[S].mem_vir + rmp->mp_seg[S].mem_len - rmp->mp_seg[D].mem_vir);

  /* We have now passed the point of no return.  The old core image has been
   * forever lost, memory for a new core image has been allocated.  Set up
   * and report new map.
   */
  if (sh_mp != NULL) {
	/* Share the text segment. */
	rmp->mp_seg[T] = sh_mp->mp_seg[T];
  } else {
	rmp->mp_seg[T].mem_phys = new_base;
	rmp->mp_seg[T].mem_vir = 0;
	rmp->mp_seg[T].mem_len = text_clicks;

	if (text_clicks > 0)
	{
		/* Zero the last click of the text segment. Otherwise the
		 * part of that click may remain unchanged.
		 */
		base = (phys_bytes)(new_base+text_clicks-1) << CLICK_SHIFT;
		if ((s= sys_memset(0, base, CLICK_SIZE)) != OK)
			panic(__FILE__, "new_mem: sys_memset failed", s);
	}
  }
  rmp->mp_seg[D].mem_phys = new_base + text_clicks;
  rmp->mp_seg[D].mem_vir = 0;
  rmp->mp_seg[D].mem_len = data_clicks;
  rmp->mp_seg[S].mem_phys = rmp->mp_seg[D].mem_phys + data_clicks + gap_clicks;
  rmp->mp_seg[S].mem_vir = rmp->mp_seg[D].mem_vir + data_clicks + gap_clicks;
  rmp->mp_seg[S].mem_len = stack_clicks;

#if (CHIP == M68000)
  rmp->mp_seg[T].mem_vir = 0;
  rmp->mp_seg[D].mem_vir = rmp->mp_seg[T].mem_len;
  rmp->mp_seg[S].mem_vir = rmp->mp_seg[D].mem_vir 
  	+ rmp->mp_seg[D].mem_len + gap_clicks;
#endif

  if((r2=sys_newmap(rmp->mp_endpoint, rmp->mp_seg)) != OK) {
	/* report new map to the kernel */
	panic(__FILE__,"sys_newmap failed", r2);
  }

  /* The old memory may have been swapped out, but the new memory is real. */
  rmp->mp_flags &= ~(WAITING|ONSWAP|SWAPIN);

  /* Zero the bss, gap, and stack segment. */
  bytes = (phys_bytes)(data_clicks + gap_clicks + stack_clicks) << CLICK_SHIFT;
  base = (phys_bytes) rmp->mp_seg[D].mem_phys << CLICK_SHIFT;
  bss_offset = (data_bytes >> CLICK_SHIFT) << CLICK_SHIFT;
  base += bss_offset;
  bytes -= bss_offset;

  if ((s=sys_memset(0, base, bytes)) != OK) {
	panic(__FILE__,"new_mem can't zero", s);
  }

  return(OK);
}
