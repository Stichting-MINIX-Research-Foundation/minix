/* This file contains a collection of miscellaneous procedures.  Some of them
 * perform simple system calls.  Some others do a little part of system calls
 * that are mostly performed by the Memory Manager.
 *
 * The entry points into this file are
 *   do_dup:	  perform the DUP system call
 *   do_fcntl:	  perform the FCNTL system call
 *   do_sync:	  perform the SYNC system call
 *   do_fsync:	  perform the FSYNC system call
 *   do_reboot:	  sync disks and prepare for shutdown
 *   do_fork:	  adjust the tables after MM has performed a FORK system call
 *   do_exec:	  handle files with FD_CLOEXEC on after MM has done an EXEC
 *   do_exit:	  a process has exited; note that in the tables
 *   do_set:	  set uid or gid for some process
 *   do_revive:	  revive a process that was waiting for something (e.g. TTY)
 *   do_svrctl:	  file system control
 *   do_getsysinfo:	request copy of FS data structure
 */

#include "fs.h"
#include <fcntl.h>
#include <unistd.h>	/* cc runs out of memory with unistd.h :-( */
#include <minix/callnr.h>
#include <minix/com.h>
#include <sys/svrctl.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"
#include "super.h"

/*===========================================================================*
 *				do_getsysinfo				     *
 *===========================================================================*/
PUBLIC int do_getsysinfo()
{
  struct fproc *proc_addr;
  vir_bytes src_addr, dst_addr;
  size_t len;
  int s;

  switch(m_in.info_what) {
  case SI_PROC_ADDR:
  	proc_addr = &fproc[0];
  	src_addr = (vir_bytes) &proc_addr;
  	len = sizeof(struct fproc *);
  	break; 
  case SI_PROC_TAB:
  	src_addr = (vir_bytes) fproc;
  	len = sizeof(struct fproc) * NR_PROCS;
  	break; 
  case SI_DMAP_TAB:
  	src_addr = (vir_bytes) dmap;
  	len = sizeof(struct dmap) * NR_DEVICES;
  	break; 
  default:
  	return(EINVAL);
  }

  dst_addr = (vir_bytes) m_in.info_where;
  if (OK != (s=sys_datacopy(SELF, src_addr, who, dst_addr, len)))
  	return(s);
  return(OK);

}

/*===========================================================================*
 *				do_dup					     *
 *===========================================================================*/
PUBLIC int do_dup()
{
/* Perform the dup(fd) or dup2(fd,fd2) system call. These system calls are
 * obsolete.  In fact, it is not even possible to invoke them using the
 * current library because the library routines call fcntl().  They are
 * provided to permit old binary programs to continue to run.
 */

  register int rfd;
  register struct filp *f;
  struct filp *dummy;
  int r;

  /* Is the file descriptor valid? */
  rfd = m_in.fd & ~DUP_MASK;		/* kill off dup2 bit, if on */
  if ((f = get_filp(rfd)) == NIL_FILP) return(err_code);

  /* Distinguish between dup and dup2. */
  if (m_in.fd == rfd) {			/* bit not on */
	/* dup(fd) */
	if ( (r = get_fd(0, 0, &m_in.fd2, &dummy)) != OK) return(r);
  } else {
	/* dup2(fd, fd2) */
	if (m_in.fd2 < 0 || m_in.fd2 >= OPEN_MAX) return(EBADF);
	if (rfd == m_in.fd2) return(m_in.fd2);	/* ignore the call: dup2(x, x) */
	m_in.fd = m_in.fd2;		/* prepare to close fd2 */
	(void) do_close();	/* cannot fail */
  }

  /* Success. Set up new file descriptors. */
  f->filp_count++;
  fp->fp_filp[m_in.fd2] = f;
  return(m_in.fd2);
}

/*===========================================================================*
 *				do_fcntl				     *
 *===========================================================================*/
PUBLIC int do_fcntl()
{
/* Perform the fcntl(fd, request, ...) system call. */

  register struct filp *f;
  int new_fd, r, fl;
  long cloexec_mask;		/* bit map for the FD_CLOEXEC flag */
  long clo_value;		/* FD_CLOEXEC flag in proper position */
  struct filp *dummy;

  /* Is the file descriptor valid? */
  if ((f = get_filp(m_in.fd)) == NIL_FILP) return(err_code);

  switch (m_in.request) {
     case F_DUPFD:
	/* This replaces the old dup() system call. */
	if (m_in.addr < 0 || m_in.addr >= OPEN_MAX) return(EINVAL);
	if ((r = get_fd(m_in.addr, 0, &new_fd, &dummy)) != OK) return(r);
	f->filp_count++;
	fp->fp_filp[new_fd] = f;
	return(new_fd);

     case F_GETFD:
	/* Get close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
	return( ((fp->fp_cloexec >> m_in.fd) & 01) ? FD_CLOEXEC : 0);

     case F_SETFD:
	/* Set close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
	cloexec_mask = 1L << m_in.fd;	/* singleton set position ok */
	clo_value = (m_in.addr & FD_CLOEXEC ? cloexec_mask : 0L);
	fp->fp_cloexec = (fp->fp_cloexec & ~cloexec_mask) | clo_value;
	return(OK);

     case F_GETFL:
	/* Get file status flags (O_NONBLOCK and O_APPEND). */
	fl = f->filp_flags & (O_NONBLOCK | O_APPEND | O_ACCMODE);
	return(fl);	

     case F_SETFL:
	/* Set file status flags (O_NONBLOCK and O_APPEND). */
	fl = O_NONBLOCK | O_APPEND;
	f->filp_flags = (f->filp_flags & ~fl) | (m_in.addr & fl);
	return(OK);

     case F_GETLK:
     case F_SETLK:
     case F_SETLKW:
	/* Set or clear a file lock. */
	r = lock_op(f, m_in.request);
	return(r);
     default:
	return(EINVAL);
  }
}

