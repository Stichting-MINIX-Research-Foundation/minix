/* This file contains a collection of miscellaneous procedures.  Some of them
 * perform simple system calls.  Some others do a little part of system calls
 * that are mostly performed by the Memory Manager.
 *
 * The entry points into this file are
 *   do_dup:	  perform the DUP system call
 *   do_fcntl:	  perform the FCNTL system call
 *   do_sync:	  perform the SYNC system call
 *   do_fsync:	  perform the FSYNC system call
 *   pm_reboot:	  sync disks and prepare for shutdown
 *   pm_fork:	  adjust the tables after MM has performed a FORK system call
 *   do_exec:	  handle files with FD_CLOEXEC on after MM has done an EXEC
 *   do_exit:	  a process has exited; note that in the tables
 *   pm_setgid:	  set group ids for some process
 *   pm_setuid:	  set user ids for some process
 *   do_svrctl:	  file system control
 *   do_getsysinfo:	request copy of FS data structure
 *   pm_dumpcore: create a core dump
 */

#include "fs.h"
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>	/* cc runs out of memory with unistd.h :-( */
#include <minix/callnr.h>
#include <minix/safecopies.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <sys/ptrace.h>
#include <sys/svrctl.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"
#include "super.h"

#define CORE_NAME	"core"
#define CORE_MODE	0777	/* mode to use on core image files */

FORWARD _PROTOTYPE( void free_proc, (struct fproc *freed, int flags));
FORWARD _PROTOTYPE( int dumpcore, (int proc_e, struct mem_map *seg_ptr));
FORWARD _PROTOTYPE( int write_bytes, (struct inode *rip, off_t off,
	char *buf, size_t bytes));
FORWARD _PROTOTYPE( int write_seg, (struct inode *rip, off_t off, int proc_e,
	int seg, off_t seg_off, phys_bytes seg_bytes));

#define FP_EXITING	1

/*===========================================================================*
 *				do_getsysinfo				     *
 *===========================================================================*/
