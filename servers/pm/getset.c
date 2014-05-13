/* This file handles the 6 system calls that get and set uids and gids.
 * It also handles getpid(), setsid(), and getpgrp().  The code for each
 * one is so tiny that it hardly seemed worthwhile to make each a separate
 * function.
 */

#include "pm.h"
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <limits.h>
#include <minix/com.h>
#include <signal.h>
#include "mproc.h"

/*===========================================================================*
 *				do_get					     *
 *===========================================================================*/
int do_get()
{
/* Handle PM_GETUID, PM_GETGID, PM_GETGROUPS, PM_GETPID, PM_GETPGRP, PM_GETSID,
 * PM_ISSETUGID.
 */
  register struct mproc *rmp = mp;
  int r;
  int ngroups;

  switch(call_nr) {
	case PM_GETGROUPS:
		ngroups = m_in.m_lc_pm_groups.num;
		if (ngroups > NGROUPS_MAX || ngroups < 0)
			return(EINVAL);

		if (ngroups == 0) {
			r = rmp->mp_ngroups;
			break;
		}

		if (ngroups < rmp->mp_ngroups)
			/* Asking for less groups than available */
			return(EINVAL);

		r = sys_datacopy(SELF, (vir_bytes) rmp->mp_sgroups, who_e,
			m_in.m_lc_pm_groups.ptr, ngroups * sizeof(gid_t));

		if (r != OK)
			return(r);

		r = rmp->mp_ngroups;
		break;
	case PM_GETUID:
		r = rmp->mp_realuid;
		rmp->mp_reply.m_pm_lc_getuid.euid = rmp->mp_effuid;
		break;

	case PM_GETGID:
		r = rmp->mp_realgid;
		rmp->mp_reply.m_pm_lc_getgid.egid = rmp->mp_effgid;
		break;

	case PM_GETPID:
		r = mproc[who_p].mp_pid;
		rmp->mp_reply.m_pm_lc_getpid.parent_pid = mproc[rmp->mp_parent].mp_pid;
		break;

	case PM_GETPGRP:
		r = rmp->mp_procgrp;
		break;

	case PM_GETSID:
	{
		struct mproc *target;
		pid_t p = m_in.m_lc_pm_getsid.pid;
		target = p ? find_proc(p) : &mproc[who_p];
		r = ESRCH;
		if(target)
			r = target->mp_procgrp;
		break;
	}
	case PM_ISSETUGID:
		r = !!(rmp->mp_flags & TAINTED);
		break;

	default:
		r = EINVAL;
		break;	
  }
  return(r);
}

/*===========================================================================*
 *				do_set					     *
 *===========================================================================*/
int do_set()
{
/* Handle PM_SETUID, PM_SETEUID, PM_SETGID, PM_SETGROUPS, PM_SETEGID, and
 * SETSID. These calls have in common that, if successful, they will be
 * forwarded to VFS as well.
 */
  register struct mproc *rmp = mp;
  message m;
  int r, i;
  int ngroups;
  uid_t uid;
  gid_t gid;

  memset(&m, 0, sizeof(m));

  switch(call_nr) {
	case PM_SETUID:
	case PM_SETEUID:
		uid = m_in.m_lc_pm_setuid.uid;
		if (rmp->mp_realuid != uid && rmp->mp_effuid != SUPER_USER)
			return(EPERM);
		if(call_nr == PM_SETUID) rmp->mp_realuid = uid;
		rmp->mp_effuid = uid;

		m.m_type = VFS_PM_SETUID;
		m.VFS_PM_ENDPT = rmp->mp_endpoint;
		m.VFS_PM_EID = rmp->mp_effuid;
		m.VFS_PM_RID = rmp->mp_realuid;

		break;

	case PM_SETGID:
	case PM_SETEGID:
		gid = m_in.m_lc_pm_setgid.gid;
		if (rmp->mp_realgid != gid && rmp->mp_effuid != SUPER_USER)
			return(EPERM);
		if(call_nr == PM_SETGID) rmp->mp_realgid = gid;
		rmp->mp_effgid = gid;

		m.m_type = VFS_PM_SETGID;
		m.VFS_PM_ENDPT = rmp->mp_endpoint;
		m.VFS_PM_EID = rmp->mp_effgid;
		m.VFS_PM_RID = rmp->mp_realgid;

		break;
	case PM_SETGROUPS:
		if (rmp->mp_effuid != SUPER_USER)
			return(EPERM);

		ngroups = m_in.m_lc_pm_groups.num;

		if (ngroups > NGROUPS_MAX || ngroups < 0) 
			return(EINVAL);

		if (ngroups > 0 && m_in.m_lc_pm_groups.ptr == 0)
			return(EFAULT);

		r = sys_datacopy(who_e, m_in.m_lc_pm_groups.ptr, SELF,
			     (vir_bytes) rmp->mp_sgroups,
			     ngroups * sizeof(gid_t));
		if (r != OK) 
			return(r);

		for (i = 0; i < ngroups; i++) {
			if (rmp->mp_sgroups[i] > GID_MAX)
				return(EINVAL);
		}
		for (i = ngroups; i < NGROUPS_MAX; i++) {
			rmp->mp_sgroups[i] = 0;
		}
		rmp->mp_ngroups = ngroups;

		m.m_type = VFS_PM_SETGROUPS;
		m.VFS_PM_ENDPT = rmp->mp_endpoint;
		m.VFS_PM_GROUP_NO = rmp->mp_ngroups;
		m.VFS_PM_GROUP_ADDR = (char *) rmp->mp_sgroups;

		break;
	case PM_SETSID:
		if (rmp->mp_procgrp == rmp->mp_pid) return(EPERM);
		rmp->mp_procgrp = rmp->mp_pid;

		m.m_type = VFS_PM_SETSID;
		m.VFS_PM_ENDPT = rmp->mp_endpoint;

		break;

	default:
		return(EINVAL);
  }

  /* Send the request to VFS */
  tell_vfs(rmp, &m);

  /* Do not reply until VFS has processed the request */
  return(SUSPEND);
}
