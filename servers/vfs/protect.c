
/* This file deals with protection in the file system.  It contains the code
 * for four system calls that relate to protection.
 *
 * The entry points into this file are
 *   do_chmod:	perform the CHMOD and FCHMOD system calls
 *   do_chown:	perform the CHOWN and FCHOWN system calls
 *   do_umask:	perform the UMASK system call
 *   do_access:	perform the ACCESS system call
 *
 * Changes for VFS:
 *   Jul 2006 (Balazs Gerofi)
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



/*===========================================================================*
 *				do_chmod				     *
 *===========================================================================*/
PUBLIC int do_chmod()
{
  struct filp *flp;
  struct chmod_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  struct vnode *vp;
  int r, ch_mode;
    
  if (call_nr == CHMOD) {
      /* Perform the chmod(name, mode) system call. */
      if (fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);

      /* Fill in lookup request fields */
      lookup_req.path = user_fullpath;
      lookup_req.lastc = NULL;
      lookup_req.flags = EAT_PATH;

      /* Request lookup */
      if ((r = lookup(&lookup_req, &res)) != OK) return r;

      req.inode_nr = res.inode_nr;
      req.fs_e = res.fs_e;
  } 
  else if (call_nr == FCHMOD) {
      if (!(flp = get_filp(m_in.m3_i1))) return err_code;
      req.inode_nr = flp->filp_vno->v_inode_nr;
      req.fs_e = flp->filp_vno->v_fs_e;
  }
  else panic(__FILE__, "do_chmod called with strange call_nr", call_nr);

  /* Find vnode, if it's in use. */
  vp = find_vnode(req.fs_e, req.inode_nr);

  /* Fill in request message fields.*/
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  req.rmode = m_in.mode;
  
  /* Issue request */
  if((r = req_chmod(&req, &ch_mode)) != OK) return r;

  if(vp != NIL_VNODE)
  	vp->v_mode = ch_mode;

  return OK;
}

/*===========================================================================*
 *				do_chown				     *
 *===========================================================================*/
PUBLIC int do_chown()
{
  int inode_nr;
  int fs_e;
  struct filp *flp;
  struct chown_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  struct vnode *vp;
  int r, ch_mode;
  
  if (call_nr == CHOWN) {
      /* Perform the chmod(name, mode) system call. */
      if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
      
      /* Fill in lookup request fields */
      lookup_req.path = user_fullpath;
      lookup_req.lastc = NULL;
      lookup_req.flags = EAT_PATH;

      /* Request lookup */
      if ((r = lookup(&lookup_req, &res)) != OK) return r;

      req.inode_nr = res.inode_nr;
      req.fs_e = res.fs_e;
  } 
  else if (call_nr == FCHOWN) {
      if (!(flp = get_filp(m_in.m1_i1))) return err_code;
      req.inode_nr = flp->filp_vno->v_inode_nr;
      req.fs_e = flp->filp_vno->v_fs_e;
  }
  else panic(__FILE__, "do_chmod called with strange call_nr", call_nr);

  /* Find vnode, if it's in use. */
  vp = find_vnode(req.fs_e, req.inode_nr);

  /* Fill in request message fields.*/
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  req.newuid = m_in.owner;
  req.newgid = m_in.group;
  
  /* Issue request */
  r = req_chown(&req, &ch_mode);

  if(r == OK && vp) {
  	vp->v_uid = m_in.owner;
  	vp->v_gid = m_in.group;
  	vp->v_mode = ch_mode;
  }

  return r;
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
  struct access_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  int r;
    
  /* First check to see if the mode is correct. */
  if ( (m_in.mode & ~(R_OK | W_OK | X_OK)) != 0 && m_in.mode != F_OK)
	return(EINVAL);

  if (fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);

  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = NULL;
  lookup_req.flags = EAT_PATH;

  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;

  /* Fill in request fields */
  req.fs_e = res.fs_e;
  req.amode = m_in.mode;
  req.inode_nr = res.inode_nr;
  req.uid = fp->fp_realuid;         /* real user and group id */
  req.gid = fp->fp_realgid;
  
  /* Issue request */
  return req_access(&req);
}


/*===========================================================================*
 *				forbidden				     *
 *===========================================================================*/
PUBLIC int forbidden(struct vnode *vp, mode_t access_desired)
{
/* Given a pointer to an inode, 'rip', and the access desired, determine
 * if the access is allowed, and if not why not.  The routine looks up the
 * caller's uid in the 'fproc' table.  If access is allowed, OK is returned
 * if it is forbidden, EACCES is returned.
 */

  register struct super_block *sp;
  register mode_t bits, perm_bits;
  int r, shift, type;

  if (vp->v_uid == (uid_t)-1 || vp->v_gid == (gid_t)-1)
  {
	printf("forbidden: bad uid/gid in vnode\n");
	printf("forbidden: last allocated at %s, %d\n", vp->v_file, vp->v_line);
	return EACCES;
  }

  /* Isolate the relevant rwx bits from the mode. */
  bits = vp->v_mode;
  if (fp->fp_effuid == SU_UID) {
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
	if (fp->fp_effuid == vp->v_uid) shift = 6;	/* owner */
	else if (fp->fp_effgid == vp->v_gid ) shift = 3;	/* group */
	else shift = 0;					/* other */
	perm_bits = (bits >> shift) & (R_BIT | W_BIT | X_BIT);
  }

  /* If access desired is not a subset of what is allowed, it is refused. */
  r = OK;
  if ((perm_bits | access_desired) != perm_bits) {
  	r = EACCES;
  	}

  /* Check to see if someone is trying to write on a file system that is
   * mounted read-only.
   */
  if (r == OK)
	if (access_desired & W_BIT)
	 	r = read_only(vp);

  return(r);
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