PUBLIC int do_getsysinfo()
{
  struct fproc *proc_addr;
  vir_bytes src_addr, dst_addr;
  size_t len;
  int s;

  if (!super_user)
  {
	printf("FS: unauthorized call of do_getsysinfo by proc %d\n", who_e);
	return(EPERM);	/* only su may call do_getsysinfo. This call may leak
			 * information (and is not stable enough to be part 
			 * of the API/ABI).
			 */
  }

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
  if (OK != (s=sys_datacopy(SELF, src_addr, who_e, dst_addr, len)))
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
  FD_SET(m_in.fd2, &fp->fp_filp_inuse);
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

     case F_FREESP:
     {
	/* Free a section of a file. Preparation is done here,
	 * actual freeing in freesp_inode().
	 */
	off_t start, end;
	struct flock flock_arg;
	signed long offset;

	/* Check if it's a regular file. */
	if((f->filp_ino->i_mode & I_TYPE) != I_REGULAR) {
		return EINVAL;
	}

	/* Copy flock data from userspace. */
	if((r = sys_datacopy(who_e, (vir_bytes) m_in.name1, 
	  SELF, (vir_bytes) &flock_arg,
	  (phys_bytes) sizeof(flock_arg))) != OK)
		return r;

	/* Convert starting offset to signed. */
	offset = (signed long) flock_arg.l_start;

	/* Figure out starting position base. */
	switch(flock_arg.l_whence) {
		case SEEK_SET: start = 0; if(offset < 0) return EINVAL; break;
		case SEEK_CUR: start = f->filp_pos; break;
		case SEEK_END: start = f->filp_ino->i_size; break;
		default: return EINVAL;
	}

	/* Check for overflow or underflow. */
	if(offset > 0 && start + offset < start) { return EINVAL; }
	if(offset < 0 && start + offset > start) { return EINVAL; }
	start += offset;
	if(flock_arg.l_len > 0) {
		end = start + flock_arg.l_len;
		if(end <= start) {
			return EINVAL;
		}
		r = freesp_inode(f->filp_ino, start, end);
	} else {
		r = truncate_inode(f->filp_ino, start);
	}
	return r;
     }

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
 *				pm_reboot				     *
 *===========================================================================*/
PUBLIC void pm_reboot()
{
  /* Perform the FS side of the reboot call. */
  int i;
  struct super_block *sp;
  struct inode dummy;

  /* Do exit processing for all leftover processes and servers,
   * but don't actually exit them (if they were really gone, PM
   * will tell us about it).
   */
  for (i = 0; i < NR_PROCS; i++)
	if((m_in.endpt1 = fproc[i].fp_endpoint) != NONE)
		free_proc(&fproc[i], 0);

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

  /* Sync any unwritten buffers. */
  do_sync();
}

/*===========================================================================*
 *				pm_fork					     *
 *===========================================================================*/
PUBLIC void pm_fork(pproc, cproc, cpid)
int pproc;	/* Parent process */
int cproc;	/* Child process */
int cpid;	/* Child process id */
{
/* Perform those aspects of the fork() system call that relate to files.
 * In particular, let the child inherit its parent's file descriptors.
 * The parent and child parameters tell who forked off whom. The file
 * system uses the same slot numbers as the kernel.
 */

  register struct fproc *cp;
  int i, parentno, childno;

  /* Check up-to-dateness of fproc. */
  okendpt(pproc, &parentno);

  /* PM gives child endpoint, which implies process slot information.
   * Don't call isokendpt, because that will verify if the endpoint
   * number is correct in fproc, which it won't be.
   */
  childno = _ENDPOINT_P(cproc);
  if(childno < 0 || childno >= NR_PROCS)
	panic(__FILE__, "FS: bogus child for forking", m_in.child_endpt);
  if(fproc[childno].fp_pid != PID_FREE)
	panic(__FILE__, "FS: forking on top of in-use child", childno);

  /* Copy the parent's fproc struct to the child. */
  fproc[childno] = fproc[parentno];

  /* Increase the counters in the 'filp' table. */
  cp = &fproc[childno];
  for (i = 0; i < OPEN_MAX; i++)
	if (cp->fp_filp[i] != NIL_FILP) cp->fp_filp[i]->filp_count++;

  /* Fill in new process and endpoint id. */
  cp->fp_pid = cpid;
  cp->fp_endpoint = cproc;

  /* A forking process never has an outstanding grant,
   * as it isn't blocking on i/o.
   */
  assert(!GRANT_VALID(fp->fp_grant));
  assert(!GRANT_VALID(cp->fp_grant));

  /* A child is not a process leader. */
  cp->fp_sesldr = 0;

  /* This child has not exec()ced yet. */
  cp->fp_execced = 0;

  /* Record the fact that both root and working dir have another user. */
  dup_inode(cp->fp_rootdir);
  dup_inode(cp->fp_workdir);
}

/*===========================================================================*
 *				free_proc				     *
 *===========================================================================*/
PRIVATE void free_proc(struct fproc *exiter, int flags)
{
  int i, task;
  register struct fproc *rfp;
  register struct filp *rfilp;
  register struct inode *rip;
  dev_t dev;

  fp = exiter;		/* get_filp() needs 'fp' */

  if (fp->fp_suspended == SUSPENDED) {
	task = -fp->fp_task;
	if (task == XPIPE || task == XPOPEN) susp_count--;
	unpause(fp->fp_endpoint);
	fp->fp_suspended = NOT_SUSPENDED;
  }

  /* Loop on file descriptors, closing any that are open. */
  for (i = 0; i < OPEN_MAX; i++) {
	(void) close_fd(fp, i);
  }

  /* Release root and working directories. */
  put_inode(fp->fp_rootdir);
  put_inode(fp->fp_workdir);
  fp->fp_rootdir = NIL_INODE;
  fp->fp_workdir = NIL_INODE;

  /* Check if any process is SUSPENDed on this driver.
   * If a driver exits, unmap its entries in the dmap table.
   * (unmapping has to be done after the first step, because the
   * dmap table is used in the first step.)
   */
  unsuspend_by_endpt(fp->fp_endpoint);

  /* The rest of these actions is only done when processes actually
   * exit.
   */
  if(!(flags & FP_EXITING))
	return;

  /* Invalidate endpoint number for error and sanity checks. */
  fp->fp_endpoint = NONE;

  /* If a session leader exits and it has a controlling tty, then revoke 
   * access to its controlling tty from all other processes using it.
   */
  if (fp->fp_sesldr && fp->fp_tty != 0) {

      dev = fp->fp_tty;

      for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	  if(rfp->fp_pid == PID_FREE) continue;
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
  }

  /* Exit done. Mark slot as free. */
  fp->fp_pid = PID_FREE;
}

/*===========================================================================*
 *				pm_exit					     *
 *===========================================================================*/
PUBLIC void pm_exit(proc)
int proc;
{
  int exitee_p;
/* Perform the file system portion of the exit(status) system call. */

  /* Nevertheless, pretend that the call came from the user. */
  okendpt(proc, &exitee_p);
  free_proc(&fproc[exitee_p], FP_EXITING);
}

/*===========================================================================*
 *				pm_setgid				     *
 *===========================================================================*/
PUBLIC void pm_setgid(proc_e, egid, rgid)
int proc_e;
int egid;
int rgid;
{
  register struct fproc *tfp;
  int slot;

  okendpt(proc_e, &slot);
  tfp = &fproc[slot];

  tfp->fp_effgid =  egid;
  tfp->fp_realgid = rgid;
}


/*===========================================================================*
 *				pm_setuid				     *
 *===========================================================================*/
PUBLIC void pm_setuid(proc_e, euid, ruid)
int proc_e;
int euid;
int ruid;
{
  register struct fproc *tfp;
  int slot;

  okendpt(proc_e, &slot);
  tfp = &fproc[slot];

  tfp->fp_effuid =  euid;
  tfp->fp_realuid = ruid;
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
	int r, major, proc_nr_n;

	if (fp->fp_effuid != SU_UID && fp->fp_effuid != SERVERS_UID)
		return(EPERM);

	/* Try to copy request structure to FS. */
	if ((r = sys_datacopy(who_e, (vir_bytes) m_in.svrctl_argp,
		FS_PROC_NR, (vir_bytes) &device,
		(phys_bytes) sizeof(device))) != OK) 
	    return(r);

	if (isokendpt(who_e, &proc_nr_n) != OK)
		return(EINVAL);

	/* Try to update device mapping. */
	major = (device.dev >> MAJOR) & BYTE;
	r=map_driver(major, who_e, device.style, 0 /* !force */);
	if (r == OK)
	{
		/* If a driver has completed its exec(), it can be announced
		 * to be up.
		*/
		if(fproc[proc_nr_n].fp_execced) {
			dev_up(major);
		} else {
			dmap[major].dmap_flags |= DMAP_BABY;
		}
	}

	return(r);
  }
  case FSDEVUNMAP: {
	struct fsdevunmap fdu;
	int r, major;
	/* Try to copy request structure to FS. */
	if ((r = sys_datacopy(who_e, (vir_bytes) m_in.svrctl_argp,
		FS_PROC_NR, (vir_bytes) &fdu,
		(phys_bytes) sizeof(fdu))) != OK) 
	    return(r);
	major = (fdu.dev >> MAJOR) & BYTE;
	r=map_driver(major, NONE, 0, 0);
	return(r);
  }
  default:
	return(EINVAL);
  }
}


