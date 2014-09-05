/* Miscellaneous system calls.				Author: Kees J. Bot
 *								31 Mar 2000
 * The entry points into this file are:
 *   do_reboot: kill all processes, then reboot system
 *   do_getsysinfo: request copy of PM data structure  (Jorrit N. Herder)
 *   do_getprocnr: lookup endpoint by process ID
 *   do_getepinfo: get the pid/uid/gid of a process given its endpoint
 *   do_getsetpriority: get/set process priority
 *   do_svrctl: process manager control
 */

#include "pm.h"
#include <minix/callnr.h>
#include <signal.h>
#include <sys/svrctl.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/sysinfo.h>
#include <minix/type.h>
#include <minix/ds.h>
#include <machine/archtypes.h>
#include <lib.h>
#include <assert.h>
#include "mproc.h"
#include "kernel/proc.h"

struct utsname uts_val = {
  OS_NAME,		/* system name */
  "noname",		/* node/network name */
  OS_RELEASE,		/* O.S. release (e.g. 3.3.0) */
  OS_VERSION,		/* O.S. version (e.g. Minix 3.3.0 (GENERIC)) */
#if defined(__i386__)
  "i386",		/* machine (cpu) type */
  "i386",		/* architecture */
#elif defined(__arm__)
  "arm",		/* machine (cpu) type */
  "arm",		/* architecture */
#else
#error			/* oops, no 'uname -mk' */
#endif
};

static char *uts_tbl[] = {
  uts_val.arch,
  NULL,			/* No kernel architecture */
  uts_val.machine,
  NULL,			/* No hostname */
  uts_val.nodename,
  uts_val.release,
  uts_val.version,
  uts_val.sysname,
  NULL,			/* No bus */			/* No bus */
};

#if ENABLE_SYSCALL_STATS
unsigned long calls_stats[NR_PM_CALLS];
#endif

/*===========================================================================*
 *				do_sysuname				     *
 *===========================================================================*/
int do_sysuname()
{
/* Set or get uname strings. */
  int r;
  size_t n;
  char *string;
#if 0 /* for updates */
  char tmp[sizeof(uts_val.nodename)];
  static short sizes[] = {
	0,	/* arch, (0 = read-only) */
	0,	/* kernel */
	0,	/* machine */
	0,	/* sizeof(uts_val.hostname), */
	sizeof(uts_val.nodename),
	0,	/* release */
	0,	/* version */
	0,	/* sysname */
  };
#endif

  if (m_in.m_lc_pm_sysuname.field >= _UTS_MAX) return(EINVAL);

  string = uts_tbl[m_in.m_lc_pm_sysuname.field];
  if (string == NULL)
	return EINVAL;	/* Unsupported field */

  switch (m_in.m_lc_pm_sysuname.req) {
  case _UTS_GET:
	/* Copy an uname string to the user. */
	n = strlen(string) + 1;
	if (n > m_in.m_lc_pm_sysuname.len) n = m_in.m_lc_pm_sysuname.len;
	r = sys_datacopy(SELF, (vir_bytes)string, mp->mp_endpoint,
		m_in.m_lc_pm_sysuname.value, (phys_bytes)n);
	if (r < 0) return(r);
	break;

#if 0	/* no updates yet */
  case _UTS_SET:
	/* Set an uname string, needs root power. */
	len = sizes[m_in.m_lc_pm_sysuname.field];
	if (mp->mp_effuid != 0 || len == 0) return(EPERM);
	n = len < m_in.m_lc_pm_sysuname.len ? len : m_in.m_lc_pm_sysuname.len;
	if (n <= 0) return(EINVAL);
	r = sys_datacopy(mp->mp_endpoint, m_in.m_lc_pm_sysuname.value, SELF,
		(phys_bytes)tmp, (phys_bytes)n);
	if (r < 0) return(r);
	tmp[n-1] = 0;
	strcpy(string, tmp);
	break;
#endif

  default:
	return(EINVAL);
  }
  /* Return the number of bytes moved. */
  return(n);
}


/*===========================================================================*
 *				do_getsysinfo			       	     *
 *===========================================================================*/
int do_getsysinfo()
{
  vir_bytes src_addr, dst_addr;
  size_t len;

  /* This call leaks important information. In the future, requests from
   * non-system processes should be denied.
   */
  if (mp->mp_effuid != 0)
  {
	printf("PM: unauthorized call of do_getsysinfo by proc %d '%s'\n",
		mp->mp_endpoint, mp->mp_name);
	sys_diagctl_stacktrace(mp->mp_endpoint);
	return EPERM;
  }

  switch(m_in.m_lsys_getsysinfo.what) {
  case SI_PROC_TAB:			/* copy entire process table */
        src_addr = (vir_bytes) mproc;
        len = sizeof(struct mproc) * NR_PROCS;
        break;
#if ENABLE_SYSCALL_STATS
  case SI_CALL_STATS:
  	src_addr = (vir_bytes) calls_stats;
  	len = sizeof(calls_stats);
  	break; 
#endif
  default:
  	return(EINVAL);
  }

  if (len != m_in.m_lsys_getsysinfo.size)
	return(EINVAL);

  dst_addr = m_in.m_lsys_getsysinfo.where;
  return sys_datacopy(SELF, src_addr, who_e, dst_addr, len);
}

