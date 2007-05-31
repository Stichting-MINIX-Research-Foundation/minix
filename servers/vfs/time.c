/* This file takes care of those system calls that deal with time.
 *
 * The entry points into this file are
 *   do_utime:		perform the UTIME system call
 *   do_stime:		PM informs FS about STIME system call
 */

#include "fs.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include "file.h"
#include "fproc.h"
#include "param.h"

#include <minix/vfsif.h>
#include "vmnt.h"

/*===========================================================================*
 *				do_utime				     *
 *===========================================================================*/
PUBLIC int do_utime()
{
/* Perform the utime(name, timep) system call. */
  register int len;
  struct utime_req req;
  struct lookup_req lookup_req;
  struct node_details res;
  int r;
  
  /* Adjust for case of 'timep' being NULL;
   * utime_strlen then holds the actual size: strlen(name)+1.
   */
  len = m_in.utime_length;
  if (len == 0) len = m_in.utime_strlen;

  if (fetch_name(m_in.utime_file, len, M1) != OK) return(err_code);
  
  /* Fill in lookup request fields */
  lookup_req.path = user_fullpath;
  lookup_req.lastc = NULL;
  lookup_req.flags = EAT_PATH;
        
  /* Request lookup */
  if ((r = lookup(&lookup_req, &res)) != OK) return r;

  /* Fill in request fields.*/
  if (m_in.utime_length == 0) {
        req.actime = 0;	
        req.modtime = clock_time();
  } else {
        req.actime = m_in.utime_actime;
        req.modtime = m_in.utime_modtime;
  }
  req.fs_e = res.fs_e;
  req.inode_nr = res.inode_nr;
  req.uid = fp->fp_effuid;
  req.gid = fp->fp_effgid;
  
  /* Issue request */
  return req_utime(&req);
}


/*===========================================================================*
 *				do_stime				     *
 *===========================================================================*/
PUBLIC int do_stime()
{
  struct vmnt *vmp;
  /* Perform the stime(tp) system call. */
  boottime = (long) m_in.pm_stime; 
    
  /* Send new time for all FS processes */
  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) { 
      if (vmp->m_fs_e != NONE) req_stime(vmp->m_fs_e, boottime);
  }

  return OK;
}