/*===========================================================================*
 *				pm_dumpcore				     *
 *===========================================================================*/
PUBLIC int pm_dumpcore(proc_e, seg_ptr)
int proc_e;
struct mem_map *seg_ptr;
{
	int r, proc_s;

	r= dumpcore(proc_e, seg_ptr);

	/* Terminate the process */
	okendpt(proc_e, &proc_s);
	free_proc(&fproc[proc_s], FP_EXITING);

	return r;
}

/*===========================================================================*
 *				dumpcore				     *
 *===========================================================================*/
PRIVATE int dumpcore(proc_e, seg_ptr)
int proc_e;
struct mem_map *seg_ptr;
{
	int r, seg, proc_s, exists;
	mode_t omode;
	vir_bytes len;
	off_t off, seg_off;
	long trace_off, trace_data;
	struct fproc *rfp;
	struct inode *rip, *ldirp;
	struct mem_map segs[NR_LOCAL_SEGS];

	okendpt(proc_e, &proc_s);
	rfp= fp= &fproc[proc_s];
	who_e= proc_e;
	who_p= proc_s;
	super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */

	/* We need the equivalent of
	 * open(CORE_NAME, O_WRONLY|O_CREAT|O_TRUNC|O_NONBLOCK, CORE_MODE)
	 */

