
/* lookup() is the main routine that controls the path name lookup. It  
 * handles mountpoints and symbolic links. The actual lookup requests
 * are sent through the req_lookup wrapper function.
 *
 *  Jul 2006 (Balazs Gerofi)
 */

#include "fs.h"
#include <string.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <unistd.h>

#include <minix/vfsif.h>
#include "fproc.h"
#include "vmnt.h"
#include "vnode.h"
#include "param.h"


/*===========================================================================*
 *				lookup  				     *
 *===========================================================================*/
PUBLIC int lookup(lookup_req, node)
lookup_req_t *lookup_req;
node_details_t *node;
{
  struct vmnt *vmp;
  struct vnode *start_node;
  struct lookup_res res;
  int r, symloop = 0;
  int cum_path_processed = 0;
  
  /* Make a copy of the request so that the original values will be kept */ 
  struct lookup_req req = *lookup_req; 
  char *fullpath = lookup_req->path;

  /* Empty (start) path? */
  if (fullpath[0] == '\0') {
      return ENOENT;
  }

  /* Set user and group ids according to the system call */
  req.uid = (call_nr == ACCESS ? fp->fp_realuid : fp->fp_effuid); 
  req.gid = (call_nr == ACCESS ? fp->fp_realgid : fp->fp_effgid); 

  /* Set the starting directories inode number and FS endpoint */
  start_node = (fullpath[0] == '/' ? fp->fp_rd : fp->fp_wd);
  req.start_dir = start_node->v_inode_nr;
  req.fs_e = start_node->v_fs_e;

  /* Is the process' root directory on the same partition?,
   * if so, set the chroot directory too. */
  if (fp->fp_rd->v_dev == fp->fp_wd->v_dev)
      req.root_dir = fp->fp_rd->v_inode_nr; 
  else
      req.root_dir = 0;

  req.symloop = symloop;

  /* Issue the request */
  r = req_lookup(&req, &res);

  /* While the response is related to mount control set the 
   * new requests respectively */
  while (r == EENTERMOUNT || r == ELEAVEMOUNT || r == ESYMLINK) {
      
      /* If a symlink was encountered during the lookup the 
       * new path has been copied back and the number of characters 
       * processed has been started over. */
      if (r == ESYMLINK || res.symloop > symloop) {
          /* The link's content is copied back to the user_fullpath
           * array. Use it as the path argument from now on... */
          fullpath = user_fullpath;
          cum_path_processed = res.char_processed;
      }
      else {
          /* Otherwise, cumulate the characters already processsed from 
           * the path */
          cum_path_processed += res.char_processed;
      }

      /* Remember the current value of the symloop counter */
      symloop = res.symloop;

      /* Symlink encountered with absolute path */
      if (r == ESYMLINK) {
          start_node = fp->fp_rd;
      }
      /* Entering a new partition */
      else if (r == EENTERMOUNT) {
          start_node = 0;
          /* Start node is now the mounted partition's root node */
          for (vmp = &vmnt[0]; vmp != &vmnt[NR_MNTS]; ++vmp) {
              if (vmp->m_mounted_on->v_inode_nr == res.inode_nr
                      && vmp->m_mounted_on->v_fs_e == res.fs_e) {
                  start_node = vmp->m_root_node;
                  break;
              }
          }
          if (!start_node) {
              printf("VFSlookup: mounted partition couldn't be found\n");
              return ENOENT;
          }

      }
      /* Climbing up mount */
      else {
          /* Find the vmnt that represents the partition on
           * which we "climb up". */
          if ((vmp = find_vmnt(res.fs_e)) == NIL_VMNT) {
              printf("VFS: couldn't find vmnt during the climbup!\n");
              return ENOENT;
          }	  
          /* Start node is the vnode on which the partition is
           * mounted */
          start_node = vmp->m_mounted_on;
      }
      /* Fill in the request fields */
      req.start_dir = start_node->v_inode_nr;
      req.fs_e = start_node->v_fs_e;

      /* Is the process' root directory on the same partition?*/
      if (start_node->v_dev == fp->fp_rd->v_dev)
          req.root_dir = fp->fp_rd->v_inode_nr;
      else
          req.root_dir = 0;

      /* Fill in the current path name */
      req.path = &fullpath[cum_path_processed];
      req.symloop = symloop;
      
      /* Issue the request */
      r = req_lookup(&req, &res);
  }

  /* If success, fill in response fields */
  if (OK == r) {
      node->inode_nr = res.inode_nr;
      node->fmode = res.fmode;
      node->fsize = res.fsize;
      node->dev = res.dev;
      node->fs_e = res.fs_e;
  }
  
  return r;
}

