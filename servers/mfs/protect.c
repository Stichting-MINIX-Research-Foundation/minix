

#include "fs.h"
#include <unistd.h>
#include <minix/callnr.h>
#include "buf.h"
#include "inode.h"
#include "super.h"

#include <minix/vfsif.h>


/*===========================================================================*
 *				fs_chmod				     *
 *===========================================================================*/
PUBLIC int fs_chmod()
{
/* Perform the chmod(name, mode) system call. */

  register struct inode *rip;
  register int r;
  
  /* Temporarily open the file. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_chmod() failed\n", SELF_E);
        return(EINVAL);
  }

  /* Now make the change. Clear setgid bit if file is not in caller's grp */
  rip->i_mode = (rip->i_mode & ~ALL_MODES) | (fs_m_in.REQ_MODE & ALL_MODES);
  rip->i_update |= CTIME;
  rip->i_dirt = DIRTY;

  /* Return full new mode to caller. */
  fs_m_out.RES_MODE = rip->i_mode;

  put_inode(rip);
  return(OK);
}


/*===========================================================================*
 *				fs_chown				     *
 *===========================================================================*/
PUBLIC int fs_chown()
{
  register struct inode *rip;
  register int r;
  /* Temporarily open the file. */
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Temporarily open the file. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_chown() failed\n", SELF_E);
        return(EINVAL);
  }

  /* Not permitted to change the owner of a file on a read-only file sys. */
  r = read_only(rip);
  if (r == OK) {
	rip->i_uid = fs_m_in.REQ_NEW_UID;
	rip->i_gid = fs_m_in.REQ_NEW_GID;
	rip->i_mode &= ~(I_SET_UID_BIT | I_SET_GID_BIT);
	rip->i_update |= CTIME;
	rip->i_dirt = DIRTY;
  }

  /* Update caller on current mode, as it may have changed. */
  fs_m_out.RES_MODE = rip->i_mode;

  put_inode(rip);

  return(r);
}

/*===========================================================================*
 *				fs_access_o				     *
 *===========================================================================*/
PUBLIC int fs_access_o()
{
  struct inode *rip;
  register int r;
  
  /* Temporarily open the file whose access is to be checked. */
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Temporarily open the file. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_access() failed\n", SELF_E);
        return(EINVAL);
  }

  /* Now check the permissions. */
  r = forbidden(rip, (mode_t) fs_m_in.REQ_MODE);
  put_inode(rip);
  return(r);
}

/*===========================================================================*
 *				forbidden				     *
 *===========================================================================*/
PUBLIC int forbidden(register struct inode *rip, mode_t access_desired)
{
/* Given a pointer to an inode, 'rip', and the access desired, determine
 * if the access is allowed, and if not why not.  The routine looks up the
 * caller's uid in the 'fproc' table.  If access is allowed, OK is returned
 * if it is forbidden, EACCES is returned.
 */

  register struct inode *old_rip = rip;
  register struct super_block *sp;
  register mode_t bits, perm_bits;
  int r, shift, type;

  if (caller_uid == (uid_t)-1 && caller_gid == (uid_t)-1)
  {
	printf(
	"forbidden: warning caller_uid and caller_gid not initialized\n");
  }

  /*
  if (rip->i_mount == I_MOUNT)	
	for (sp = &super_block[1]; sp < &super_block[NR_SUPERS]; sp++)
		if (sp->s_imount == rip) {
			rip = get_inode(sp->s_dev, ROOT_INODE);
			break;
		} 
  */

  /* Isolate the relevant rwx bits from the mode. */
  bits = rip->i_mode;
  if (caller_uid == SU_UID) {
	/* Grant read and write permission.  Grant search permission for
	 * directories.  Grant execute permission (for non-directories) if
	 * and only if one of the 'X' bits is set.
	 */
	if ( (bits & I_TYPE) == I_DIRECTORY ||
	     bits & ((X_BIT << 6) | (X_BIT << 3) | X_BIT))
		perm_bits = R_BIT | W_BIT | X_BIT;
	else
		perm_bits = R_BIT | W_BIT;
  } else {
	if (caller_uid == rip->i_uid) shift = 6;	/* owner */
	else if (caller_gid == rip->i_gid ) shift = 3;	/* group */
	else shift = 0;					/* other */
	perm_bits = (bits >> shift) & (R_BIT | W_BIT | X_BIT);
  }

  /* If access desired is not a subset of what is allowed, it is refused. */
  r = OK;
  if ((perm_bits | access_desired) != perm_bits) r = EACCES;

  /* Check to see if someone is trying to write on a file system that is
   * mounted read-only.
   */
  type = rip->i_mode & I_TYPE;
  if (r == OK)
	if (access_desired & W_BIT)
	 	r = read_only(rip);

#if 0
  if (r != OK) printf(
  "forbidden: caller uid/gid %d/%d object uid/gid %d/%d, returning %d\n",
	caller_uid, caller_gid, rip->i_uid, rip->i_gid, r);
#endif

  if (rip != old_rip) put_inode(rip);

  return(r);
}

/*===========================================================================*
 *				read_only				     *
 *===========================================================================*/
PUBLIC int read_only(ip)
struct inode *ip;		/* ptr to inode whose file sys is to be cked */
{
/* Check to see if the file system on which the inode 'ip' resides is mounted
 * read only.  If so, return EROFS, else return OK.
 */

  register struct super_block *sp;

  sp = ip->i_sp;
  return(sp->s_rd_only ? EROFS : OK);
}