  	/* Create a new inode by calling new_node(). */
        omode = I_REGULAR | (CORE_MODE & ALL_MODES & rfp->fp_umask);
    	rip = new_node(&ldirp, CORE_NAME, omode, NO_ZONE, 0, NULL);
    	r = err_code;
        put_inode(ldirp);
	exists= (r == EEXIST);
    	if (r != OK && r != EEXIST) return(r); /* error */

	/* Only do the normal open code if we didn't just create the file. */
	if (exists) {
		/* Check protections. */
		r = forbidden(rip, W_BIT);
		if (r != OK)
		{
			put_inode(rip);
			return r;
		}

		/* Make sure it is a regular file */
		switch (rip->i_mode & I_TYPE) {
		   case I_REGULAR: 
			break;
 
		   case I_DIRECTORY: 
			/* Directories may be read but not written. */
			r = EISDIR;
			break;

		   case I_CHAR_SPECIAL:
		   case I_BLOCK_SPECIAL:
		   case I_NAMED_PIPE:
			r = EPERM;
			break;
		}

		if (r != OK)
		{
			put_inode(rip);
			return r;
		}

		/* Truncate the file */
		truncate_inode(rip, 0);
		wipe_inode(rip);
		/* Send the inode from the inode cache to the
		 * block cache, so it gets written on the next
		 * cache flush.
		 */
		rw_inode(rip, WRITING);
	}

	/* Copy segments from PM */
	r= sys_datacopy(PM_PROC_NR, (vir_bytes)seg_ptr,
		SELF, (vir_bytes)segs, sizeof(segs));
	if (r != OK) panic(__FILE__, "dumpcore: cannot copy segment info", r);

	off= 0;
	r= write_bytes(rip, off, (char *)segs, sizeof(segs));
	if (r != OK)
	{
		put_inode(rip);
		return r;
	}
	off += sizeof(segs);

	/* Write out the whole kernel process table entry to get the regs. */
	for (trace_off= 0;; trace_off += sizeof(long))
	{
		r= sys_trace(T_GETUSER, proc_e, trace_off, &trace_data);
		if  (r != OK) 
		{
			printf("dumpcore pid %d: sys_trace failed "
				"at offset %d: %d\n",
				rfp->fp_pid, trace_off, r);
			break;
		}
		r= write_bytes(rip, off, (char *)&trace_data,
			sizeof(trace_data));
		if (r != OK)
		{
			put_inode(rip);
			return r;
		}
		off += sizeof(trace_data);
	}

	/* Loop through segments and write the segments themselves out. */
	for (seg = 0; seg < NR_LOCAL_SEGS; seg++) {
		len= segs[seg].mem_len << CLICK_SHIFT;
		seg_off= segs[seg].mem_vir << CLICK_SHIFT;
		r= write_seg(rip, off, proc_e, seg, seg_off, len);
		if (r != OK)
		{
			put_inode(rip);
			return r;
		}
		off += len;
	}

