/*
 * a loop that gets messages requesting work, carries out the work, and sends
 * replies.
 *
 * The entry points into this file are:
 *   main:	main program of the Virtual File System
 *   reply:	send a reply to a process after the requested work is done
 *
 * Changes for VFS:
 *   Jul 2006 (Balazs Gerofi)
 */

#include "fs.h"
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/ioc_memory.h>
#include <sys/svrctl.h>
#include <sys/select.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <minix/safecopies.h>
#include <minix/debug.h>
#include "file.h"
#include "fproc.h"
#include "param.h"

#include <minix/vfsif.h>
#include "vmnt.h"
#include "vnode.h"

#if ENABLE_SYSCALL_STATS
EXTERN unsigned long calls_stats[NCALLS];
#endif

FORWARD _PROTOTYPE( void get_work, (void)				);
FORWARD _PROTOTYPE( void init_root, (void)				);
FORWARD _PROTOTYPE( void service_pm, (void)				);

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC int main(void)
{
/* This is the main program of the file system.  The main loop consists of
 * three major activities: getting new work, processing the work, and sending
 * the reply.  This loop never terminates as long as the file system runs.
 */
  int error;

  /* SEF local startup. */
  sef_local_startup();

  /* This is the main loop that gets work, processes it, and sends replies. */
  while (TRUE) {
	SANITYCHECK;
	get_work();		/* sets who and call_nr */

	if (call_nr == DEV_REVIVE)
	{
		endpoint_t endpt;

		endpt = m_in.REP_ENDPT;
		if(endpt == FS_PROC_NR) {
			endpt = suspended_ep(m_in.m_source, m_in.REP_IO_GRANT);
			if(endpt == NONE) {
				printf("FS: proc with "
			"grant %d from %d not found (revive)\n",
					m_in.REP_IO_GRANT, m_in.m_source);
				continue;
			}
		}
		revive(endpt, m_in.REP_STATUS);
		continue;
	}
	if (call_nr == DEV_REOPEN_REPL)
	{
		reopen_reply();
		continue;
	}
	if (call_nr == DEV_CLOSE_REPL)
	{
		close_reply();
		continue;
	}
	if (call_nr == DEV_SEL_REPL1)
	{
		select_reply1();
		continue;
	}
	if (call_nr == DEV_SEL_REPL2)
	{
		select_reply2();
		continue;
	}

 	/* Check for special control messages first. */
        if (is_notify(call_nr)) {
		if (who_p == PM_PROC_NR)
		{
			/* Signaled by PM, ignore. */
		}
		else if (who_p == CLOCK)
		{
			/* Alarm timer expired. Used only for select().
			 * Check it.
			 */
			fs_expire_timers(m_in.NOTIFY_TIMESTAMP);
		}
		else
		{
			/* Device notifies us of an event. */
			dev_status(&m_in);
		}
		SANITYCHECK;
		continue;
	}

	/* We only expect notify()s from tasks. */
	if(who_p < 0) {
    		printf("FS: ignoring message from %d (%d)\n",
			who_e, m_in.m_type);
		continue;
	}

	/* Now it's safe to set and check fp. */
	fp = &fproc[who_p];	/* pointer to proc table struct */
	super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */

#if DO_SANITYCHECKS
	if(fp_is_blocked(fp)) {
		printf("VFS: requester %d call %d: suspended\n",
			who_e, call_nr);
		panic(__FILE__, "requester suspended", NO_NUM);
	}
#endif

	/* Calls from PM. */
	if (who_e == PM_PROC_NR) {
		service_pm();

		continue;
	}

	/* Calls from VM. */
	if(who_e == VM_PROC_NR) {
	    int caught = 1;
	    switch(call_nr)
	    {
		case VM_VFS_OPEN:
			error = do_vm_open();
			break;
		case VM_VFS_CLOSE:
			error = do_vm_close();
			break;
		case VM_VFS_MMAP:
			error = do_vm_mmap();
			break;
		default:
			caught = 0;
			error = 0; /* To satisfy lints. */
			break;
	   }
	   if(caught) {
		reply(who_e, error);
		continue;
	   }
	}

		SANITYCHECK;

	  /* Other calls. */
	  switch(call_nr)
	  {
	      case MAPDRIVER:
		error= do_mapdriver();
		if (error != SUSPEND) reply(who_e, error);
		break;

	      default:
		/* Call the internal function that does the work. */
		if (call_nr < 0 || call_nr >= NCALLS) { 
			error = SUSPEND;
			/* Not supposed to happen. */
			printf("VFS: illegal %d system call by %d\n",
				call_nr, who_e);
		} else if (fp->fp_pid == PID_FREE) {
			error = ENOSYS;
			printf(
		"FS, bad process, who = %d, call_nr = %d, endpt1 = %d\n",
				 who_e, call_nr, m_in.endpt1);
		} else {
#if ENABLE_SYSCALL_STATS
			calls_stats[call_nr]++;
#endif
			SANITYCHECK;
			error = (*call_vec[call_nr])();
			SANITYCHECK;
		}

		/* Copy the results back to the user and send reply. */
		if (error != SUSPEND) { reply(who_e, error); }
	}
	SANITYCHECK;
  }
  return(OK);				/* shouldn't come here */
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_restart_fail);

  /* No live update support for now. */

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *				sef_cb_init_fresh			     *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the virtual file server. */
  int s;
  register struct fproc *rfp;
  struct vmnt *vmp;
  struct vnode *root_vp;
  message mess;

  /* Clear endpoint field */
  last_login_fs_e = NONE;
  mount_m_in.m1_p3 = (char *) NONE;

  /* Initialize the process table with help of the process manager messages. 
   * Expect one message for each system process with its slot number and pid. 
   * When no more processes follow, the magic process number NONE is sent. 
   * Then, stop and synchronize with the PM.
   */
  do {
  	if (OK != (s=sef_receive(PM_PROC_NR, &mess)))
  		panic(__FILE__,"FS couldn't receive from PM", s);

	if (mess.m_type != PM_INIT)
		panic(__FILE__, "unexpected message from PM", mess.m_type);

  	if (NONE == mess.PM_PROC) break; 

	rfp = &fproc[mess.PM_SLOT];
	rfp->fp_pid = mess.PM_PID;
	rfp->fp_endpoint = mess.PM_PROC;
	rfp->fp_realuid = (uid_t) SYS_UID;
	rfp->fp_effuid = (uid_t) SYS_UID;
	rfp->fp_realgid = (gid_t) SYS_GID;
	rfp->fp_effgid = (gid_t) SYS_GID;
	rfp->fp_umask = ~0;
	rfp->fp_grant = GRANT_INVALID;
	rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
	rfp->fp_revived = NOT_REVIVING;
   
  } while (TRUE);			/* continue until process NONE */
  mess.m_type = OK;			/* tell PM that we succeeded */
  s = send(PM_PROC_NR, &mess);		/* send synchronization message */

  /* All process table entries have been set. Continue with FS initialization.
   * Certain relations must hold for the file system to work at all. Some 
   * extra block_size requirements are checked at super-block-read-in time.
   */
  if (OPEN_MAX > 127) panic(__FILE__,"OPEN_MAX > 127", NO_NUM);
  
  /* The following initializations are needed to let dev_opcl succeed .*/
  fp = (struct fproc *) NULL;
  who_e = who_p = FS_PROC_NR;

  build_dmap();			/* build device table and map boot driver */
  init_root();			/* init root device and load super block */
  init_select();		/* init select() structures */


  vmp = &vmnt[0];		/* Should be the root filesystem */
  if (vmp->m_dev == NO_DEV)
	panic(__FILE__, "vfs: no root filesystem", NO_NUM);
  root_vp= vmp->m_root_node;

  /* The root device can now be accessed; set process directories. */
  for (rfp=&fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	FD_ZERO(&(rfp->fp_filp_inuse));
  	if (rfp->fp_pid != PID_FREE) {
                
		dup_vnode(root_vp);
                rfp->fp_rd = root_vp;
		dup_vnode(root_vp);
                rfp->fp_wd = root_vp;
		
  	} else  rfp->fp_endpoint = NONE;
  }

  system_hz = sys_hz();

  SANITYCHECK;

