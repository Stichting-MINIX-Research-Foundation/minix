/* Miscellaneous system calls.				Author: Kees J. Bot
 *								31 Mar 2000
 * The entry points into this file are:
 *   do_reboot: kill all processes, then reboot system
 *   do_procstat: request process status  (Jorrit N. Herder)
 *   do_getsysinfo: request copy of PM data structure  (Jorrit N. Herder)
 *   do_getprocnr: lookup process slot number  (Jorrit N. Herder)
 *   do_allocmem: allocate a chunk of memory  (Jorrit N. Herder)
 *   do_freemem: deallocate a chunk of memory  (Jorrit N. Herder)
 *   do_getsetpriority: get/set process priority
 *   do_svrctl: process manager control
 */

#include "pm.h"
#include <minix/callnr.h>
#include <signal.h>
#include <sys/svrctl.h>
#include <sys/resource.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/type.h>
#include <string.h>
#include <lib.h>
#include "mproc.h"
#include "param.h"
#include "../../kernel/proc.h"

/*===========================================================================*
 *				do_allocmem				     *
 *===========================================================================*/
PUBLIC int do_allocmem()
{
  vir_clicks mem_clicks;
  phys_clicks mem_base;

  mem_clicks = (m_in.memsize + CLICK_SIZE -1 ) >> CLICK_SHIFT;
  mem_base = alloc_mem(mem_clicks);
  if (mem_base == NO_MEM) return(ENOMEM);
  mp->mp_reply.membase =  (phys_bytes) (mem_base << CLICK_SHIFT);
  return(OK);
}

/*===========================================================================*
 *				do_freemem				     *
 *===========================================================================*/
PUBLIC int do_freemem()
{
  vir_clicks mem_clicks;
  phys_clicks mem_base;

  mem_clicks = (m_in.memsize + CLICK_SIZE -1 ) >> CLICK_SHIFT;
  mem_base = (m_in.membase + CLICK_SIZE -1 ) >> CLICK_SHIFT;
  free_mem(mem_base, mem_clicks);
  return(OK);
}

/*===========================================================================*
 *				do_procstat				     *
 *===========================================================================*/
PUBLIC int do_procstat()
{ 
  /* For the moment, this is only used to return pending signals to 
   * system processes that request the PM for their own status. 
   *
   * Future use might include the FS requesting for process status of
   * any user process. 
   */
  if (m_in.stat_nr == SELF) {
      mp->mp_reply.sig_set = mp->mp_sigpending;
      sigemptyset(&mp->mp_sigpending);
  } 
  else {
      return(ENOSYS);
  }
  return(OK);
}

/*===========================================================================*
 *				do_getsysinfo			       	     *
 *===========================================================================*/
PUBLIC int do_getsysinfo()
{
  struct mproc *proc_addr;
  vir_bytes src_addr, dst_addr;
  struct kinfo kinfo;
  struct loadinfo loadinfo;
  static struct proc proctab[NR_PROCS+NR_TASKS];
  size_t len;
  static struct pm_mem_info pmi;
  int s, r;
  size_t holesize;

  switch(m_in.info_what) {
  case SI_KINFO:			/* kernel info is obtained via PM */
        sys_getkinfo(&kinfo);
        src_addr = (vir_bytes) &kinfo;
        len = sizeof(struct kinfo);
        break;
  case SI_PROC_ADDR:			/* get address of PM process table */
  	proc_addr = &mproc[0];
  	src_addr = (vir_bytes) &proc_addr;
  	len = sizeof(struct mproc *);
  	break; 
  case SI_PROC_TAB:			/* copy entire process table */
        src_addr = (vir_bytes) mproc;
        len = sizeof(struct mproc) * NR_PROCS;
        break;
  case SI_KPROC_TAB:			/* copy entire process table */
	if((r=sys_getproctab(proctab)) != OK)
		return r;
	src_addr = (vir_bytes) proctab;
	len = sizeof(proctab);
        break;
  case SI_MEM_ALLOC:
  	holesize = sizeof(pmi.pmi_holes);
	if((r=mem_holes_copy(pmi.pmi_holes, &holesize,
	   &pmi.pmi_hi_watermark)) != OK)
		return r;
	src_addr = (vir_bytes) &pmi;
	len = sizeof(pmi);
	break;
  case SI_LOADINFO:			/* loadinfo is obtained via PM */
        sys_getloadinfo(&loadinfo);
        src_addr = (vir_bytes) &loadinfo;
        len = sizeof(struct loadinfo);
        break;
  default:
  	return(EINVAL);
  }

  dst_addr = (vir_bytes) m_in.info_where;
  if (OK != (s=sys_datacopy(SELF, src_addr, who_e, dst_addr, len)))
  	return(s);
  return(OK);
}

