
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

FORWARD _PROTOTYPE( int Xlookup, (lookup_req_t *lookup_req,
				node_details_t *node, char **pathrem)   );

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
      node->inode_nr = 0;
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
	      printf("VFSlookup: res.inode_nr = %d, res.fs_e = %d\n",
		res.inode_nr, res.fs_e);
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

  /* Fill in response fields */
  node->inode_nr = res.inode_nr;
  node->fmode = res.fmode;
  node->fsize = res.fsize;
  node->dev = res.dev;
  node->fs_e = res.fs_e;
  node->uid = res.uid;
  node->gid = res.gid;
  
  return r;
}


/*===========================================================================*
 *				Xlookup  				     *
 *===========================================================================*/
PRIVATE int Xlookup(lookup_req, node, pathrem)
lookup_req_t *lookup_req;
node_details_t *node;
char **pathrem;
{
  struct vmnt *vmp;
  struct vnode *start_node;
  struct lookup_res res;
  int r, symloop = 0;
  int cum_path_processed = 0;

  /* Make a copy of the request so that the original values will be kept */ 
  struct lookup_req req = *lookup_req; 
  char *fullpath = lookup_req->path;

  /* Clear pathrem */
  *pathrem= NULL;

  /* Empty (start) path? */
  if (fullpath[0] == '\0') {
      node->inode_nr = 0;
      *pathrem = fullpath;
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
	      printf("VFSlookup: res.inode_nr = %d, res.fs_e = %d\n",
		res.inode_nr, res.fs_e);
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

  if (r == ENOENT)
  {
        cum_path_processed += res.char_processed;
	*pathrem= &fullpath[cum_path_processed];
  }

  /* Fill in response fields */
  node->inode_nr = res.inode_nr;
  node->fmode = res.fmode;
  node->fsize = res.fsize;
  node->dev = res.dev;
  node->fs_e = res.fs_e;
  node->uid = res.uid;
  node->gid = res.gid;
  
  return r;
}


/*===========================================================================*
 *				lookup_vp  				     *
 *===========================================================================*/
PUBLIC int lookup_vp(lookup_req, vpp)
lookup_req_t *lookup_req;
struct vnode **vpp;
{
  int r, lookup_res;
  struct vnode *vp;
  struct vmnt *vmp;
  node_req_t node_req;
  struct node_details res;

  lookup_res = lookup(lookup_req, &res);

  if (res.inode_nr == 0)
  {
#if 0
	printf("lookup_vp: lookup returned no inode\n");
	printf("lookup_res = %d, last = '%s'\n\n",
		lookup_res, lookup_req->lastc);
#endif
	*vpp= NULL;
	return lookup_res;
  }

  /* Check whether vnode is already in use or not */
  if ((vp = find_vnode(res.fs_e, res.inode_nr)) != NIL_VNODE) {
        vp->v_ref_count++;
	*vpp= vp;
	return lookup_res;
  }

  /* See if free vnode is available */
  if ((vp = get_free_vnode(__FILE__, __LINE__)) == NIL_VNODE) {
      printf("VFS lookup_vp: no free vnode available\n");
      *vpp= NULL;
      return EINVAL;
  }

  /* Fill in request message fields.*/
  node_req.fs_e = res.fs_e;
  node_req.inode_nr = res.inode_nr;

  /* Issue request */
  if ((r = req_getnode(&node_req, &res)) != OK)
  {
	printf("lookup_vp: req_getnode failed: %d\n", r);
	*vpp= NULL;
	return r;
  }
  
  /* Fill in the free vnode's fields */
  vp->v_fs_e = res.fs_e;
  vp->v_inode_nr = res.inode_nr;
  vp->v_mode = res.fmode;
  vp->v_size = res.fsize;
  vp->v_uid = res.uid;
  vp->v_gid = res.gid;
  vp->v_sdev = res.dev;

  if ( (vmp = find_vmnt(vp->v_fs_e)) == NIL_VMNT)
	panic(__FILE__, "lookup_vp: vmnt not found", NO_NUM);

  vp->v_vmnt = vmp; 
  vp->v_dev = vmp->m_dev;
  vp->v_fs_count = 1;
  vp->v_ref_count = 1;

  *vpp= vp;
  return lookup_res;
}

/*===========================================================================*
 *				Xlookup_vp  				     *
 *===========================================================================*/
PUBLIC int Xlookup_vp(lookup_req, vpp, pathrem)
lookup_req_t *lookup_req;
struct vnode **vpp;
char **pathrem;
{
  int r, lookup_res;
  struct vnode *vp;
  struct vmnt *vmp;
  node_req_t node_req;
  struct node_details res;

  lookup_res = Xlookup(lookup_req, &res, pathrem);

  if (res.inode_nr == 0)
  {
#if 0
	printf("Xlookup_vp: lookup returned no inode\n");
	printf("lookup_res = %d, last = '%s'\n\n",
		lookup_res, lookup_req->lastc);
#endif
	*vpp= NULL;
	return lookup_res;
  }

  /* Check whether vnode is already in use or not */
  if ((vp = find_vnode(res.fs_e, res.inode_nr)) != NIL_VNODE) {
        vp->v_ref_count++;
	*vpp= vp;
	return lookup_res;
  }

  /* See if free vnode is available */
  if ((vp = get_free_vnode(__FILE__, __LINE__)) == NIL_VNODE) {
      printf("VFS Xlookup_vp: no free vnode available\n");
      *vpp= NULL;
      return EINVAL;
  }

  /* Fill in request message fields.*/
  node_req.fs_e = res.fs_e;
  node_req.inode_nr = res.inode_nr;

  /* Issue request */
  if ((r = req_getnode(&node_req, &res)) != OK)
  {
	printf("Xlookup_vp: req_getnode failed: %d\n", r);
	*vpp= NULL;
	return r;
  }
  
  /* Fill in the free vnode's fields */
  vp->v_fs_e = res.fs_e;
  vp->v_inode_nr = res.inode_nr;
  vp->v_mode = res.fmode;
  vp->v_size = res.fsize;
  vp->v_uid = res.uid;
  vp->v_gid = res.gid;
  vp->v_sdev = res.dev;

  if ( (vmp = find_vmnt(vp->v_fs_e)) == NIL_VMNT)
	panic(__FILE__, "Xlookup_vp: vmnt not found", NO_NUM);

  vp->v_vmnt = vmp; 
  vp->v_dev = vmp->m_dev;
  vp->v_fs_count = 1;
  vp->v_ref_count = 1;

  *vpp= vp;
  return lookup_res;
}