#if DO_SANITYCHECKS
  FIXME("VFS: DO_SANITYCHECKS is on");
#endif

  return(OK);
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE void get_work()
{  
  /* Normally wait for new input.  However, if 'reviving' is
   * nonzero, a suspended process must be awakened.
   */
  int r, found_one, fd_nr;
  struct filp *f;
  register struct fproc *rp;

  while (reviving != 0) {
	found_one= FALSE;

	/* Revive a suspended process. */
	for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++) 
		if (rp->fp_pid != PID_FREE && rp->fp_revived == REVIVING) {
			int blocked_on = rp->fp_blocked_on;
			found_one= TRUE;
			who_p = (int)(rp - fproc);
			who_e = rp->fp_endpoint;
			call_nr = rp->fp_fd & BYTE;

			m_in.fd = (rp->fp_fd >>8) & BYTE;
			m_in.buffer = rp->fp_buffer;
			m_in.nbytes = rp->fp_nbytes;
			/*no longer hanging*/
			rp->fp_blocked_on = FP_BLOCKED_ON_NONE;
			rp->fp_revived = NOT_REVIVING;
			reviving--;
			/* This should be a pipe I/O, not a device I/O.
			 * If it is, it'll 'leak' grants.
			 */
			assert(!GRANT_VALID(rp->fp_grant));

			if (blocked_on == FP_BLOCKED_ON_PIPE)
			{
				fp= rp;
				fd_nr= (rp->fp_fd >> 8);
				f= get_filp(fd_nr);
				assert(f != NULL);
				r= rw_pipe((call_nr == READ) ? READING :
					WRITING, who_e, fd_nr, f,
					rp->fp_buffer, rp->fp_nbytes);
				if (r != SUSPEND)
					reply(who_e, r);
				continue;
			}

			return;
		}
	if (!found_one)
		panic(__FILE__,"get_work couldn't revive anyone", NO_NUM);
  }

  for(;;) {
    int r;
    /* Normal case.  No one to revive. */
    if ((r=sef_receive(ANY, &m_in)) != OK)
	panic(__FILE__,"fs sef_receive error", r);
    who_e = m_in.m_source;
    who_p = _ENDPOINT_P(who_e);

    /* 
     * negative who_p is never used to access the fproc array. Negative numbers
     * (kernel tasks) are treated in a special way
     */
    if(who_p >= (int)(sizeof(fproc) / sizeof(struct fproc)))
     	panic(__FILE__,"receive process out of range", who_p);
    if(who_p >= 0 && fproc[who_p].fp_endpoint == NONE) {
    	printf("FS: ignoring request from %d, endpointless slot %d (%d)\n",
		m_in.m_source, who_p, m_in.m_type);
	continue;
    }
    if(who_p >= 0 && fproc[who_p].fp_endpoint != who_e) {
	if(fproc[who_p].fp_endpoint == NONE) { 
		printf("slot unknown even\n");
	}
    	printf("FS: receive endpoint inconsistent (source %d, who_p %d, stored ep %d, who_e %d).\n",
		m_in.m_source, who_p, fproc[who_p].fp_endpoint, who_e);
#if 0
	panic(__FILE__, "FS: inconsistent endpoint ", NO_NUM);
#endif
	continue;
    }
    call_nr = m_in.m_type;
    return;
  }
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC void reply(whom, result)
int whom;			/* process to reply to */
int result;			/* result of the call (usually OK or error #) */
{
/* Send a reply to a user process.  If the send fails, just ignore it. */
  int s;

#if 0
  if (call_nr == SYMLINK)
	printf("vfs:reply: replying %d for call %d\n", result, call_nr);
#endif

  m_out.reply_type = result;
  s = sendnb(whom, &m_out);
  if (s != OK) printf("VFS: couldn't send reply %d to %d: %d\n",
	result, whom, s);
}

