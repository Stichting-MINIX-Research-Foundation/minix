/* This file deals with protection in the file system.  It contains the code
 * for four system calls that relate to protection.
 *
 * The entry points into this file are
 *   do_chmod:	perform the CHMOD and FCHMOD system calls
 *   do_chown:	perform the CHOWN and FCHOWN system calls
 *   do_umask:	perform the UMASK system call
 *   do_access:	perform the ACCESS system call
 */

#include "fs.h"
#include <unistd.h>
#include <minix/callnr.h>
#include "file.h"
#include "fproc.h"
#include "param.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"

FORWARD _PROTOTYPE( in_group, (gid_t grp)				);

/*===========================================================================*
 *				do_chmod				     *
 *===========================================================================*/
PUBLIC int do_chmod()
{
/* Perform the chmod(name, mode) and fchmod(fd, mode) system calls. */

  struct filp *flp;
  struct vnode *vp;
  int r;
  mode_t new_mode;
    
  if (call_nr == CHMOD) {
  	/* Temporarily open the file */
	if(fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);
	if ((vp = eat_path(PATH_NOFLAGS)) == NIL_VNODE) return(err_code);
  } else {	/* call_nr == FCHMOD */
	/* File is already opened; get a pointer to vnode from filp. */
	if (!(flp = get_filp(m_in.fd))) return(err_code);
	vp = flp->filp_vno;
	dup_vnode(vp);
  } 

  /* Only the owner or the super_user may change the mode of a file.
   * No one may change the mode of a file on a read-only file system.
   */
  if (vp->v_uid != fp->fp_effuid && fp->fp_effuid != SU_UID)
	r = EPERM;
  else
	r = read_only(vp);

  /* If error, return inode. */
  if (r != OK)	{
	put_vnode(vp);
	return(r);
  }

  /* Now make the change. Clear setgid bit if file is not in caller's grp */
  if (fp->fp_effuid != SU_UID && vp->v_gid != fp->fp_effgid) 
	  m_in.mode &= ~I_SET_GID_BIT;

  if ((r = req_chmod(vp->v_fs_e, vp->v_inode_nr, m_in.mode, &new_mode)) == OK)
	vp->v_mode = new_mode;

  put_vnode(vp);
  return(OK);
}


/*===========================================================================*
 *				do_chown				     *
 *===========================================================================*/