/*===========================================================================*
 *				do_sync					     *
 *===========================================================================*/
PUBLIC int do_sync()
{
/* Perform the sync() system call.  Flush all the tables. 
 * The order in which the various tables are flushed is critical.  The
 * blocks must be flushed last, since rw_inode() leaves its results in
 * the block cache.
 */
  register struct inode *rip;
  register struct buf *bp;

  /* Write all the dirty inodes to the disk. */
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++)
	if (rip->i_count > 0 && rip->i_dirt == DIRTY) rw_inode(rip, WRITING);

  /* Write all the dirty blocks to the disk, one drive at a time. */
  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++)
	if (bp->b_dev != NO_DEV && bp->b_dirt == DIRTY) flushall(bp->b_dev);

  return(OK);		/* sync() can't fail */
}

/*===========================================================================*
 *				do_fsync				     *
 *===========================================================================*/
PUBLIC int do_fsync()
{
/* Perform the fsync() system call. For now, don't be unnecessarily smart. */

  do_sync();

  return(OK);
}

/*===========================================================================*
 *				do_reboot				     *
 *===========================================================================*/
PUBLIC int do_reboot()
{
  /* Perform the FS side of the reboot call. */
  int i;
  struct super_block *sp;
  struct inode dummy;

  /* Only PM may make this call directly. */
  if (who != PM_PROC_NR) return(EGENERIC);

  /* Do exit processing for all leftover processes and servers. */
  for (i = 0; i < NR_PROCS; i++) { m_in.slot1 = i; do_exit(); }

  /* The root file system is mounted onto itself, which keeps it from being
   * unmounted.  Pull an inode out of thin air and put the root on it.
   */
  put_inode(super_block[0].s_imount);
  super_block[0].s_imount= &dummy;
  dummy.i_count = 2;			/* expect one "put" */

  /* Unmount all filesystems.  File systems are mounted on other file systems,
   * so you have to pull off the loose bits repeatedly to get it all undone.
   */
  for (i= 0; i < NR_SUPERS; i++) {
	/* Unmount at least one. */
	for (sp= &super_block[0]; sp < &super_block[NR_SUPERS]; sp++) {
		if (sp->s_dev != NO_DEV) (void) unmount(sp->s_dev);
	}
  }

  return(OK);
}

/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
PUBLIC int do_fork()
{
/* Perform those aspects of the fork() system call that relate to files.
 * In particular, let the child inherit its parent's file descriptors.
 * The parent and child parameters tell who forked off whom. The file
 * system uses the same slot numbers as the kernel.  Only MM makes this call.
 */

  register struct fproc *cp;
  int i;

  /* Only PM may make this call directly. */
  if (who != PM_PROC_NR) return(EGENERIC);

  /* Copy the parent's fproc struct to the child. */
  fproc[m_in.child] = fproc[m_in.parent];

  /* Increase the counters in the 'filp' table. */
  cp = &fproc[m_in.child];
  for (i = 0; i < OPEN_MAX; i++)
	if (cp->fp_filp[i] != NIL_FILP) cp->fp_filp[i]->filp_count++;

  /* Fill in new process id. */
  cp->fp_pid = m_in.pid;

  /* A child is not a process leader. */
  cp->fp_sesldr = 0;

  /* Record the fact that both root and working dir have another user. */
  dup_inode(cp->fp_rootdir);
  dup_inode(cp->fp_workdir);
  return(OK);
}

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
PUBLIC int do_exec()
{
/* Files can be marked with the FD_CLOEXEC bit (in fp->fp_cloexec).  When
 * MM does an EXEC, it calls FS to allow FS to find these files and close them.
 */

  register int i;
  long bitmap;

  /* Only PM may make this call directly. */
  if (who != PM_PROC_NR) return(EGENERIC);

  /* The array of FD_CLOEXEC bits is in the fp_cloexec bit map. */
  fp = &fproc[m_in.slot1];		/* get_filp() needs 'fp' */
  bitmap = fp->fp_cloexec;
  if (bitmap == 0) return(OK);	/* normal case, no FD_CLOEXECs */

  /* Check the file desriptors one by one for presence of FD_CLOEXEC. */
  for (i = 0; i < OPEN_MAX; i++) {
	m_in.fd = i;
	if ( (bitmap >> i) & 01) (void) do_close();
  }

  return(OK);
}

