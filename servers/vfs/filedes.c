/* This file contains the procedures that manipulate file descriptors.
 *
 * The entry points into this file are
 *   get_fd:	 look for free file descriptor and free filp slots
 *   get_filp:	 look up the filp entry for a given file descriptor
 *   find_filp:	 find a filp slot that points to a given vnode
 *   inval_filp: invalidate a filp and associated fd's, only let close()
 *               happen on it
 */

#include <sys/select.h>
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
	if (fp->fp_filp[i] == NIL_FILP && !FD_ISSET(i, &fp->fp_filp_inuse)) {
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
  if (fild < 0 || fild >= OPEN_MAX ) return(NIL_FILP);
  if (rfp->fp_filp[fild] == NIL_FILP && FD_ISSET(fild, &rfp->fp_filp_inuse)) 
	err_code = EIO;	/* The filedes is not there, but is not closed either.
			 */
  
  return(rfp->fp_filp[fild]);	/* may also be NIL_FILP */
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
  return(NIL_FILP);
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
			fproc[f].fp_filp[fd] = NIL_FILP;
			n++;
		}
	}
  }

  return(n);	/* Report back how often this filp has been invalidated. */
}
