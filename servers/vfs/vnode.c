
/* This file contains the routines related to vnodes.
 * The entry points are:
 *      
 *  get_vnode - increase counter and get details of an inode
 *  get_free_vnode - get a pointer to a free vnode obj
 *  find_vnode - find a vnode according to the FS endpoint and the inode num.  
 *  dup_vnode - duplicate vnode (i.e. increase counter)
 *  put_vnode - drop vnode (i.e. decrease counter)  
 *  
 *  Jul 2006 (Balazs Gerofi)
 */
#include "fs.h"
#include "vnode.h"
#include "vmnt.h"

#include <minix/vfsif.h>

/*===========================================================================*
 *				get_vnode				     *
 *===========================================================================*/
PUBLIC struct vnode *get_vnode(int fs_e, int inode_nr)
{
/* get_vnode() is called to get the details of the specified inode.
 * Note that inode's usage counter in the FS is supposed to be incremented.
 */
  struct vnode *vp, *vp2;
  struct vmnt *vmp;

  /* Request & response structures */
  struct node_req req;
  struct node_details res;
  
  /* Check whether a free vnode is avaliable */
  if ((vp = get_free_vnode()) == NIL_VNODE) {
        printf("VFSget_vnode: no vnode available\n");
        return NIL_VNODE;
  }
  
  /* Fill req struct */
  req.inode_nr = inode_nr;
  req.fs_e = fs_e;

  /* Send request to FS */
  if (req_getnode(&req, &res) != OK) {
        printf("VFSget_vnode: couldn't find vnode\n"); 
        return NIL_VNODE;
  }

  /* Fill in the free vnode's fields and return it */
  vp->v_fs_e = res.fs_e;
  vp->v_inode_nr = res.inode_nr;
  vp->v_mode = res.fmode;
  vp->v_size = res.fsize;
  vp->v_sdev = res.dev;
  
  /* Find corresponding virtual mount object */
  if ( (vmp = find_vmnt(vp->v_fs_e)) == NIL_VMNT)
        printf("VFS: vmnt not found by get_vnode()\n");
  
  vp->v_vmnt = vmp; 
  vp->v_dev = vmp->m_dev;
  vp->v_count = 1;
  
  return vp; 
}


/*===========================================================================*
 *				get_free_vnode				     *
 *===========================================================================*/
PUBLIC struct vnode *get_free_vnode()
{
/* Find a free vnode slot in the vnode table */    
  struct vnode *vp;

  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; ++vp)
      if (vp->v_count == 0) return vp;
  
  err_code = ENFILE;
  return NIL_VNODE;
}

/*===========================================================================*
 *				find_vnode				     *
 *===========================================================================*/
PUBLIC struct vnode *find_vnode(int fs_e, int numb)
{
/* Find a specified (FS endpoint and inode number) vnode in the
 * vnode table */
  struct vnode *vp;

  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; ++vp)
      if (vp->v_count > 0 && vp->v_inode_nr == numb
              && vp->v_fs_e == fs_e) return vp;
  
  return NIL_VNODE;
}


/*===========================================================================*
 *				dup_vnode				     *
 *===========================================================================*/
PUBLIC void dup_vnode(struct vnode *vp)
{
/* dup_vnode() is called to increment the vnode and therefore the
 * referred inode's counter.
 */
  struct node_req req;
  struct node_details res;
  
  if (vp == NIL_VNODE) {
      printf("VFSdup_vnode NIL_VNODE\n");
      return;
  }

  /* Fill req struct */
  req.inode_nr = vp->v_inode_nr;
  req.fs_e = vp->v_fs_e;

  /* Send request to FS */
  if (req_getnode(&req, &res) != OK)
        printf("VFSdup_vnode Warning: inode doesn't exist\n"); 
  else
        vp->v_count++;
}


/*===========================================================================*
 *				put_vnode				     *
 *===========================================================================*/
PUBLIC void put_vnode(struct vnode *vp)
{
/* Decrease vnode's usage counter and decrease inode's usage counter in the 
 * corresponding FS process.
 */
  struct node_req req;
  
  if (vp == NIL_VNODE) {
        /*printf("VFSput_vnode NIL_VNODE\n");*/
        return;
  }

  /* Fill in request fields */
  req.fs_e = vp->v_fs_e;
  req.inode_nr = vp->v_inode_nr;

  /* Send request */
  if (req_putnode(&req) == OK) {
      /* Decrease counter */
      if (--vp->v_count == 0) {
          vp->v_pipe = NO_PIPE;
          vp->v_sdev = NO_DEV;
          vp->v_index = 0;
      }
  }
  else 
      printf("VFSput_vnode Warning: inode doesn't exist\n"); 
}


