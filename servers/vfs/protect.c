
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
  int r;
    
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

  /* Fill in request message fields.*/
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  req.rmode = m_in.mode;
  
  /* Issue request */
  return req_chmod(&req);
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
  int r;
  
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

  /* Fill in request message fields.*/
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  req.newuid = m_in.owner;
  req.newgid = m_in.group;
  
  /* Issue request */
  return req_chown(&req);
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

