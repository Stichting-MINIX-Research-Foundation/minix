/* This file contains the procedures for creating, opening, closing, and
 * seeking on files.
 *
 * The entry points into this file are
 *   do_creat:	perform the CREAT system call
 *   do_open:	perform the OPEN system call
 *   do_mknod:	perform the MKNOD system call
 *   do_mkdir:	perform the MKDIR system call
 *   do_close:	perform the CLOSE system call
 *   do_lseek:  perform the LSEEK system call
 */

#include "fs.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "lock.h"
#include "param.h"
#include "super.h"

#define offset m2_l1

PRIVATE char mode_map[] = {R_BIT, W_BIT, R_BIT|W_BIT, 0};

FORWARD _PROTOTYPE( int common_open, (int oflags, mode_t omode)		);
FORWARD _PROTOTYPE( int pipe_open, (struct inode *rip,mode_t bits,int oflags));
FORWARD _PROTOTYPE( struct inode *new_node, (char *path, mode_t bits,
							zone_t z0)	);

/*===========================================================================*
 *				do_creat				     *
 *===========================================================================*/
PUBLIC int do_creat()
{
/* Perform the creat(name, mode) system call. */
  int r;

  if (fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);
  r = common_open(O_WRONLY | O_CREAT | O_TRUNC, (mode_t) m_in.mode);
  return(r);
}

/*===========================================================================*
 *				do_open					     *
 *===========================================================================*/
PUBLIC int do_open()
{
/* Perform the open(name, flags,...) system call. */

  int create_mode = 0;		/* is really mode_t but this gives problems */
  int r;

  /* If O_CREAT is set, open has three parameters, otherwise two. */
  if (m_in.mode & O_CREAT) {
	create_mode = m_in.c_mode;	
	r = fetch_name(m_in.c_name, m_in.name1_length, M1);
  } else {
	r = fetch_name(m_in.name, m_in.name_length, M3);
  }

  if (r != OK) return(err_code); /* name was bad */
  r = common_open(m_in.mode, create_mode);
  return(r);
}

/*===========================================================================*
 *				common_open				     *
 *===========================================================================*/
PRIVATE int common_open(register int oflags, mode_t omode)
{
/* Common code from do_creat and do_open. */

  register struct inode *rip;
  int r, b, exist = TRUE;
  dev_t dev;
  mode_t bits;
  off_t pos;
  struct filp *fil_ptr, *filp2;

  /* Remap the bottom two bits of oflags. */
  bits = (mode_t) mode_map[oflags & O_ACCMODE];

  /* See if file descriptor and filp slots are available. */
  if ( (r = get_fd(0, bits, &m_in.fd, &fil_ptr)) != OK) return(r);

  /* If O_CREATE is set, try to make the file. */ 
  if (oflags & O_CREAT) {
  	/* Create a new inode by calling new_node(). */
        omode = I_REGULAR | (omode & ALL_MODES & fp->fp_umask);
    	rip = new_node(user_path, omode, NO_ZONE);
    	r = err_code;
    	if (r == OK) exist = FALSE;      /* we just created the file */
	else if (r != EEXIST) return(r); /* other error */
	else exist = !(oflags & O_EXCL); /* file exists, if the O_EXCL 
					    flag is set this is an error */
  } else {
	 /* Scan path name. */
    	if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);
  }

  /* Claim the file descriptor and filp slot and fill them in. */
  fp->fp_filp[m_in.fd] = fil_ptr;
  fil_ptr->filp_count = 1;
  fil_ptr->filp_ino = rip;
  fil_ptr->filp_flags = oflags;

  /* Only do the normal open code if we didn't just create the file. */
  if (exist) {
  	/* Check protections. */
  	if ((r = forbidden(rip, bits)) == OK) {
  		/* Opening reg. files directories and special files differ. */
	  	switch (rip->i_mode & I_TYPE) {
    		   case I_REGULAR: 
			/* Truncate regular file if O_TRUNC. */
			if (oflags & O_TRUNC) {
				if ((r = forbidden(rip, W_BIT)) !=OK) break;
				truncate(rip);
				wipe_inode(rip);
				/* Send the inode from the inode cache to the
				 * block cache, so it gets written on the next
				 * cache flush.
				 */
				rw_inode(rip, WRITING);
			}
			break;
 
	    	   case I_DIRECTORY: 
			/* Directories may be read but not written. */
			r = (bits & W_BIT ? EISDIR : OK);
			break;

	     	   case I_CHAR_SPECIAL:
     		   case I_BLOCK_SPECIAL:
			/* Invoke the driver for special processing. */
			dev = (dev_t) rip->i_zone[0];
			r = dev_open(dev, who, bits | (oflags & ~O_ACCMODE));
			break;

		   case I_NAMED_PIPE:
			oflags |= O_APPEND;	/* force append mode */
			fil_ptr->filp_flags = oflags;
			r = pipe_open(rip, bits, oflags);
			if (r != ENXIO) {
				/* See if someone else is doing a rd or wt on
				 * the FIFO.  If so, use its filp entry so the
				 * file position will be automatically shared.
				 */
				b = (bits & R_BIT ? R_BIT : W_BIT);
				fil_ptr->filp_count = 0; /* don't find self */
				if ((filp2 = find_filp(rip, b)) != NIL_FILP) {
					/* Co-reader or writer found. Use it.*/
					fp->fp_filp[m_in.fd] = filp2;
					filp2->filp_count++;
					filp2->filp_ino = rip;
					filp2->filp_flags = oflags;

					/* i_count was incremented incorrectly
					 * by eatpath above, not knowing that
					 * we were going to use an existing
					 * filp entry.  Correct this error.
					 */
					rip->i_count--;
				} else {
					/* Nobody else found.  Restore filp. */
					fil_ptr->filp_count = 1;
					if (b == R_BIT)
					     pos = rip->i_zone[V2_NR_DZONES+0];
					else
					     pos = rip->i_zone[V2_NR_DZONES+1];
					fil_ptr->filp_pos = pos;
				}
			}
			break;
 		}
  	}
  }

  /* If error, release inode. */
  if (r != OK) {
	if (r == SUSPEND) return(r);		/* Oops, just suspended */
	fp->fp_filp[m_in.fd] = NIL_FILP;
	fil_ptr->filp_count= 0;
	put_inode(rip);
	return(r);
  }
  
  return(m_in.fd);
}