	rip->i_size= off;
	rip->i_dirt = DIRTY;

	put_inode(rip);
	return OK;
}


/*===========================================================================*
 *				write_bytes				     *
 *===========================================================================*/
PRIVATE int write_bytes(rip, off, buf, bytes)
struct inode *rip; 		/* inode descriptor to read from */
off_t off;			/* offset in file */
char *buf;
size_t bytes;			/* how much is to be transferred? */
{
	int r, block_size;
	off_t n, o, b_off;
	block_t b;
	struct buf *bp;

	block_size= rip->i_sp->s_block_size;
	for (o= off - (off % block_size); o < off+bytes; o += block_size)
	{
		if (o < off)
			b_off= off-o;
		else
			b_off= 0;
		n= block_size-b_off;
		if (o+b_off+n > off+bytes)
			n= off+bytes-(o+b_off);

		b = read_map(rip, o);

		if (b == NO_BLOCK) {
			/* Writing to a nonexistent block. Create and enter
			 * in inode.
			 */
			if ((bp= new_block(rip, o)) == NIL_BUF)
				return(err_code);
		} 
		else
		{
			/* Just read the block, no need to optimize for
			 * writing entire blocks.
			 */
			bp = get_block(rip->i_dev, b, NORMAL);
		}

		if (n != block_size && o >= rip->i_size && b_off == 0) {
			zero_block(bp);
		}

		/* Copy a chunk from user space to the block buffer. */
		memcpy((bp->b_data+b_off), buf, n);
		bp->b_dirt = DIRTY;
		if (b_off + n == block_size)
			put_block(bp, FULL_DATA_BLOCK);
		else
			put_block(bp, PARTIAL_DATA_BLOCK);

		buf += n;
	}

	return OK;
}

/*===========================================================================*
 *				write_seg				     *
 *===========================================================================*/
PRIVATE int write_seg(rip, off, proc_e, seg, seg_off, seg_bytes)
struct inode *rip; 		/* inode descriptor to read from */
off_t off;			/* offset in file */
int proc_e;			/* process number (endpoint) */
int seg;			/* T, D, or S */
off_t seg_off;			/* Offset in segment */
phys_bytes seg_bytes;		/* how much is to be transferred? */
{
  int r, block_size, fl;
  off_t n, o, b_off;
  block_t b;
  struct buf *bp;

  block_size= rip->i_sp->s_block_size;
  for (o= off - (off % block_size); o < off+seg_bytes; o += block_size)
  {
	if (o < off)
		b_off= off-o;
	else
		b_off= 0;
	n= block_size-b_off;
	if (o+b_off+n > off+seg_bytes)
		n= off+seg_bytes-(o+b_off);

	b = read_map(rip, o);
	if (b == NO_BLOCK) {
		/* Writing to a nonexistent block. Create and enter in inode.*/
		if ((bp= new_block(rip, o)) == NIL_BUF)
			return(err_code);
	} else {
		/* Normally an existing block to be partially overwritten is
		 * first read in.  However, a full block need not be read in.
		 * If it is already in the cache, acquire it, otherwise just
		 * acquire a free buffer.
		 */
		fl = (n == block_size ? NO_READ : NORMAL);
		bp = get_block(rip->i_dev, b, fl);
	}

	if (n != block_size && o >= rip->i_size && b_off == 0) {
		zero_block(bp);
	}

	/* Copy a chunk from user space to the block buffer. */
	r = sys_vircopy(proc_e, seg, (phys_bytes) seg_off,
			FS_PROC_NR, D, (phys_bytes) (bp->b_data+b_off),
			(phys_bytes) n);
	bp->b_dirt = DIRTY;
	fl = (b_off + n == block_size ? FULL_DATA_BLOCK : PARTIAL_DATA_BLOCK);
	put_block(bp, fl);

	seg_off += n;
  }

  return OK;
}


