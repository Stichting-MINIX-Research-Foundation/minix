/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include "buf.h"
#include "inode.h"
#include "super.h"

static struct inode *new_node(struct inode *ldirp, char *string, mode_t
	bits, uid_t uid, gid_t gid, block_t z0);


/*===========================================================================*
 *				fs_create				     *
 *===========================================================================*/
int fs_create(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid,
	struct fsdriver_node *node)
{
  int r;
  struct inode *ldirp;
  struct inode *rip;

  /* Try to make the file. */

  /* Get last directory inode (i.e., directory that will hold the new inode) */
  if ((ldirp = get_inode(fs_dev, dir_nr)) == NULL)
	  return(ENOENT);

  /* Create a new inode by calling new_node(). */
  rip = new_node(ldirp, name, mode, uid, gid, NO_BLOCK);
  r = err_code;

  /* If an error occurred, release inode. */
  if (r != OK) {
	  put_inode(ldirp);
	  put_inode(rip);
	  return(r);
  }

  /* Reply message */
  node->fn_ino_nr = rip->i_num;
  node->fn_mode = rip->i_mode;
  node->fn_size = rip->i_size;
  node->fn_uid = rip->i_uid;
  node->fn_gid = rip->i_gid;
  node->fn_dev = NO_DEV;

  /* Drop parent dir */
  put_inode(ldirp);

  return(OK);
}


/*===========================================================================*
 *				fs_mknod				     *
 *===========================================================================*/
int fs_mknod(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid,
	dev_t dev)
{
  struct inode *ip, *ldirp;

  /* Get last directory inode */
  if((ldirp = get_inode(fs_dev, dir_nr)) == NULL)
	  return(ENOENT);

  /* Try to create the new node */
  ip = new_node(ldirp, name, mode, uid, gid, (block_t) dev);

  put_inode(ip);
  put_inode(ldirp);
  return(err_code);
}


/*===========================================================================*
 *				fs_mkdir				     *
 *===========================================================================*/
int fs_mkdir(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid)
{
  int r1, r2;			/* status codes */
  ino_t dot, dotdot;		/* inode numbers for . and .. */
  struct inode *rip, *ldirp;

  /* Get last directory inode */
  if((ldirp = get_inode(fs_dev, dir_nr)) == NULL)
      return(ENOENT);

  /* Next make the inode. If that fails, return error code. */
  rip = new_node(ldirp, name, mode, uid, gid, (block_t) 0);

  if(rip == NULL || err_code == EEXIST) {
	  put_inode(rip);		/* can't make dir: it already exists */
	  put_inode(ldirp);
	  return(err_code);
  }

  /* Get the inode numbers for . and .. to enter in the directory. */
  dotdot = ldirp->i_num;	/* parent's inode number */
  dot = rip->i_num;		/* inode number of the new dir itself */

  /* Now make dir entries for . and .. unless the disk is completely full. */
  /* enter . in the new dir*/
  r1 = search_dir(rip, ".", &dot, ENTER, I_DIRECTORY);
  /* enter .. in the new dir */
  r2 = search_dir(rip, "..", &dotdot, ENTER, I_DIRECTORY);

  /* If both . and .. were successfully entered, increment the link counts. */
  if (r1 == OK && r2 == OK) {
	  /* Normal case.  It was possible to enter . and .. in the new dir. */
	  rip->i_links_count++;	/* this accounts for . */
	  ldirp->i_links_count++;	/* this accounts for .. */
	  ldirp->i_dirt = IN_DIRTY;	/* mark parent's inode as dirty */
  } else {
	  /* It was not possible to enter . or .. probably disk was full -
	   * links counts haven't been touched. */
	  if (search_dir(ldirp, name, NULL, DELETE, 0) != OK)
		  panic("Dir disappeared: %d ", (int) rip->i_num);
	  rip->i_links_count--;	/* undo the increment done in new_node() */
  }
  rip->i_dirt = IN_DIRTY;	/* either way, i_links_count has changed */

  put_inode(ldirp);		/* return the inode of the parent dir */
  put_inode(rip);		/* return the inode of the newly made dir */
  return(err_code);		/* new_node() always sets 'err_code' */
}


/*===========================================================================*
 *                             fs_slink 				     *
 *===========================================================================*/
int fs_slink(ino_t dir_nr, char *name, uid_t uid, gid_t gid,
	struct fsdriver_data *data, size_t bytes)
{
  struct inode *sip;           /* inode containing symbolic link */
  struct inode *ldirp;         /* directory containing link */
  register int r;              /* error code */
  char* link_target_buf = NULL;       /* either sip->i_block or bp->b_data */
  struct buf *bp = NULL;    /* disk buffer for link */

  /* Temporarily open the dir. */
  if( (ldirp = get_inode(fs_dev, dir_nr)) == NULL)
	  return(EINVAL);

  /* Create the inode for the symlink. */
  sip = new_node(ldirp, name, (I_SYMBOLIC_LINK | RWX_MODES), uid, gid, 0);

  /* If we can then create fast symlink (store it in inode),
   * Otherwise allocate a disk block for the contents of the symlink and
   * copy contents of symlink (the name pointed to) into first disk block. */
  if( (r = err_code) == OK) {
	if (bytes + 1 > sip->i_sp->s_block_size) {
		r = ENAMETOOLONG;
	} else if ((bytes + 1) <= MAX_FAST_SYMLINK_LENGTH) {
		r = fsdriver_copyin(data, 0, (char *) sip->i_block, bytes);
		sip->i_dirt = IN_DIRTY;
		link_target_buf = (char*) sip->i_block;
        } else {
		if ((bp = new_block(sip, (off_t) 0)) != NULL) {
			r = fsdriver_copyin(data, 0, b_data(bp), bytes);
			lmfs_markdirty(bp);
			link_target_buf = b_data(bp);
		} else {
			r = err_code;
		}
	}
	if (r == OK) {
		assert(link_target_buf);
		link_target_buf[bytes] = '\0';
		sip->i_size = (off_t) strlen(link_target_buf);
		if (sip->i_size != bytes) {
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

	put_block(bp); /* put_block() accepts NULL. */

	if(r != OK) {
		sip->i_links_count = NO_LINK;
		if (search_dir(ldirp, name, NULL, DELETE, 0) != OK)
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
	char *string, mode_t bits, uid_t uid, gid_t gid, block_t b0)
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

  if (S_ISDIR(bits) && (ldirp->i_links_count >= USHRT_MAX ||
			ldirp->i_links_count >= LINK_MAX)) {
        /* New entry is a directory, alas we can't give it a ".." */
        err_code = EMLINK;
        return(NULL);
  }

  /* Get final component of the path. */
  rip = advance(ldirp, string);

  if ( rip == NULL && err_code == ENOENT) {
	/* Last path component does not exist.  Make new directory entry. */
	if ( (rip = alloc_inode(ldirp, bits, uid, gid)) == NULL) {
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
	if ((r=search_dir(ldirp, string, &rip->i_num, ENTER,
			  rip->i_mode & I_TYPE)) != OK) {
		rip->i_links_count--;	/* pity, have to free disk inode */
		rip->i_dirt = IN_DIRTY;	/* dirty inodes are written out */
		put_inode(rip);	/* this call frees the inode */
		err_code = r;
		return(NULL);
	}

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
 *				fs_seek					     *
 *===========================================================================*/
void fs_seek(ino_t ino_nr)
{
  struct inode *rip;

  /* inhibit read ahead */
  if ((rip = find_inode(fs_dev, ino_nr)) != NULL)
	rip->i_seek = ISEEK;
}
