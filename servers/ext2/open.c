/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <minix/com.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>

static struct inode *new_node(struct inode *ldirp, char *string, mode_t
	bits, block_t z0);


/*===========================================================================*
 *				fs_create				     *
 *===========================================================================*/
int fs_create()
{
  phys_bytes len;
  int r;
  struct inode *ldirp;
  struct inode *rip;
  mode_t omode;
  char lastc[NAME_MAX + 1];

  /* Read request message */
  omode = fs_m_in.m_vfs_fs_create.mode;
  caller_uid = fs_m_in.m_vfs_fs_create.uid;
  caller_gid = fs_m_in.m_vfs_fs_create.gid;

  /* Try to make the file. */

  /* Copy the last component (i.e., file name) */
  len = fs_m_in.m_vfs_fs_create.path_len; /* including trailing '\0' */
  if (len > NAME_MAX + 1 || len > EXT2_NAME_MAX + 1)
	return(ENAMETOOLONG);

  err_code = sys_safecopyfrom(VFS_PROC_NR, fs_m_in.m_vfs_fs_create.grant,
			      (vir_bytes) 0, (vir_bytes) lastc, (size_t) len);
  if (err_code != OK) return err_code;
  NUL(lastc, len, sizeof(lastc));

  /* Get last directory inode (i.e., directory that will hold the new inode) */
  if ((ldirp = get_inode(fs_dev, fs_m_in.m_vfs_fs_create.inode)) == NULL)
	  return(ENOENT);

  /* Create a new inode by calling new_node(). */
  rip = new_node(ldirp, lastc, omode, NO_BLOCK);
  r = err_code;

  /* If an error occurred, release inode. */
  if (r != OK) {
	  put_inode(ldirp);
	  put_inode(rip);
	  return(r);
  }

  /* Reply message */
  fs_m_out.m_fs_vfs_create.inode = rip->i_num;
  fs_m_out.m_fs_vfs_create.mode = rip->i_mode;
  fs_m_out.m_fs_vfs_create.file_size = rip->i_size;

  /* This values are needed for the execution */
  fs_m_out.m_fs_vfs_create.uid = rip->i_uid;
  fs_m_out.m_fs_vfs_create.gid = rip->i_gid;

  /* Drop parent dir */
  put_inode(ldirp);

  return(OK);
}


/*===========================================================================*
 *				fs_mknod				     *
 *===========================================================================*/
int fs_mknod()
{
  struct inode *ip, *ldirp;
  char lastc[NAME_MAX + 1];
  phys_bytes len;

  /* Copy the last component and set up caller's user and group id */
  len = fs_m_in.m_vfs_fs_mknod.path_len; /* including trailing '\0' */
  if (len > NAME_MAX + 1 || len > EXT2_NAME_MAX + 1)
	return(ENAMETOOLONG);

  err_code = sys_safecopyfrom(VFS_PROC_NR, fs_m_in.m_vfs_fs_mknod.grant,
                             (vir_bytes) 0, (vir_bytes) lastc, (size_t) len);
  if (err_code != OK) return err_code;
  NUL(lastc, len, sizeof(lastc));

  caller_uid = fs_m_in.m_vfs_fs_mknod.uid;
  caller_gid = fs_m_in.m_vfs_fs_mknod.gid;

  /* Get last directory inode */
  if((ldirp = get_inode(fs_dev, fs_m_in.m_vfs_fs_mknod.inode)) == NULL)
	  return(ENOENT);

  /* Try to create the new node */
  ip = new_node(ldirp, lastc, fs_m_in.m_vfs_fs_mknod.mode,
		(block_t) fs_m_in.m_vfs_fs_mknod.device);

  put_inode(ip);
  put_inode(ldirp);
  return(err_code);
}


/*===========================================================================*
 *				fs_mkdir				     *
 *===========================================================================*/