PUBLIC int do_chown()
{
/* Perform the chmod(name, mode) and fchmod(fd, mode) system calls. */
  int inode_nr;
  int fs_e;
  struct filp *flp;
  struct vnode *vp;
  int r;
  uid_t uid;
  gid_t gid;
  mode_t new_mode;
  
  if (call_nr == CHOWN) {
	/* Temporarily open the file. */
      if(fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
      if ((vp = eat_path(PATH_NOFLAGS)) == NIL_VNODE) return(err_code);
  } else {	/* call_nr == FCHOWN */
  	/* File is already opened; get a pointer to the vnode from filp. */
      if (!(flp = get_filp(m_in.fd))) return(err_code);
      vp = flp->filp_vno;
      dup_vnode(vp);
  }

  r = read_only(vp);
  if (r == OK) {
	/* FS is R/W. Whether call is allowed depends on ownership, etc. */
	/* The super user can do anything, so check permissions only if we're
	   a regular user. */
	if (fp->fp_effuid != SU_UID) {
		/* Regular users can only change groups of their own files. */
		if (vp->v_uid != fp->fp_effuid) r = EPERM;	
		if (vp->v_uid != m_in.owner) r = EPERM;	/* no giving away */
		if (fp->fp_effgid != m_in.group) r = EPERM;
	}
  }

  if (r == OK) {
  	/* Do not change uid/gid if new uid/gid is -1. */
  	uid = (m_in.owner == (uid_t)-1 ? vp->v_uid : m_in.owner);
  	gid = (m_in.group == (gid_t)-1 ? vp->v_gid : m_in.group);
	if ((r = req_chown(vp->v_fs_e, vp->v_inode_nr, uid, gid,
		      &new_mode)) == OK) {
	  	vp->v_uid = uid;
		vp->v_gid = gid;
		vp->v_mode = new_mode;
	}
  }

  put_vnode(vp);
  return(r);
}


/*===========================================================================*
 *				do_umask				     *
 *===========================================================================*/
PUBLIC int do_umask()
{
/* Perform the umask(co_mode) system call. */
  register mode_t r;

  r = ~fp->fp_umask;		/* set 'r' to complement of old mask */
  fp->fp_umask = ~(m_in.co_mode & RWX_MODES);
  return(r);			/* return complement of old mask */
}


/*===========================================================================*
 *				do_access				     *
 *===========================================================================*/
PUBLIC int do_access()
{
/* Perform the access(name, mode) system call. */
  int r;
  struct vnode *vp;
    
  /* First check to see if the mode is correct. */
  if ( (m_in.mode & ~(R_OK | W_OK | X_OK)) != 0 && m_in.mode != F_OK)
	return(EINVAL);

  /* Temporarily open the file. */
  if (fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);
  if ((vp = eat_path(PATH_NOFLAGS)) == NIL_VNODE) return(err_code);

  r = forbidden(vp, m_in.mode);
  put_vnode(vp);
  return(r);
}


/*===========================================================================*
 *				forbidden				     *
 *===========================================================================*/
PUBLIC int forbidden(struct vnode *vp, mode_t access_desired)
{
/* Given a pointer to an vnode, 'vp', and the access desired, determine
 * if the access is allowed, and if not why not.  The routine looks up the
 * caller's uid in the 'fproc' table.  If access is allowed, OK is returned
 * if it is forbidden, EACCES is returned.
 */

  register struct super_block *sp;
  register mode_t bits, perm_bits;
  uid_t uid;
  gid_t gid;
  int r, shift, type;

  if (vp->v_uid == (uid_t) -1 || vp->v_gid == (gid_t) -1) return(EACCES);

  /* Isolate the relevant rwx bits from the mode. */
  bits = vp->v_mode;
  uid = (call_nr == ACCESS ? fp->fp_realuid : fp->fp_effuid);
  gid = (call_nr == ACCESS ? fp->fp_realgid : fp->fp_effgid);

  if (uid == SU_UID) {
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
	if (uid == vp->v_uid) shift = 6;		/* owner */
	else if (gid == vp->v_gid) shift = 3;		/* group */
	else if (in_group(vp->v_gid) == OK) shift = 3;	/* suppl. groups */
	else shift = 0;					/* other */
	perm_bits = (bits >> shift) & (R_BIT | W_BIT | X_BIT);
  }

  /* If access desired is not a subset of what is allowed, it is refused. */
  r = OK;
  if ((perm_bits | access_desired) != perm_bits) r = EACCES;

  /* Check to see if someone is trying to write on a file system that is
   * mounted read-only.
   */
  if (r == OK)
	if (access_desired & W_BIT)
	 	r = read_only(vp);

  return(r);
}


/*===========================================================================*
 *				in_group				     *
 *===========================================================================*/
PRIVATE int in_group(gid_t grp)
{
  int i;

  for (i = 0; i < fp->fp_ngroups; i++)
	if (fp->fp_sgroups[i] == grp)
		return(OK);
	
  return(EINVAL);
}


/*===========================================================================*
 *				read_only				     *
 *===========================================================================*/
PUBLIC int read_only(vp)
struct vnode *vp;		/* ptr to inode whose file sys is to be cked */
{
/* Check to see if the file system on which the inode 'ip' resides is mounted
 * read only.  If so, return EROFS, else return OK.
 */
  register struct vmnt *mp;

  mp = vp->v_vmnt;
  return(mp->m_flags ? EROFS : OK);
}