/*===========================================================================*
 *				do_getprocnr			             *
 *===========================================================================*/
int do_getprocnr(void)
{
  register struct mproc *rmp;

  /* This check should be replaced by per-call ACL checks. */
  if (who_e != RS_PROC_NR) {
	printf("PM: unauthorized call of do_getprocnr by %d\n", who_e);
	return EPERM;
  }

  if ((rmp = find_proc(m_in.m_lsys_pm_getprocnr.pid)) == NULL)
	return(ESRCH);

  mp->mp_reply.m_pm_lsys_getprocnr.endpt = rmp->mp_endpoint;
  return(OK);
}

/*===========================================================================*
 *				do_getepinfo			             *
 *===========================================================================*/
int do_getepinfo(void)
{
  struct mproc *rmp;
  endpoint_t ep;
  int slot;

  ep = m_in.m_lsys_pm_getepinfo.endpt;
  if (pm_isokendpt(ep, &slot) != OK)
	return(ESRCH);

  rmp = &mproc[slot];
  mp->mp_reply.m_pm_lsys_getepinfo.uid = rmp->mp_effuid;
  mp->mp_reply.m_pm_lsys_getepinfo.gid = rmp->mp_effgid;
  return(rmp->mp_pid);
}

/*===========================================================================*
 *				do_reboot				     *
 *===========================================================================*/
int do_reboot()
{
  message m;

  /* Check permission to abort the system. */
  if (mp->mp_effuid != SUPER_USER) return(EPERM);

  /* See how the system should be aborted. */
  abort_flag = m_in.m_lc_pm_reboot.how;

  /* notify readclock (some arm systems power off via RTC alarms) */
  if (abort_flag & RB_POWERDOWN) {
	endpoint_t readclock_ep;
	if (ds_retrieve_label_endpt("readclock.drv", &readclock_ep) == OK) {
		message m; /* no params to set, nothing we can do if it fails */
		_taskcall(readclock_ep, RTCDEV_PWR_OFF, &m);
	}
  }

  /* Order matters here. When VFS is told to reboot, it exits all its
   * processes, and then would be confused if they're exited again by
   * SIGKILL. So first kill, then reboot. 
   */

  check_sig(-1, SIGKILL, FALSE /* ksig*/); /* kill all users except init */
  sys_stop(INIT_PROC_NR);		   /* stop init, but keep it around */

  /* Tell VFS to reboot */
  memset(&m, 0, sizeof(m));
  m.m_type = VFS_PM_REBOOT;

  tell_vfs(&mproc[VFS_PROC_NR], &m);

  return(SUSPEND);			/* don't reply to caller */
}

/*===========================================================================*
 *				do_getsetpriority			     *
 *===========================================================================*/
int do_getsetpriority()
{
	int r, arg_which, arg_who, arg_pri;
	struct mproc *rmp;

	arg_which = m_in.m_lc_pm_priority.which;
	arg_who = m_in.m_lc_pm_priority.who;
	arg_pri = m_in.m_lc_pm_priority.prio;	/* for SETPRIORITY */

	/* Code common to GETPRIORITY and SETPRIORITY. */

	/* Only support PRIO_PROCESS for now. */
	if (arg_which != PRIO_PROCESS)
		return(EINVAL);

	if (arg_who == 0)
		rmp = mp;
	else
		if ((rmp = find_proc(arg_who)) == NULL)
			return(ESRCH);

	if (mp->mp_effuid != SUPER_USER &&
	   mp->mp_effuid != rmp->mp_effuid && mp->mp_effuid != rmp->mp_realuid)
		return EPERM;

	/* If GET, that's it. */
	if (call_nr == PM_GETPRIORITY) {
		return(rmp->mp_nice - PRIO_MIN);
	}

	/* Only root is allowed to reduce the nice level. */
	if (rmp->mp_nice > arg_pri && mp->mp_effuid != SUPER_USER)
		return(EACCES);
	
	/* We're SET, and it's allowed.
	 *
	 * The value passed in is currently between PRIO_MIN and PRIO_MAX.
	 * We have to scale this between MIN_USER_Q and MAX_USER_Q to match
	 * the kernel's scheduling queues.
	 */

	if ((r = sched_nice(rmp, arg_pri)) != OK) {
		return r;
	}

	rmp->mp_nice = arg_pri;
	return(OK);
}

/*===========================================================================*
 *				do_svrctl				     *
 *===========================================================================*/