int fs_mkdir()
{
  int r1, r2;			/* status codes */
  ino_t dot, dotdot;		/* inode numbers for . and .. */
  struct inode *rip, *ldirp;
  char lastc[NAME_MAX + 1];         /* last component */
  phys_bytes len;

  /* Copy the last component and set up caller's user and group id */
  len = fs_m_in.m_vfs_fs_mkdir.path_len; /* including trailing '\0' */
  if (len > NAME_MAX + 1 || len > EXT2_NAME_MAX + 1)
	return(ENAMETOOLONG);

  err_code = sys_safecopyfrom(VFS_PROC_NR, fs_m_in.m_vfs_fs_mkdir.grant,
			      (vir_bytes) 0, (vir_bytes) lastc, (phys_bytes) len);
  if(err_code != OK) return(err_code);
  NUL(lastc, len, sizeof(lastc));

  caller_uid = fs_m_in.m_vfs_fs_mkdir.uid;
  caller_gid = fs_m_in.m_vfs_fs_mkdir.gid;

  /* Get last directory inode */
  if((ldirp = get_inode(fs_dev, fs_m_in.m_vfs_fs_mkdir.inode)) == NULL)
      return(ENOENT);

  /* Next make the inode. If that fails, return error code. */
  rip = new_node(ldirp, lastc, fs_m_in.m_vfs_fs_mkdir.mode, (block_t) 0);

  if(rip == NULL || err_code == EEXIST) {
	  put_inode(rip);		/* can't make dir: it already exists */
	  put_inode(ldirp);
	  return(err_code);
  }

  /* Get the inode numbers for . and .. to enter in the directory. */
  dotdot = ldirp->i_num;	/* parent's inode number */
  dot = rip->i_num;		/* inode number of the new dir itself */

  /* Now make dir entries for . and .. unless the disk is completely full. */
  /* Use dot1 and dot2, so the mode of the directory isn't important. */
  rip->i_mode = fs_m_in.m_vfs_fs_mkdir.mode;	/* set mode */
  /* enter . in the new dir*/
  r1 = search_dir(rip, dot1, &dot, ENTER, IGN_PERM, I_DIRECTORY);
  /* enter .. in the new dir */
  r2 = search_dir(rip, dot2, &dotdot, ENTER, IGN_PERM, I_DIRECTORY);

  /* If both . and .. were successfully entered, increment the link counts. */
  if (r1 == OK && r2 == OK) {
	  /* Normal case.  It was possible to enter . and .. in the new dir. */
	  rip->i_links_count++;	/* this accounts for . */
	  ldirp->i_links_count++;	/* this accounts for .. */
	  ldirp->i_dirt = IN_DIRTY;	/* mark parent's inode as dirty */
  } else {
	  /* It was not possible to enter . or .. probably disk was full -
	   * links counts haven't been touched. */
	  if (search_dir(ldirp, lastc, NULL, DELETE, IGN_PERM, 0) != OK)
		  panic("Dir disappeared: %d ", (int) rip->i_num);
	  rip->i_links_count--;	/* undo the increment done in new_node() */
  }
  rip->i_dirt = IN_DIRTY;		/* either way, i_links_count has changed */

  put_inode(ldirp);		/* return the inode of the parent dir */
  put_inode(rip);		/* return the inode of the newly made dir */
  return(err_code);		/* new_node() always sets 'err_code' */
}


/*===========================================================================*
 *                             fs_slink 				     *
 *===========================================================================*/
int fs_slink()
{
  phys_bytes len;
  struct inode *sip;           /* inode containing symbolic link */
  struct inode *ldirp;         /* directory containing link */
  register int r;              /* error code */
  char string[NAME_MAX];       /* last component of the new dir's path name */
  char* link_target_buf = NULL;       /* either sip->i_block or bp->b_data */
  struct buf *bp = NULL;    /* disk buffer for link */

  caller_uid = fs_m_in.m_vfs_fs_slink.uid;
  caller_gid = fs_m_in.m_vfs_fs_slink.gid;

  /* Copy the link name's last component */
  len = fs_m_in.m_vfs_fs_slink.path_len;
  if (len > NAME_MAX || len > EXT2_NAME_MAX)
	return(ENAMETOOLONG);

  r = sys_safecopyfrom(VFS_PROC_NR, fs_m_in.m_vfs_fs_slink.grant_path,
		       (vir_bytes) 0, (vir_bytes) string, (size_t) len);
  if (r != OK) return(r);
  NUL(string, len, sizeof(string));

  /* Temporarily open the dir. */
  if( (ldirp = get_inode(fs_dev, fs_m_in.m_vfs_fs_slink.inode)) == NULL)
	  return(EINVAL);

  /* Create the inode for the symlink. */
  sip = new_node(ldirp, string, (I_SYMBOLIC_LINK | RWX_MODES), 0);

  /* If we can then create fast symlink (store it in inode),
   * Otherwise allocate a disk block for the contents of the symlink and
   * copy contents of symlink (the name pointed to) into first disk block. */
  if( (r = err_code) == OK) {
	if ( (fs_m_in.m_vfs_fs_slink.mem_size + 1) > sip->i_sp->s_block_size) {
		r = ENAMETOOLONG;
	} else if ((fs_m_in.m_vfs_fs_slink.mem_size + 1) <= MAX_FAST_SYMLINK_LENGTH) {
		r = sys_safecopyfrom(VFS_PROC_NR,
				     fs_m_in.m_vfs_fs_slink.grant_target,
				     (vir_bytes) 0, (vir_bytes) sip->i_block,
                                     (vir_bytes) fs_m_in.m_vfs_fs_slink.mem_size);
		sip->i_dirt = IN_DIRTY;
		link_target_buf = (char*) sip->i_block;
        } else {
		if ((bp = new_block(sip, (off_t) 0)) != NULL) {
			sys_safecopyfrom(VFS_PROC_NR,
					 fs_m_in.m_vfs_fs_slink.grant_target,
					 (vir_bytes) 0, (vir_bytes) b_data(bp),
					 (vir_bytes) fs_m_in.m_vfs_fs_slink.mem_size);
			lmfs_markdirty(bp);
			link_target_buf = b_data(bp);
		} else {
			r = err_code;
		}
	}
	if (r == OK) {
		assert(link_target_buf);
		link_target_buf[fs_m_in.m_vfs_fs_slink.mem_size] = '\0';
		sip->i_size = (off_t) strlen(link_target_buf);
		if (sip->i_size != fs_m_in.m_vfs_fs_slink.mem_size) {
			  /* This can happen if the user provides a buffer
			   * with a \0 in it. This can cause a lot of trouble
			   * when the symlink is used later. We could just use
			   * the strlen() value, but we want to let the user
			   * know he did something wrong. ENAMETOOLONG doesn't
			   * exactly describe the error, but there is no
			   * ENAMETOOWRONG.
			   */
			  r = ENAMETOOLONG;
		  }
	}

	put_block(bp, DIRECTORY_BLOCK); /* put_block() accepts NULL. */

	if(r != OK) {
		sip->i_links_count = NO_LINK;
		if (search_dir(ldirp, string, NULL, DELETE, IGN_PERM, 0) != OK)
			panic("Symbolic link vanished");
	}
  }

  /* put_inode() accepts NULL as a noop, so the below are safe. */
  put_inode(sip);
  put_inode(ldirp);

  return(r);
}