/*===========================================================================*
 *				new_node				     *
 *===========================================================================*/
PRIVATE struct inode *new_node(char *path, mode_t bits,	zone_t z0)
{
/* New_node() is called by common_open(), do_mknod(), and do_mkdir().  
 * In all cases it allocates a new inode, makes a directory entry for it on 
 * the path 'path', and initializes it.  It returns a pointer to the inode if 
 * it can do this; otherwise it returns NIL_INODE.  It always sets 'err_code'
 * to an appropriate value (OK or an error code).
 */

  register struct inode *rlast_dir_ptr, *rip;
  register int r;
  char string[NAME_MAX];

  /* See if the path can be opened down to the last directory. */
  if ((rlast_dir_ptr = last_dir(path, string)) == NIL_INODE) return(NIL_INODE);

  /* The final directory is accessible. Get final component of the path. */
  rip = advance(rlast_dir_ptr, string);
  if ( rip == NIL_INODE && err_code == ENOENT) {
	/* Last path component does not exist.  Make new directory entry. */
	if ( (rip = alloc_inode(rlast_dir_ptr->i_dev, bits)) == NIL_INODE) {
		/* Can't creat new inode: out of inodes. */
		put_inode(rlast_dir_ptr);
		return(NIL_INODE);
	}

	/* Force inode to the disk before making directory entry to make
	 * the system more robust in the face of a crash: an inode with
	 * no directory entry is much better than the opposite.
	 */
	rip->i_nlinks++;
	rip->i_zone[0] = z0;		/* major/minor device numbers */
	rw_inode(rip, WRITING);		/* force inode to disk now */

	/* New inode acquired.  Try to make directory entry. */
	if ((r = search_dir(rlast_dir_ptr, string, &rip->i_num,ENTER)) != OK) {
		put_inode(rlast_dir_ptr);
		rip->i_nlinks--;	/* pity, have to free disk inode */
		rip->i_dirt = DIRTY;	/* dirty inodes are written out */
		put_inode(rip);	/* this call frees the inode */
		err_code = r;
		return(NIL_INODE);
	}

  } else {
	/* Either last component exists, or there is some problem. */
	if (rip != NIL_INODE)
		r = EEXIST;
	else
		r = err_code;
  }

  /* Return the directory inode and exit. */
  put_inode(rlast_dir_ptr);
  err_code = r;
  return(rip);
}