/*===========================================================================*
 *				do_getprocnr			             *
 *===========================================================================*/
PUBLIC int do_getprocnr()
{
  register struct mproc *rmp;
  static char search_key[PROC_NAME_LEN+1];
  int key_len;
  int s;

  if (m_in.pid >= 0) {			/* lookup process by pid */
  	for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
		if ((rmp->mp_flags & IN_USE) && (rmp->mp_pid==m_in.pid)) {
  			mp->mp_reply.endpt = rmp->mp_endpoint;
  			return(OK);
		} 
	}
  	return(ESRCH);			
  } else if (m_in.namelen > 0) {	/* lookup process by name */
  	key_len = MIN(m_in.namelen, PROC_NAME_LEN);
 	if (OK != (s=sys_datacopy(who_e, (vir_bytes) m_in.addr, 
 			SELF, (vir_bytes) search_key, key_len))) 
 		return(s);
 	search_key[key_len] = '\0';	/* terminate for safety */
  	for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
		if (((rmp->mp_flags & (IN_USE | ZOMBIE)) == IN_USE) && 
			strncmp(rmp->mp_name, search_key, key_len)==0) {
  			mp->mp_reply.endpt = rmp->mp_endpoint;
  			return(OK);
		} 
	}
  	return(ESRCH);			
  } else {			/* return own/parent process number */
  	mp->mp_reply.endpt = who_e;
	mp->mp_reply.pendpt = mproc[mp->mp_parent].mp_endpoint;
  }

  return(OK);
}

/*===========================================================================*
 *				do_reboot				     *
 *===========================================================================*/
PUBLIC int do_reboot()
{
  char monitor_code[256];		
  vir_bytes code_addr;
  int code_size;
  int abort_flag;

  /* Check permission to abort the system. */
  if (mp->mp_effuid != SUPER_USER) return(EPERM);

  /* See how the system should be aborted. */
  abort_flag = (unsigned) m_in.reboot_flag;
  if (abort_flag >= RBT_INVALID) return(EINVAL); 
  if (RBT_MONITOR == abort_flag) {
	int r;
	if(m_in.reboot_strlen >= sizeof(monitor_code))
		return EINVAL;
	if((r = sys_datacopy(who_e, (vir_bytes) m_in.reboot_code,
		SELF, (vir_bytes) monitor_code, m_in.reboot_strlen)) != OK)
		return r;
	code_addr = (vir_bytes) monitor_code;
	monitor_code[m_in.reboot_strlen] = '\0';
	code_size = m_in.reboot_strlen + 1;
  }

  /* Order matters here. When FS is told to reboot, it exits all its
   * processes, and then would be confused if they're exited again by
   * SIGKILL. So first kill, then reboot. 
   */

  check_sig(-1, SIGKILL); 		/* kill all users except init */
  sys_nice(INIT_PROC_NR, PRIO_STOP);	/* stop init, but keep it around */
  tell_fs(REBOOT, 0, 0, 0);		/* tell FS to synchronize */

  /* Ask the kernel to abort. All system services, including the PM, will 
   * get a HARD_STOP notification. Await the notification in the main loop.
   */
  sys_abort(abort_flag, PM_PROC_NR, code_addr, code_size);
  return(SUSPEND);			/* don't reply to caller */
}