/*===========================================================================*
 *				init_root				     *
 *===========================================================================*/
PRIVATE void init_root()
{
  int r = OK;
  struct vmnt *vmp;
  struct vnode *root_node;
  struct dmap *dp;
  char *label;
  message m;
  struct node_details res;
  
  /* Open the root device. */
  root_dev = DEV_IMGRD;
  ROOT_FS_E = MFS_PROC_NR;
  
  /* Wait FS login message */
  if (last_login_fs_e != ROOT_FS_E) {
	  /* Wait FS login message */
	  if (sef_receive(ROOT_FS_E, &m) != OK) {
		  printf("VFS: Error receiving login request from FS_e %d\n", 
				  ROOT_FS_E);
		  panic(__FILE__, "Error receiving login request from root filesystem\n", ROOT_FS_E);
	  }
	  if (m.m_type != FS_READY) {
		  printf("VFS: Invalid login request from FS_e %d\n", 
				  ROOT_FS_E);
		  panic(__FILE__, "Error receiving login request from root filesystem\n", ROOT_FS_E);
	  }
  }
  last_login_fs_e = NONE;
  
  /* Initialize vmnt table */
  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp)
      vmp->m_dev = NO_DEV;
  
  vmp = &vmnt[0];
 
  /* We'll need a vnode for the root inode, check whether there is one */
  if ((root_node = get_free_vnode()) == NIL_VNODE) 
	panic(__FILE__,"Cannot get free vnode", r);

  
  /* Get driver process' endpoint */  
  dp = &dmap[(root_dev >> MAJOR) & BYTE];
  if (dp->dmap_driver == NONE) {
	panic(__FILE__,"No driver for root device", r);
  }

  label= dp->dmap_label;
  if (strlen(label) == 0)
  {
	panic(__FILE__, "vfs:init_root: no label for major", root_dev >> MAJOR);
  }

  /* Issue request */
  r = req_readsuper(ROOT_FS_E, label, root_dev, 0 /*!readonly*/,
	1 /*isroot*/, &res);
  if (r != OK) {
      panic(__FILE__,"Cannot read superblock from root", r);
  }
  
  /* Fill in root node's fields */
  root_node->v_fs_e = res.fs_e;
  root_node->v_inode_nr = res.inode_nr;
  root_node->v_mode = res.fmode;
  root_node->v_size = res.fsize;
  root_node->v_sdev = NO_DEV;
  root_node->v_fs_count = 1;
  root_node->v_ref_count = 1;

  /* Fill in max file size and blocksize for the vmnt */
  vmp->m_fs_e = res.fs_e;
  vmp->m_dev = root_dev;
  vmp->m_flags = 0;
  
  /* Root node is indeed on the partition */
  root_node->v_vmnt = vmp;
  root_node->v_dev = vmp->m_dev;

  /* Root directory is not mounted on a vnode. */
  vmp->m_mounted_on = NULL;
  vmp->m_root_node = root_node;
  strcpy(vmp->m_label, "fs_imgrd");	/* FIXME: obtain this from RS */
}