/*===========================================================================*
 *				pipe_open				     *
 *===========================================================================*/
PRIVATE int pipe_open(register struct inode *rip, register mode_t bits,
	register int oflags)
{
/*  This function is called from common_open. It checks if
 *  there is at least one reader/writer pair for the pipe, if not
 *  it suspends the caller, otherwise it revives all other blocked
 *  processes hanging on the pipe.
 */

  rip->i_pipe = I_PIPE; 
  if (find_filp(rip, bits & W_BIT ? R_BIT : W_BIT) == NIL_FILP) { 
	if (oflags & O_NONBLOCK) {
		if (bits & W_BIT) return(ENXIO);
	} else {
		suspend(XPOPEN);	/* suspend caller */
		return(SUSPEND);
	}
  } else if (susp_count > 0) {/* revive blocked processes */
	release(rip, OPEN, susp_count);
	release(rip, CREAT, susp_count);
  }
  return(OK);
}

/*===========================================================================*
 *				do_mknod				     *
 *===========================================================================*/
PUBLIC int do_mknod()
{
/* Perform the mknod(name, mode, addr) system call. */

  register mode_t bits, mode_bits;
  struct inode *ip;

  /* Only the super_user may make nodes other than fifos. */
  mode_bits = (mode_t) m_in.mk_mode;		/* mode of the inode */
  if (!super_user && ((mode_bits & I_TYPE) != I_NAMED_PIPE)) return(EPERM);
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  bits = (mode_bits & I_TYPE) | (mode_bits & ALL_MODES & fp->fp_umask);
  ip = new_node(user_path, bits, (zone_t) m_in.mk_z0);
  put_inode(ip);
  return(err_code);
}

/*===========================================================================*
 *				do_mkdir				     *
 *===========================================================================*/
PUBLIC int do_mkdir()
{
/* Perform the mkdir(name, mode) system call. */

  int r1, r2;			/* status codes */
  ino_t dot, dotdot;		/* inode numbers for . and .. */
  mode_t bits;			/* mode bits for the new inode */
  char string[NAME_MAX];	/* last component of the new dir's path name */
  register struct inode *rip, *ldirp;

  /* Check to see if it is possible to make another link in the parent dir. */
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  ldirp = last_dir(user_path, string);	/* pointer to new dir's parent */
  if (ldirp == NIL_INODE) return(err_code);
  if (ldirp->i_nlinks >= (ldirp->i_sp->s_version == V1 ?
  	 CHAR_MAX : SHRT_MAX)) {
	put_inode(ldirp);	/* return parent */
	return(EMLINK);
  }

  /* Next make the inode. If that fails, return error code. */
  bits = I_DIRECTORY | (m_in.mode & RWX_MODES & fp->fp_umask);
  rip = new_node(user_path, bits, (zone_t) 0);
  if (rip == NIL_INODE || err_code == EEXIST) {
	put_inode(rip);		/* can't make dir: it already exists */
	put_inode(ldirp);	/* return parent too */
	return(err_code);
  }

  /* Get the inode numbers for . and .. to enter in the directory. */
  dotdot = ldirp->i_num;	/* parent's inode number */
  dot = rip->i_num;		/* inode number of the new dir itself */

  /* Now make dir entries for . and .. unless the disk is completely full. */
  /* Use dot1 and dot2, so the mode of the directory isn't important. */
  rip->i_mode = bits;	/* set mode */
  r1 = search_dir(rip, dot1, &dot, ENTER);	/* enter . in the new dir */
  r2 = search_dir(rip, dot2, &dotdot, ENTER);	/* enter .. in the new dir */

  /* If both . and .. were successfully entered, increment the link counts. */
  if (r1 == OK && r2 == OK) {
	/* Normal case.  It was possible to enter . and .. in the new dir. */
	rip->i_nlinks++;	/* this accounts for . */
	ldirp->i_nlinks++;	/* this accounts for .. */
	ldirp->i_dirt = DIRTY;	/* mark parent's inode as dirty */
  } else {
	/* It was not possible to enter . or .. probably disk was full. */
	(void) search_dir(ldirp, string, (ino_t *) 0, DELETE);
	rip->i_nlinks--;	/* undo the increment done in new_node() */
  }
  rip->i_dirt = DIRTY;		/* either way, i_nlinks has changed */

  put_inode(ldirp);		/* return the inode of the parent dir */
  put_inode(rip);		/* return the inode of the newly made dir */
  return(err_code);		/* new_node() always sets 'err_code' */
}