/*===========================================================================*
 *				do_getsetpriority			     *
 *===========================================================================*/
PUBLIC int do_getsetpriority()
{
	int arg_which, arg_who, arg_pri;
	int rmp_nr;
	struct mproc *rmp;

	arg_which = m_in.m1_i1;
	arg_who = m_in.m1_i2;
	arg_pri = m_in.m1_i3;	/* for SETPRIORITY */

	/* Code common to GETPRIORITY and SETPRIORITY. */

	/* Only support PRIO_PROCESS for now. */
	if (arg_which != PRIO_PROCESS)
		return(EINVAL);

	if (arg_who == 0)
		rmp_nr = who_p;
	else
		if ((rmp_nr = proc_from_pid(arg_who)) < 0)
			return(ESRCH);

	rmp = &mproc[rmp_nr];

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
	
	/* We're SET, and it's allowed. Do it and tell kernel. */
	rmp->mp_nice = arg_pri;
	return sys_nice(rmp->mp_endpoint, arg_pri);
}

/*===========================================================================*
 *				do_svrctl				     *
 *===========================================================================*/
PUBLIC int do_svrctl()
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

  /* Is the request indeed for the MM? */
  if (((req >> 8) & 0xFF) != 'M') return(EINVAL);

  /* Control operations local to the PM. */
  switch(req) {
  case MMSETPARAM:
  case MMGETPARAM: {
      struct sysgetenv sysgetenv;
      char search_key[64];
      char *val_start;
      size_t val_len;
      size_t copy_len;

      /* Copy sysgetenv structure to PM. */
      if (sys_datacopy(who_e, ptr, SELF, (vir_bytes) &sysgetenv, 
              sizeof(sysgetenv)) != OK) return(EFAULT);  

      /* Set a param override? */
      if (req == MMSETPARAM) {
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
              sysgetenv.keylen)) != OK)
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

#if ENABLE_SWAP
  case MMSWAPON: {
	struct mmswapon swapon;

	if (mp->mp_effuid != SUPER_USER) return(EPERM);

	if (sys_datacopy(who_e, (phys_bytes) ptr,
		PM_PROC_NR, (phys_bytes) &swapon,
		(phys_bytes) sizeof(swapon)) != OK) return(EFAULT);

	return(swap_on(swapon.file, swapon.offset, swapon.size)); }

  case MMSWAPOFF: {
	if (mp->mp_effuid != SUPER_USER) return(EPERM);

	return(swap_off()); }
#endif /* SWAP */

  default:
	return(EINVAL);
  }
}

/*===========================================================================*
 *				_read_pm				     *
 *===========================================================================*/
PUBLIC ssize_t _read_pm(fd, buffer, nbytes, seg, ep)
int fd;
void *buffer;
size_t nbytes;
int seg;
int ep;
{
  message m;

  m.m1_i1 = _PM_SEG_FLAG | fd;
  m.m1_i2 = nbytes;
  m.m1_p1 = (char *) buffer;
  m.m1_p2 = (char *) seg;
  m.m1_p3 = (char *) ep;
  return(_syscall(FS_PROC_NR, READ, &m));
}

/*===========================================================================*
 *				_write_pm				     *
 *===========================================================================*/
PUBLIC ssize_t _write_pm(fd, buffer, nbytes, seg, ep)
int fd;
void *buffer;
size_t nbytes;
int seg;
int ep;
{
  message m;

  m.m1_i1 = _PM_SEG_FLAG | fd;
  m.m1_i2 = nbytes;
  m.m1_p1 = (char *) buffer;
  m.m1_p2 = (char *) seg;
  m.m1_p3 = (char *) ep;
  return(_syscall(FS_PROC_NR, WRITE, &m));
}