/*===========================================================================*
 *				new_node				     *
 *===========================================================================*/
static struct inode *new_node(struct inode *ldirp,
	char *string, mode_t bits, block_t b0)
{
/* New_node() is called by fs_open(), fs_mknod(), and fs_mkdir().
 * In all cases it allocates a new inode, makes a directory entry for it in
 * the ldirp directory with string name, and initializes it.
 * It returns a pointer to the inode if it can do this;
 * otherwise it returns NULL.  It always sets 'err_code'
 * to an appropriate value (OK or an error code).
 */

  register struct inode *rip;
  register int r;

  if (ldirp->i_links_count == NO_LINK) { /* Dir does not actually exist */
	err_code = ENOENT;
	return(NULL);
  }

  /* Get final component of the path. */
  rip = advance(ldirp, string, IGN_PERM);

  if (S_ISDIR(bits) && (ldirp->i_links_count >= USHRT_MAX ||
			ldirp->i_links_count >= LINK_MAX)) {
        /* New entry is a directory, alas we can't give it a ".." */
        put_inode(rip);
        err_code = EMLINK;
        return(NULL);
  }

  if ( rip == NULL && err_code == ENOENT) {
	/* Last path component does not exist.  Make new directory entry. */
	if ( (rip = alloc_inode(ldirp, bits)) == NULL) {
		/* Can't creat new inode: out of inodes. */
		return(NULL);
	}

	/* Force inode to the disk before making directory entry to make
	 * the system more robust in the face of a crash: an inode with
	 * no directory entry is much better than the opposite.
	 */
	rip->i_links_count++;
	rip->i_block[0] = b0;		/* major/minor device numbers */
	rw_inode(rip, WRITING);		/* force inode to disk now */

	/* New inode acquired.  Try to make directory entry. */
	if ((r=search_dir(ldirp, string, &rip->i_num, ENTER, IGN_PERM,
			  rip->i_mode & I_TYPE)) != OK) {
		rip->i_links_count--;	/* pity, have to free disk inode */
		rip->i_dirt = IN_DIRTY;	/* dirty inodes are written out */
		put_inode(rip);	/* this call frees the inode */
		err_code = r;
		return(NULL);
	}

  } else if (err_code == EENTERMOUNT || err_code == ELEAVEMOUNT) {
	r = EEXIST;
  } else {
	/* Either last component exists, or there is some problem. */
	if (rip != NULL)
		r = EEXIST;
	else
		r = err_code;
  }

  /* The caller has to return the directory inode (*ldirp).  */
  err_code = r;
  return(rip);
}


/*===========================================================================*
 *				fs_inhibread				     *
 *===========================================================================*/
int fs_inhibread()
{
  struct inode *rip;

  if((rip = find_inode(fs_dev, fs_m_in.m_vfs_fs_inhibread.inode)) == NULL)
	  return(EINVAL);

  /* inhibit read ahead */
  rip->i_seek = ISEEK;

  return(OK);
}
