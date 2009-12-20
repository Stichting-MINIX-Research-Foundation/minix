#include "fs.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>

PRIVATE char mode_map[] = {R_BIT, W_BIT, R_BIT|W_BIT, 0};
FORWARD _PROTOTYPE( struct inode *new_node, (struct inode *ldirp, 
	char *string, mode_t bits, zone_t z0));


/*===========================================================================*
 *				fs_create				     *
 *===========================================================================*/
PUBLIC int fs_create()
{
  phys_bytes len;
  int r, b;
  struct inode *ldirp;
  struct inode *rip;
  mode_t omode;
  char lastc[NAME_MAX];
  
  /* Read request message */
  omode = fs_m_in.REQ_MODE;
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Try to make the file. */ 

  /* Copy the last component (i.e., file name) */
  len = MFS_MIN(fs_m_in.REQ_PATH_LEN, sizeof(lastc));
  err_code = sys_safecopyfrom(FS_PROC_NR, fs_m_in.REQ_GRANT, 0,
			      (vir_bytes) lastc, (phys_bytes) len, D);
  if (err_code != OK) return err_code;
  MFS_NUL(lastc, len, sizeof(lastc));

  /* Get last directory inode (i.e., directory that will hold the new inode) */
  if ((ldirp = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE)
	  return(ENOENT);

  /* Create a new inode by calling new_node(). */
  rip = new_node(ldirp, lastc, omode, NO_ZONE);
  r = err_code;

  /* If an error occurred, release inode. */
  if (r != OK) {
	  put_inode(ldirp);
	  put_inode(rip);
	  return(r);
  }
  
  /* Reply message */
  fs_m_out.m_source = rip->i_dev;  /* filled with FS endpoint by the system */
  fs_m_out.RES_INODE_NR = rip->i_num;
  fs_m_out.RES_MODE = rip->i_mode;
  fs_m_out.RES_FILE_SIZE_LO = rip->i_size;

  /* This values are needed for the execution */
  fs_m_out.RES_UID = rip->i_uid;
  fs_m_out.RES_GID = rip->i_gid;

  /* Drop parent dir */
  put_inode(ldirp);
  
  return(OK);
}


/*===========================================================================*
 *				fs_mknod				     *
 *===========================================================================*/
PUBLIC int fs_mknod()
{
  struct inode *ip, *ldirp;
  char lastc[NAME_MAX];
  phys_bytes len;

  /* Copy the last component and set up caller's user and group id */
  len = MFS_MIN(fs_m_in.REQ_PATH_LEN, sizeof(lastc));
  err_code = sys_safecopyfrom(FS_PROC_NR, fs_m_in.REQ_GRANT, 0,
			      (vir_bytes) lastc, (phys_bytes) len, D);
  if (err_code != OK) return err_code;
  MFS_NUL(lastc, len, sizeof(lastc));
  
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Get last directory inode */
  if((ldirp = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE)
	  return(ENOENT);
  
  /* Try to create the new node */
  ip = new_node(ldirp, lastc, fs_m_in.REQ_MODE, (zone_t) fs_m_in.REQ_DEV);

  put_inode(ip);
  put_inode(ldirp);
  return(err_code);
}


/*===========================================================================*
 *				fs_mkdir				     *
 *===========================================================================*/
PUBLIC int fs_mkdir()
{
  int r1, r2;			/* status codes */
  ino_t dot, dotdot;		/* inode numbers for . and .. */
  struct inode *rip, *ldirp;
  char lastc[NAME_MAX];         /* last component */
  phys_bytes len;

  /* Copy the last component and set up caller's user and group id */
  len = MFS_MIN(fs_m_in.REQ_PATH_LEN, sizeof(lastc));
  err_code = sys_safecopyfrom(FS_PROC_NR, fs_m_in.REQ_GRANT, 0,
			      (vir_bytes) lastc, (phys_bytes) len, D);
  if(err_code != OK) return(err_code);
  MFS_NUL(lastc, len, sizeof(lastc));

  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Get last directory inode */
  if((ldirp = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE)
      return(ENOENT);
  
  /* Next make the inode. If that fails, return error code. */
  rip = new_node(ldirp, lastc, fs_m_in.REQ_MODE, (zone_t) 0);
  
  if(rip == NIL_INODE || err_code == EEXIST) {
	  put_inode(rip);		/* can't make dir: it already exists */
	  put_inode(ldirp);
	  return(err_code);
  }
  
  /* Get the inode numbers for . and .. to enter in the directory. */
  dotdot = ldirp->i_num;	/* parent's inode number */
  dot = rip->i_num;		/* inode number of the new dir itself */

  /* Now make dir entries for . and .. unless the disk is completely full. */
  /* Use dot1 and dot2, so the mode of the directory isn't important. */
  rip->i_mode = fs_m_in.REQ_MODE;	/* set mode */
  r1 = search_dir(rip, dot1, &dot, ENTER, IGN_PERM);/* enter . in the new dir*/
  r2 = search_dir(rip, dot2, &dotdot, ENTER, IGN_PERM); /* enter .. in the new
							 dir */

  /* If both . and .. were successfully entered, increment the link counts. */
  if (r1 == OK && r2 == OK) {
	  /* Normal case.  It was possible to enter . and .. in the new dir. */
	  rip->i_nlinks++;	/* this accounts for . */
	  ldirp->i_nlinks++;	/* this accounts for .. */
	  ldirp->i_dirt = DIRTY;	/* mark parent's inode as dirty */
  } else {
	  /* It was not possible to enter . or .. probably disk was full -
	   * links counts haven't been touched. */
	  if(search_dir(ldirp, lastc, (ino_t *) 0, DELETE, IGN_PERM) != OK)
		  panic(__FILE__, "Dir disappeared ", rip->i_num);
	  rip->i_nlinks--;	/* undo the increment done in new_node() */
  }
  rip->i_dirt = DIRTY;		/* either way, i_nlinks has changed */

  put_inode(ldirp);		/* return the inode of the parent dir */
  put_inode(rip);		/* return the inode of the newly made dir */
  return(err_code);		/* new_node() always sets 'err_code' */
}


/*===========================================================================*
 *                             fs_slink 				     *
 *===========================================================================*/
PUBLIC int fs_slink()
{
  phys_bytes len;
  struct inode *sip;           /* inode containing symbolic link */
  struct inode *ldirp;         /* directory containing link */
  register int r;              /* error code */
  char string[NAME_MAX];       /* last component of the new dir's path name */
  struct buf *bp;              /* disk buffer for link */
    
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Copy the link name's last component */
  len = MFS_MIN(fs_m_in.REQ_PATH_LEN, sizeof(string));
  r = sys_safecopyfrom(FS_PROC_NR, fs_m_in.REQ_GRANT, 0,
		       (vir_bytes) string, (phys_bytes) len, D);
  if (r != OK) return(r);
  MFS_NUL(string, len, sizeof(string));
  
  /* Temporarily open the dir. */
  if( (ldirp = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE)
	  return(EINVAL);
  
  /* Create the inode for the symlink. */
  sip = new_node(ldirp, string, (mode_t) (I_SYMBOLIC_LINK | RWX_MODES),
		   (zone_t) 0);

  /* Allocate a disk block for the contents of the symlink.
   * Copy contents of symlink (the name pointed to) into first disk block. */
  if( (r = err_code) == OK) {
	  r = (bp = new_block(sip, (off_t) 0)) == NIL_BUF ? err_code : 
		  sys_safecopyfrom(FS_PROC_NR, fs_m_in.REQ_GRANT3, 0,
				   (vir_bytes) bp->b_data,
				   (vir_bytes) fs_m_in.REQ_MEM_SIZE, D);

	  if(r == OK) {
		  bp->b_data[_MIN_BLOCK_SIZE-1] = '\0';
		  sip->i_size = strlen(bp->b_data);
		  if(sip->i_size != fs_m_in.REQ_MEM_SIZE) {
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
	  
	  put_block(bp, DIRECTORY_BLOCK); /* put_block() accepts NIL_BUF. */
  
	  if(r != OK) {
		  sip->i_nlinks = 0;
		  if(search_dir(ldirp, string, (ino_t *) 0, DELETE, 
							IGN_PERM) != OK)
					
			  panic(__FILE__, "Symbolic link vanished", NO_NUM);
	  } 
  }

  /* put_inode() accepts NIL_INODE as a noop, so the below are safe. */
  put_inode(sip);
  put_inode(ldirp);
  
  return(r);
}


/*===========================================================================*
 *				fs_newnode				     *
 *===========================================================================*/
PUBLIC int fs_newnode()
{
  register int r;
  mode_t bits;
  struct inode *rip;

  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  bits = fs_m_in.REQ_MODE;

  /* Try to allocate the inode */
  if( (rip = alloc_inode(fs_dev, bits) ) == NIL_INODE)
	  return err_code;

  switch (bits & S_IFMT) {
	case S_IFBLK:
	case S_IFCHR:
		rip->i_zone[0] = fs_m_in.REQ_DEV; /* major/minor dev numbers*/
		break;
  }
  
  rw_inode(rip, WRITING);	/* mark inode as allocated */
  rip->i_update = ATIME | CTIME | MTIME;
  
  /* Fill in the fields of the response message */
  fs_m_out.RES_INODE_NR = rip->i_num;
  fs_m_out.RES_MODE = rip->i_mode;
  fs_m_out.RES_FILE_SIZE_LO = rip->i_size;
  fs_m_out.RES_UID = rip->i_uid;
  fs_m_out.RES_GID = rip->i_gid;
  fs_m_out.RES_DEV = (dev_t) rip->i_zone[0];

  return(OK);
}


/*===========================================================================*
 *				new_node				     *
 *===========================================================================*/
PRIVATE struct inode *new_node(struct inode *ldirp,
	char *string, mode_t bits, zone_t z0)
{
/* New_node() is called by fs_open(), fs_mknod(), and fs_mkdir().  
 * In all cases it allocates a new inode, makes a directory entry for it in
 * the ldirp directory with string name, and initializes it.  
 * It returns a pointer to the inode if it can do this; 
 * otherwise it returns NIL_INODE.  It always sets 'err_code'
 * to an appropriate value (OK or an error code).
 * 
 * The parsed path rest is returned in 'parsed' if parsed is nonzero. It
 * has to hold at least NAME_MAX bytes.
 */

  register struct inode *rip;
  register int r;

  /* Get final component of the path. */
  rip = advance(ldirp, string, IGN_PERM);

  if (S_ISDIR(bits) && 
      (ldirp)->i_nlinks >= ((ldirp)->i_sp->s_version == V1 ?
      CHAR_MAX : SHRT_MAX)) {
        /* New entry is a directory, alas we can't give it a ".." */
        put_inode(rip);
        err_code = EMLINK;
        return(NIL_INODE);
  }

  if ( rip == NIL_INODE && err_code == ENOENT) {
	/* Last path component does not exist.  Make new directory entry. */
	if ( (rip = alloc_inode((ldirp)->i_dev, bits)) == NIL_INODE) {
		/* Can't creat new inode: out of inodes. */
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
	if((r=search_dir(ldirp, string, &rip->i_num, ENTER, IGN_PERM)) != OK) {
		rip->i_nlinks--;	/* pity, have to free disk inode */
		rip->i_dirt = DIRTY;	/* dirty inodes are written out */
		put_inode(rip);	/* this call frees the inode */
		err_code = r;
		return(NIL_INODE);
	}

  } else if (err_code == EENTERMOUNT || err_code == ELEAVEMOUNT) {
  	r = EEXIST;
  } else { 
	/* Either last component exists, or there is some problem. */
	if (rip != NIL_INODE)
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
PUBLIC int fs_inhibread()
{
  struct inode *rip;
  
  if((rip = find_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE)
	  return(EINVAL);

  /* inhibit read ahead */
  rip->i_seek = ISEEK;	
  
  return(OK);
}

