/* This file contains the procedures that manipulate file descriptors.
 *
 * The entry points into this file are
 *   get_fd:	    look for free file descriptor and free filp slots
 *   get_filp:	    look up the filp entry for a given file descriptor
 *   find_filp:	    find a filp slot that points to a given vnode
 *   inval_filp:    invalidate a filp and associated fd's, only let close()
 *                  happen on it
 *   do_verify_fd:  verify whether the given file descriptor is valid for 
 *                  the given endpoint.
 *   do_set_filp:   marks a filp as in-flight.
 *   do_copy_filp:  copies a filp to another endpoint.
 *   do_put_filp:   marks a filp as not in-flight anymore.
 *   do_cancel_fd:  cancel the transaction when something goes wrong for 
 *                  the receiver.
 */

#include <sys/select.h>
#include <minix/callnr.h>
#include <minix/u64.h>
#include <assert.h>
#include "fs.h"
#include "file.h"
#include "fproc.h"
#include "vnode.h"

/*===========================================================================*
 *				get_fd					     *
 *===========================================================================*/
PUBLIC int get_fd(int start, mode_t bits, int *k, struct filp **fpt)
{
/* Look for a free file descriptor and a free filp slot.  Fill in the mode word
 * in the latter, but don't claim either one yet, since the open() or creat()
 * may yet fail.
 */

  register struct filp *f;
  register int i;

  /* Search the fproc fp_filp table for a free file descriptor. */
  for (i = start; i < OPEN_MAX; i++) {
	if (fp->fp_filp[i] == NULL && !FD_ISSET(i, &fp->fp_filp_inuse)) {
		/* A file descriptor has been located. */
		*k = i;
		break;
	}
  }

  /* Check to see if a file descriptor has been found. */
  if (i >= OPEN_MAX) return(EMFILE);

  /* Now that a file descriptor has been found, look for a free filp slot. */
  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	assert(f->filp_count >= 0);
	if (f->filp_count == 0) {
		f->filp_mode = bits;
		f->filp_pos = cvu64(0);
		f->filp_selectors = 0;
		f->filp_select_ops = 0;
		f->filp_pipe_select_ops = 0;
		f->filp_flags = 0;
		f->filp_state = FS_NORMAL;
		f->filp_select_flags = 0;
		*fpt = f;
		return(OK);
	}
  }

  /* If control passes here, the filp table must be full.  Report that back. */
  return(ENFILE);
}


/*===========================================================================*
 *				get_filp				     *
 *===========================================================================*/
PUBLIC struct filp *get_filp(fild)
int fild;			/* file descriptor */
{
/* See if 'fild' refers to a valid file descr.  If so, return its filp ptr. */

  return get_filp2(fp, fild);
}


/*===========================================================================*
 *				get_filp2				     *
 *===========================================================================*/
PUBLIC struct filp *get_filp2(rfp, fild)
register struct fproc *rfp;
int fild;			/* file descriptor */
{
/* See if 'fild' refers to a valid file descr.  If so, return its filp ptr. */

  err_code = EBADF;
  if (fild < 0 || fild >= OPEN_MAX ) return(NULL);
  if (rfp->fp_filp[fild] == NULL && FD_ISSET(fild, &rfp->fp_filp_inuse)) 
	err_code = EIO;	/* The filedes is not there, but is not closed either.
			 */
  
  return(rfp->fp_filp[fild]);	/* may also be NULL */
}


/*===========================================================================*
 *				find_filp				     *
 *===========================================================================*/
PUBLIC struct filp *find_filp(register struct vnode *vp, mode_t bits)
{
/* Find a filp slot that refers to the vnode 'vp' in a way as described
 * by the mode bit 'bits'. Used for determining whether somebody is still
 * interested in either end of a pipe.  Also used when opening a FIFO to
 * find partners to share a filp field with (to shared the file position).
 * Like 'get_fd' it performs its job by linear search through the filp table.
 */

  register struct filp *f;

  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	if (f->filp_count != 0 && f->filp_vno == vp && (f->filp_mode & bits)){
		return(f);
	}
  }

  /* If control passes here, the filp wasn't there.  Report that back. */
  return(NULL);
}

/*===========================================================================*
 *				invalidate				     *
 *===========================================================================*/
PUBLIC int invalidate(struct filp *fp)
{
/* Invalidate filp. fp_filp_inuse is not cleared, so filp can't be reused
   until it is closed first. */

  int f, fd, n = 0;
  for(f = 0; f < NR_PROCS; f++) {
	if(fproc[f].fp_pid == PID_FREE) continue;
	for(fd = 0; fd < OPEN_MAX; fd++) {
		if(fproc[f].fp_filp[fd] && fproc[f].fp_filp[fd] == fp) {
			fproc[f].fp_filp[fd] = NULL;
			n++;
		}
	}
  }

  return(n);	/* Report back how often this filp has been invalidated. */
}

/*===========================================================================*
 *                              verify_fd                                    *
 *===========================================================================*/
PUBLIC filp_id_t verify_fd(ep, fd)
endpoint_t ep;
int fd;
{
  /*
   * verify whether the given file descriptor 'fd' is valid for the
   * endpoint 'ep'. When the file descriptor is valid verify_fd returns a 
   * pointer to that filp, else it returns NULL.
   */
  int proc;

  if (isokendpt(ep, &proc) != OK) {
	return NULL;
  }

  return get_filp2(&fproc[proc], fd);
}

