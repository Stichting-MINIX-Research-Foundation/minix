/* Miscellaneous system calls.				Author: Kees J. Bot
 *								31 Mar 2000
 * The entry points into this file are:
 *   do_reboot: kill all processes, then reboot system
 *   do_getsysinfo: request copy of PM data structure  (Jorrit N. Herder)
 *   do_getprocnr: lookup process slot number  (Jorrit N. Herder)
 *   do_getepinfo: get the pid/uid/gid of a process given its endpoint
 *   do_getsetpriority: get/set process priority
 *   do_svrctl: process manager control
 */

#define brk _brk

#include "pm.h"
#include <minix/callnr.h>
#include <signal.h>
#include <sys/svrctl.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/reboot.h>
#include <minix/sysinfo.h>
#include <minix/type.h>
#include <minix/vm.h>
#include <string.h>
#include <machine/archtypes.h>
#include <lib.h>
#include <assert.h>
#include "mproc.h"
#include "param.h"
#include "kernel/proc.h"

struct utsname uts_val = {
  "Minix",		/* system name */
  "noname",		/* node/network name */
  OS_RELEASE,		/* O.S. release (e.g. 1.5) */
  OS_VERSION,		/* O.S. version (e.g. 10) */
  "xyzzy",		/* machine (cpu) type (filled in later) */
#if defined(__i386__)
  "i386",		/* architecture */
#elif defined(__arm__)
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
unsigned long calls_stats[NCALLS];
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

  if ((unsigned) m_in.sysuname_field >= _UTS_MAX) return(EINVAL);

  string = uts_tbl[m_in.sysuname_field];
  if (string == NULL)
	return EINVAL;	/* Unsupported field */

  switch (m_in.sysuname_req) {
  case _UTS_GET:
	/* Copy an uname string to the user. */
	n = strlen(string) + 1;
	if (n > m_in.sysuname_len) n = m_in.sysuname_len;
	r = sys_vircopy(SELF, (phys_bytes) string, 
		mp->mp_endpoint, (phys_bytes) m_in.sysuname_value,
		(phys_bytes) n);
	if (r < 0) return(r);
	break;

#if 0	/* no updates yet */
  case _UTS_SET:
	/* Set an uname string, needs root power. */
	len = sizes[m_in.sysuname_field];
	if (mp->mp_effuid != 0 || len == 0) return(EPERM);
	n = len < m_in.sysuname_len ? len : m_in.sysuname_len;
	if (n <= 0) return(EINVAL);
	r = sys_vircopy(mp->mp_endpoint, (phys_bytes) m_in.sysuname_value,
		SELF, (phys_bytes) tmp, (phys_bytes) n);
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
	sys_sysctl_stacktrace(mp->mp_endpoint);
	return EPERM;
  }

  switch(m_in.SI_WHAT) {
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

  if (len != m_in.SI_SIZE)
	return(EINVAL);

  dst_addr = (vir_bytes) m_in.SI_WHERE;
  return sys_datacopy(SELF, src_addr, who_e, dst_addr, len);
}

/*===========================================================================*
 *				do_getprocnr			             *
 *===========================================================================*/
int do_getprocnr()
{
  register struct mproc *rmp;
  static char search_key[PROC_NAME_LEN+1];
  int key_len;
  int s;

  /* This call should be moved to DS. */
  if (mp->mp_effuid != 0)
  {
	/* For now, allow non-root processes to request their own endpoint. */
	if (m_in.pid < 0 && m_in.namelen == 0) {
		mp->mp_reply.PM_ENDPT = who_e;
		mp->mp_reply.PM_PENDPT = NONE;
		return OK;
	}

	printf("PM: unauthorized call of do_getprocnr by proc %d\n",
		mp->mp_endpoint);
	sys_sysctl_stacktrace(mp->mp_endpoint);
	return EPERM;
  }

#if 0
  printf("PM: do_getprocnr(%d) call from endpoint %d, %s\n",
	m_in.pid, mp->mp_endpoint, mp->mp_name);
#endif

  if (m_in.pid >= 0) {			/* lookup process by pid */
	if ((rmp = find_proc(m_in.pid)) != NULL) {
		mp->mp_reply.PM_ENDPT = rmp->mp_endpoint;
#if 0
		printf("PM: pid result: %d\n", rmp->mp_endpoint);
#endif
		return(OK);
	}
  	return(ESRCH);			
  } else if (m_in.namelen > 0) {	/* lookup process by name */
  	key_len = MIN(m_in.namelen, PROC_NAME_LEN);
 	if (OK != (s=sys_datacopy(who_e, (vir_bytes) m_in.PMBRK_ADDR, 
 			SELF, (vir_bytes) search_key, key_len))) 
 		return(s);
 	search_key[key_len] = '\0';	/* terminate for safety */
  	for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
		if (((rmp->mp_flags & (IN_USE | EXITING)) == IN_USE) && 
			strncmp(rmp->mp_name, search_key, key_len)==0) {
  			mp->mp_reply.PM_ENDPT = rmp->mp_endpoint;
  			return(OK);
		} 
	}
  	return(ESRCH);			
  } else {			/* return own/parent process number */
#if 0
	printf("PM: endpt result: %d\n", mp->mp_reply.PM_ENDPT);
#endif
  	mp->mp_reply.PM_ENDPT = who_e;
	mp->mp_reply.PM_PENDPT = mproc[mp->mp_parent].mp_endpoint;
  }

