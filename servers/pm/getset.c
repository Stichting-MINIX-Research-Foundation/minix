/* This file handles the 4 system calls that get and set uids and gids.
 * It also handles getpid(), setsid(), and getpgrp().  The code for each
 * one is so tiny that it hardly seemed worthwhile to make each a separate
 * function.
 */

#include "pm.h"
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <signal.h>
#include "mproc.h"
#include "param.h"

/*===========================================================================*
 *				do_getset				     *
 *===========================================================================*/
PUBLIC int do_getset()
{
/* Handle GETUID, GETGID, GETPID, GETPGRP, SETUID, SETGID, SETSID.  The four
 * GETs and SETSID return their primary results in 'r'.  GETUID, GETGID, and
 * GETPID also return secondary results (the effective IDs, or the parent
 * process ID) in 'reply_res2', which is returned to the user.
 */

  register struct mproc *rmp = mp;
  int r, proc;

  switch(call_nr) {
	case GETUID:
		r = rmp->mp_realuid;
		rmp->mp_reply.reply_res2 = rmp->mp_effuid;
		break;

	case GETGID:
		r = rmp->mp_realgid;
		rmp->mp_reply.reply_res2 = rmp->mp_effgid;
		break;

	case GETPID:
		r = mproc[who_p].mp_pid;
		rmp->mp_reply.reply_res2 = mproc[rmp->mp_parent].mp_pid;
		if(pm_isokendpt(m_in.endpt, &proc) == OK && proc >= 0)
			rmp->mp_reply.reply_res3 = mproc[proc].mp_pid;
		break;

	case SETEUID:
	case SETUID:
		if (rmp->mp_realuid != (uid_t) m_in.usr_id && 
				rmp->mp_effuid != SUPER_USER)
			return(EPERM);
		if(call_nr == SETUID) rmp->mp_realuid = (uid_t) m_in.usr_id;
		rmp->mp_effuid = (uid_t) m_in.usr_id;

		if (rmp->mp_fs_call != PM_IDLE)
		{
			panic(__FILE__, "do_getset: not idle",
				rmp->mp_fs_call);
		}
		rmp->mp_fs_call= PM_SETUID;
		r= notify(FS_PROC_NR);
		if (r != OK)
			panic(__FILE__, "do_getset: unable to notify FS", r);
		
		/* Do not reply until FS is ready to process the setuid
		 * request
		 */
		r= SUSPEND;
		break;

	case SETEGID:
	case SETGID:
		if (rmp->mp_realgid != (gid_t) m_in.grp_id && 
				rmp->mp_effuid != SUPER_USER)
			return(EPERM);
		if(call_nr == SETGID) rmp->mp_realgid = (gid_t) m_in.grp_id;
		rmp->mp_effgid = (gid_t) m_in.grp_id;

		if (rmp->mp_fs_call != PM_IDLE)
		{
			panic(__FILE__, "do_getset: not idle",
				rmp->mp_fs_call);
		}
		rmp->mp_fs_call= PM_SETGID;
		r= notify(FS_PROC_NR);
		if (r != OK)
			panic(__FILE__, "do_getset: unable to notify FS", r);

		/* Do not reply until FS is ready to process the setgid
		 * request
		 */
		r= SUSPEND;
		break;

	case SETSID:
		if (rmp->mp_procgrp == rmp->mp_pid) return(EPERM);
		rmp->mp_procgrp = rmp->mp_pid;

		if (rmp->mp_fs_call != PM_IDLE)
		{
			panic(__FILE__, "do_getset: not idle",
				rmp->mp_fs_call);
		}
		rmp->mp_fs_call= PM_SETSID;
		r= notify(FS_PROC_NR);
		if (r != OK)
			panic(__FILE__, "do_getset: unable to notify FS", r);

		/* Do not reply until FS is ready to process the setsid
		 * request
		 */
		r= SUSPEND;
		break;

	case GETPGRP:
		r = rmp->mp_procgrp;
		break;

	default:
		r = EINVAL;
		break;	
  }
  return(r);
}