/*===========================================================================*
 *                              do_verify_fd                                 *
 *===========================================================================*/
PUBLIC int do_verify_fd(void)
{
  m_out.ADDRESS = (void *) verify_fd(m_in.IO_ENDPT, m_in.COUNT);
  return (m_out.ADDRESS != NULL) ? OK : EINVAL;
}

/*===========================================================================*
 *                              set_filp                                     *
 *===========================================================================*/
PUBLIC int set_filp(sfilp)
filp_id_t sfilp;
{
  if (sfilp == NULL) {
	return EINVAL;
  } else {
	sfilp->filp_count++;
	return OK;
  }
}

/*===========================================================================*
 *                              do_set_filp                                  *
 *===========================================================================*/
PUBLIC int do_set_filp(void)
{
  return set_filp((filp_id_t) m_in.ADDRESS);;
}

/*===========================================================================*
 *                              copy_filp                                    *
 *===========================================================================*/
PUBLIC int copy_filp(to_ep, cfilp)
endpoint_t to_ep;
filp_id_t cfilp;
{
  int j;
  int proc;

  if (isokendpt(to_ep, &proc) != OK) {
	return EINVAL;
  }

  /* Find an open slot in fp_filp */
  for (j = 0; j < OPEN_MAX; j++) {
	if (fproc[proc].fp_filp[j] == NULL && 
			!FD_ISSET(j, &fproc[proc].fp_filp_inuse)) {

		/* Found a free slot, add descriptor */
		FD_SET(j, &fproc[proc].fp_filp_inuse);
		fproc[proc].fp_filp[j] = cfilp;
		fproc[proc].fp_filp[j]->filp_count++;
		return j;
	}
  }

  /* File Descriptor Table is Full */
  return EMFILE;
}

/*===========================================================================*
 *                              do_copy_filp                                 *
 *===========================================================================*/
PUBLIC int do_copy_filp(void)
{
  return copy_filp(m_in.IO_ENDPT, (filp_id_t) m_in.ADDRESS);
}

/*===========================================================================*
 *                              put_filp                                     *
 *===========================================================================*/
PUBLIC int put_filp(pfilp)
filp_id_t pfilp;
{
  if (pfilp == NULL) {
	return EINVAL;
  } else {
	close_filp(pfilp);
	return OK;
  }
}

/*===========================================================================*
 *                              do_put_filp                                  *
 *===========================================================================*/
PUBLIC int do_put_filp(void)
{
  return put_filp((filp_id_t) m_in.ADDRESS);
}

/*===========================================================================*
 *                             cancel_fd                                     *
 *===========================================================================*/
PUBLIC int cancel_fd(ep, fd)
endpoint_t ep;
int fd;
{
  int j;
  int proc;

  if (isokendpt(ep, &proc) != OK) {
	return EINVAL;
  }

  /* Check that the input 'fd' is valid */
  if (verify_fd(ep, fd) != NULL) {

	/* Found a valid descriptor, remove it */
	FD_CLR(fd, &fproc[proc].fp_filp_inuse);
	fproc[proc].fp_filp[fd]->filp_count--;
	fproc[proc].fp_filp[fd] = NULL;

	return fd;
  }

  /* File descriptor is not valid for the endpoint. */
  return EINVAL;
}

/*===========================================================================*
 *                              do_cancel_fd                                 *
 *===========================================================================*/
PUBLIC int do_cancel_fd(void)
{
  return cancel_fd(m_in.IO_ENDPT, m_in.COUNT);
}

/*===========================================================================*
 *				close_filp				     *
 *===========================================================================*/
PUBLIC void close_filp(fp)
struct filp *fp;
{
  int mode_word, rw;
  dev_t dev;
  struct vnode *vp;

  vp = fp->filp_vno;
  if (fp->filp_count - 1 == 0 && fp->filp_mode != FILP_CLOSED) {
	/* Check to see if the file is special. */
	mode_word = vp->v_mode & I_TYPE;
	if (mode_word == I_CHAR_SPECIAL || mode_word == I_BLOCK_SPECIAL) {
		dev = (dev_t) vp->v_sdev;
		if (mode_word == I_BLOCK_SPECIAL)  {
			if (vp->v_bfs_e == ROOT_FS_E) {
				/* Invalidate the cache unless the special is
				 * mounted. Assume that the root filesystem's
				 * is open only for fsck.
			 	 */
          			req_flush(vp->v_bfs_e, dev);
          		}
		}
		/* Do any special processing on device close. */
		(void) dev_close(dev, fp-filp);
		/* Ignore any errors, even SUSPEND. */

		fp->filp_mode = FILP_CLOSED;
	}
  }

  /* If the inode being closed is a pipe, release everyone hanging on it. */
  if (vp->v_pipe == I_PIPE) {
	rw = (fp->filp_mode & R_BIT ? WRITE : READ);
	release(vp, rw, NR_PROCS);
  }

  /* If a write has been done, the inode is already marked as DIRTY. */
  if (--fp->filp_count == 0) {
	if (vp->v_pipe == I_PIPE) {
		/* Last reader or writer is going. Tell PFS about latest
		 * pipe size.
		 */
		truncate_vnode(vp, vp->v_size);
	}
		
	put_vnode(fp->filp_vno);
  }
}