  return(OK);
}

/*===========================================================================*
 *				do_getepinfo			             *
 *===========================================================================*/
int do_getepinfo()
{
  register struct mproc *rmp;
  endpoint_t ep;

  ep = m_in.PM_ENDPT;

  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
	if ((rmp->mp_flags & IN_USE) && (rmp->mp_endpoint == ep)) {
		mp->mp_reply.reply_res2 = rmp->mp_effuid;
		mp->mp_reply.reply_res3 = rmp->mp_effgid;
		return(rmp->mp_pid);
	}
  }

  /* Process not found */
  return(ESRCH);
}

/*===========================================================================*
 *				do_getepinfo_o			             *
 *===========================================================================*/
int do_getepinfo_o()
{
  register struct mproc *rmp;
  endpoint_t ep;

  /* This call should be moved to DS. */
  if (mp->mp_effuid != 0) {
	printf("PM: unauthorized call of do_getepinfo_o by proc %d\n",
		mp->mp_endpoint);
	sys_sysctl_stacktrace(mp->mp_endpoint);
	return EPERM;
  }

  ep = m_in.PM_ENDPT;

  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
	if ((rmp->mp_flags & IN_USE) && (rmp->mp_endpoint == ep)) {
		mp->mp_reply.reply_res2 = (short) rmp->mp_effuid;
		mp->mp_reply.reply_res3 = (char) rmp->mp_effgid;
		return(rmp->mp_pid);
	}
  }

  /* Process not found */
  return(ESRCH);
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
  abort_flag = (unsigned) m_in.reboot_flag;
  if (abort_flag >= RBT_INVALID) return(EINVAL); 

  /* Order matters here. When VFS is told to reboot, it exits all its
   * processes, and then would be confused if they're exited again by
   * SIGKILL. So first kill, then reboot. 
   */

  check_sig(-1, SIGKILL, FALSE /* ksig*/); /* kill all users except init */
  sys_stop(INIT_PROC_NR);		   /* stop init, but keep it around */

  /* Tell VFS to reboot */
  m.m_type = PM_REBOOT;

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

	arg_which = m_in.m1_i1;
	arg_who = m_in.m1_i2;
	arg_pri = m_in.m1_i3;	/* for SETPRIORITY */

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
	if (call_nr == GETPRIORITY) {
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

  req = m_in.svrctl_req;
  ptr = (vir_bytes) m_in.svrctl_argp;

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
 *				_brk				             *
 *===========================================================================*/

extern char *_brksize;
int brk(brk_addr)
void *brk_addr;
{
	int r;
/* PM wants to call brk() itself. */
	if((r=vm_brk(PM_PROC_NR, brk_addr)) != OK) {
#if 0
		printf("PM: own brk(%p) failed: vm_brk() returned %d\n",
			brk_addr, r);
#endif
		return -1;
	}
	_brksize = brk_addr;
	return 0;
}