int do_svrctl()
{
  int s, req;
  vir_bytes ptr;
#define MAX_LOCAL_PARAMS 2
  static struct {
  	char name[30];
  	char value[30];
  } local_param_overrides[MAX_LOCAL_PARAMS];
  static int local_params = 0;

  req = m_in.m_lsys_svrctl.request;
  ptr = m_in.m_lsys_svrctl.arg;

  /* Is the request indeed for the PM? */
  if (((req >> 8) & 0xFF) != 'M') return(EINVAL);

  /* Control operations local to the PM. */
  switch(req) {
  case PMSETPARAM:
  case PMGETPARAM: {
      struct sysgetenv sysgetenv;
      char search_key[64];
      char *val_start;
      size_t val_len;
      size_t copy_len;

      /* Copy sysgetenv structure to PM. */
      if (sys_datacopy(who_e, ptr, SELF, (vir_bytes) &sysgetenv, 
              sizeof(sysgetenv)) != OK) return(EFAULT);  

      /* Set a param override? */
      if (req == PMSETPARAM) {
  	if (local_params >= MAX_LOCAL_PARAMS) return ENOSPC;
  	if (sysgetenv.keylen <= 0
  	 || sysgetenv.keylen >=
  	 	 sizeof(local_param_overrides[local_params].name)
  	 || sysgetenv.vallen <= 0
  	 || sysgetenv.vallen >=
  	 	 sizeof(local_param_overrides[local_params].value))
  		return EINVAL;
  		
          if ((s = sys_datacopy(who_e, (vir_bytes) sysgetenv.key,
            SELF, (vir_bytes) local_param_overrides[local_params].name,
               sysgetenv.keylen)) != OK)
               	return s;
          if ((s = sys_datacopy(who_e, (vir_bytes) sysgetenv.val,
            SELF, (vir_bytes) local_param_overrides[local_params].value,
              sysgetenv.vallen)) != OK)
               	return s;
            local_param_overrides[local_params].name[sysgetenv.keylen] = '\0';
            local_param_overrides[local_params].value[sysgetenv.vallen] = '\0';

  	local_params++;

  	return OK;
      }

      if (sysgetenv.keylen == 0) {	/* copy all parameters */
          val_start = monitor_params;
          val_len = sizeof(monitor_params);
      } 
      else {				/* lookup value for key */
      	  int p;
          /* Try to get a copy of the requested key. */
          if (sysgetenv.keylen > sizeof(search_key)) return(EINVAL);
          if ((s = sys_datacopy(who_e, (vir_bytes) sysgetenv.key,
                  SELF, (vir_bytes) search_key, sysgetenv.keylen)) != OK)
              return(s);

          /* Make sure key is null-terminated and lookup value.
           * First check local overrides.
           */
          search_key[sysgetenv.keylen-1]= '\0';
          for(p = 0; p < local_params; p++) {
          	if (!strcmp(search_key, local_param_overrides[p].name)) {
          		val_start = local_param_overrides[p].value;
          		break;
          	}
          }
          if (p >= local_params && (val_start = find_param(search_key)) == NULL)
               return(ESRCH);
          val_len = strlen(val_start) + 1;
      }

      /* See if it fits in the client's buffer. */
      if (val_len > sysgetenv.vallen)
      	return E2BIG;

      /* Value found, make the actual copy (as far as possible). */
      copy_len = MIN(val_len, sysgetenv.vallen); 
      if ((s=sys_datacopy(SELF, (vir_bytes) val_start, 
              who_e, (vir_bytes) sysgetenv.val, copy_len)) != OK)
          return(s);

      return OK;
  }

  default:
	return(EINVAL);
  }
}

/*===========================================================================*
 *				do_getrusage				     *
 *===========================================================================*/
int do_getrusage()
{
	int res = 0;
	clock_t user_time = 0;
	clock_t sys_time = 0;
	struct rusage r_usage;
	u64_t usec;
	if (m_in.m_lc_pm_rusage.who != RUSAGE_SELF &&
		m_in.m_lc_pm_rusage.who != RUSAGE_CHILDREN)
		return EINVAL;
	if ((res = sys_getrusage(&r_usage, who_e)) < 0)
		return res;

	if (m_in.m_lc_pm_rusage.who == RUSAGE_CHILDREN) {
		usec = mp->mp_child_utime * 1000000 / sys_hz();
		r_usage.ru_utime.tv_sec = usec / 1000000;
		r_usage.ru_utime.tv_usec = usec % 1000000;
		usec = mp->mp_child_stime * 1000000 / sys_hz();
		r_usage.ru_stime.tv_sec = usec / 1000000;
		r_usage.ru_stime.tv_usec = usec % 1000000;
	}

	return sys_datacopy(SELF, (vir_bytes)&r_usage, who_e,
		m_in.m_lc_pm_rusage.addr, (vir_bytes) sizeof(r_usage));
}