/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
PUBLIC int do_exit()
{
/* Perform the file system portion of the exit(status) system call. */

  register int i, exitee, task;
  register struct fproc *rfp;
  register struct filp *rfilp;
  register struct inode *rip;
  dev_t dev;

  /* Only PM may do the EXIT call directly. */
  if (who != PM_PROC_NR) return(EGENERIC);

  /* Nevertheless, pretend that the call came from the user. */
  fp = &fproc[m_in.slot1];		/* get_filp() needs 'fp' */
  exitee = m_in.slot1;

  if (fp->fp_suspended == SUSPENDED) {
	task = -fp->fp_task;
	if (task == XPIPE || task == XPOPEN) susp_count--;
	m_in.pro = exitee;
	(void) do_unpause();	/* this always succeeds for MM */
	fp->fp_suspended = NOT_SUSPENDED;
  }

  /* Loop on file descriptors, closing any that are open. */
  for (i = 0; i < OPEN_MAX; i++) {
	m_in.fd = i;
	(void) do_close();
  }

  /* Release root and working directories. */
  put_inode(fp->fp_rootdir);
  put_inode(fp->fp_workdir);
  fp->fp_rootdir = NIL_INODE;
  fp->fp_workdir = NIL_INODE;

  /* If a session leader exits then revoke access to its controlling tty from
   * all other processes using it.
   */
  if (!fp->fp_sesldr) {
	fp->fp_pid = PID_FREE;
  	return(OK);		/* not a session leader */
  }
  fp->fp_sesldr = FALSE;
  if (fp->fp_tty == 0) {
	fp->fp_pid = PID_FREE;
  	return(OK);		/* no controlling tty */
  }
  dev = fp->fp_tty;

  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if (rfp->fp_tty == dev) rfp->fp_tty = 0;

	for (i = 0; i < OPEN_MAX; i++) {
		if ((rfilp = rfp->fp_filp[i]) == NIL_FILP) continue;
		if (rfilp->filp_mode == FILP_CLOSED) continue;
		rip = rfilp->filp_ino;
		if ((rip->i_mode & I_TYPE) != I_CHAR_SPECIAL) continue;
		if ((dev_t) rip->i_zone[0] != dev) continue;
		dev_close(dev);
		rfilp->filp_mode = FILP_CLOSED;
	}
  }

  /* Mark slot as free. */
  fp->fp_pid = PID_FREE;
  return(OK);
}

/*===========================================================================*
 *				do_set					     *
 *===========================================================================*/
PUBLIC int do_set()
{
/* Set uid_t or gid_t field. */

  register struct fproc *tfp;

  /* Only PM may make this call directly. */
  if (who != PM_PROC_NR) return(EGENERIC);

  tfp = &fproc[m_in.slot1];
  if (call_nr == SETUID) {
	tfp->fp_realuid = (uid_t) m_in.real_user_id;
	tfp->fp_effuid =  (uid_t) m_in.eff_user_id;
  }
  if (call_nr == SETGID) {
	tfp->fp_effgid =  (gid_t) m_in.eff_grp_id;
	tfp->fp_realgid = (gid_t) m_in.real_grp_id;
  }
  return(OK);
}

/*===========================================================================*
 *				do_revive				     *
 *===========================================================================*/
PUBLIC int do_revive()
{
/* A driver, typically TTY, has now gotten the characters that were needed for 
 * a previous read.  The process did not get a reply when it made the call.
 * Instead it was suspended.  Now we can send the reply to wake it up.  This
 * business has to be done carefully, since the incoming message is from
 * a driver (to which no reply can be sent), and the reply must go to a process
 * that blocked earlier.  The reply to the caller is inhibited by returning the
 * 'SUSPEND' pseudo error, and the reply to the blocked process is done
 * explicitly in revive().
 */

  revive(m_in.REP_PROC_NR, m_in.REP_STATUS);
  return(SUSPEND);		/* don't reply to the TTY task */
}

/*===========================================================================*
 *				do_svrctl				     *
 *===========================================================================*/
PUBLIC int do_svrctl()
{
  switch (m_in.svrctl_req) {
  case FSSIGNON: {
	/* A server in user space calls in to manage a device. */
	struct fssignon device;
	int r, major;

	if (fp->fp_effuid != SU_UID) return(EPERM);

	/* Try to copy request structure to FS. */
	if ((r = sys_datacopy(who, (vir_bytes) m_in.svrctl_argp,
		FS_PROC_NR, (vir_bytes) &device,
		(phys_bytes) sizeof(device))) != OK) 
	    return(r);

	/* Try to update device mapping. */
	major = (device.dev >> MAJOR) & BYTE;
	r=map_driver(major, who, device.style);
	return(r);
  }
  default:
	return(EINVAL);
  }
}