/*===========================================================================*
 *				service_pm				     *
 *===========================================================================*/
PRIVATE void service_pm()
{
  int r;

  switch (call_nr) {
  case PM_SETUID:
	pm_setuid(m_in.PM_PROC, m_in.PM_EID, m_in.PM_RID);

	m_out.m_type = PM_SETUID_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

  case PM_SETGID:
	pm_setgid(m_in.PM_PROC, m_in.PM_EID, m_in.PM_RID);

	m_out.m_type = PM_SETGID_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

  case PM_SETSID:
	pm_setsid(m_in.PM_PROC);

	m_out.m_type = PM_SETSID_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

  case PM_EXEC:
	r = pm_exec(m_in.PM_PROC, m_in.PM_PATH, m_in.PM_PATH_LEN,
		m_in.PM_FRAME, m_in.PM_FRAME_LEN);

	/* Reply status to PM */
	m_out.m_type = PM_EXEC_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;
	m_out.PM_STATUS = r;

	break;

  case PM_EXIT:
	pm_exit(m_in.PM_PROC);

	/* Reply dummy status to PM for synchronization */
	m_out.m_type = PM_EXIT_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

  case PM_DUMPCORE:
	r = pm_dumpcore(m_in.PM_PROC,
		NULL /* (struct mem_map *) m_in.PM_SEGPTR */);

	/* Reply status to PM */
	m_out.m_type = PM_CORE_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;
	m_out.PM_STATUS = r;
	
	break;

  case PM_FORK:
  case PM_FORK_NB:
	pm_fork(m_in.PM_PPROC, m_in.PM_PROC, m_in.PM_CPID);

	m_out.m_type = (call_nr == PM_FORK) ? PM_FORK_REPLY : PM_FORK_NB_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;
  case PM_SETGROUPS:
	pm_setgroups(m_in.PM_PROC, m_in.PM_GROUP_NO, m_in.PM_GROUP_ADDR);

	m_out.m_type = PM_SETGROUPS_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

  case PM_UNPAUSE:
	unpause(m_in.PM_PROC);

	m_out.m_type = PM_UNPAUSE_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

  case PM_REBOOT:
	pm_reboot();

	/* Reply dummy status to PM for synchronization */
	m_out.m_type = PM_REBOOT_REPLY;

	break;

  default:
	printf("VFS: don't know how to handle PM request %x\n", call_nr);

	return;
  }

  r = send(PM_PROC_NR, &m_out);
  if (r != OK)
	panic(__FILE__, "service_pm: send failed", r);

}