/*===========================================================================*
 *				do_close				     *
 *===========================================================================*/
PUBLIC int do_close()
{
/* Perform the close(fd) system call. */

  register struct filp *rfilp;
  register struct inode *rip;
  struct file_lock *flp;
  int rw, mode_word, lock_count;
  dev_t dev;

  /* First locate the inode that belongs to the file descriptor. */
  if ( (rfilp = get_filp(m_in.fd)) == NIL_FILP) return(err_code);
  rip = rfilp->filp_ino;	/* 'rip' points to the inode */

  if (rfilp->filp_count - 1 == 0 && rfilp->filp_mode != FILP_CLOSED) {
	/* Check to see if the file is special. */
	mode_word = rip->i_mode & I_TYPE;
	if (mode_word == I_CHAR_SPECIAL || mode_word == I_BLOCK_SPECIAL) {
		dev = (dev_t) rip->i_zone[0];
		if (mode_word == I_BLOCK_SPECIAL)  {
			/* Invalidate cache entries unless special is mounted
			 * or ROOT
			 */
			if (!mounted(rip)) {
			        (void) do_sync();	/* purge cache */
				invalidate(dev);
			}    
		}
		/* Do any special processing on device close. */
		dev_close(dev);
	}
  }

  /* If the inode being closed is a pipe, release everyone hanging on it. */
  if (rip->i_pipe == I_PIPE) {
	rw = (rfilp->filp_mode & R_BIT ? WRITE : READ);
	release(rip, rw, NR_PROCS);
  }

  /* If a write has been done, the inode is already marked as DIRTY. */
  if (--rfilp->filp_count == 0) {
	if (rip->i_pipe == I_PIPE && rip->i_count > 1) {
		/* Save the file position in the i-node in case needed later.
		 * The read and write positions are saved separately.  The
		 * last 3 zones in the i-node are not used for (named) pipes.
		 */
		if (rfilp->filp_mode == R_BIT)
			rip->i_zone[V2_NR_DZONES+0] = (zone_t) rfilp->filp_pos;
		else
			rip->i_zone[V2_NR_DZONES+1] = (zone_t) rfilp->filp_pos;
	}
	put_inode(rip);
  }

  fp->fp_cloexec &= ~(1L << m_in.fd);	/* turn off close-on-exec bit */
  fp->fp_filp[m_in.fd] = NIL_FILP;

  /* Check to see if the file is locked.  If so, release all locks. */
  if (nr_locks == 0) return(OK);
  lock_count = nr_locks;	/* save count of locks */
  for (flp = &file_lock[0]; flp < &file_lock[NR_LOCKS]; flp++) {
	if (flp->lock_type == 0) continue;	/* slot not in use */
	if (flp->lock_inode == rip && flp->lock_pid == fp->fp_pid) {
		flp->lock_type = 0;
		nr_locks--;
	}
  }
  if (nr_locks < lock_count) lock_revive();	/* lock released */
  return(OK);
}

/*===========================================================================*
 *				do_lseek				     *
 *===========================================================================*/
PUBLIC int do_lseek()
{
/* Perform the lseek(ls_fd, offset, whence) system call. */

  register struct filp *rfilp;
  register off_t pos;

  /* Check to see if the file descriptor is valid. */
  if ( (rfilp = get_filp(m_in.ls_fd)) == NIL_FILP) return(err_code);

  /* No lseek on pipes. */
  if (rfilp->filp_ino->i_pipe == I_PIPE) return(ESPIPE);

  /* The value of 'whence' determines the start position to use. */
  switch(m_in.whence) {
	case 0:	pos = 0;	break;
	case 1: pos = rfilp->filp_pos;	break;
	case 2: pos = rfilp->filp_ino->i_size;	break;
	default: return(EINVAL);
  }

  /* Check for overflow. */
  if (((long)m_in.offset > 0) && ((long)(pos + m_in.offset) < (long)pos)) 
  	return(EINVAL);
  if (((long)m_in.offset < 0) && ((long)(pos + m_in.offset) > (long)pos)) 
  	return(EINVAL);
  pos = pos + m_in.offset;

  if (pos != rfilp->filp_pos)
	rfilp->filp_ino->i_seek = ISEEK;	/* inhibit read ahead */
  rfilp->filp_pos = pos;
  m_out.reply_l1 = pos;		/* insert the long into the output message */
  return(OK);
}
