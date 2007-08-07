
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

#include "fproc.h"
#include "file.h"

#include <minix/vfsif.h>


/*===========================================================================*
 *				get_free_vnode				     *
 *===========================================================================*/
PUBLIC struct vnode *get_free_vnode(file, line)
char *file;
int line;
{
/* Find a free vnode slot in the vnode table */    
  struct vnode *vp;

  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; ++vp)
	if (vp->v_ref_count == 0)
	{
		vp->v_pipe= NO_PIPE;
		vp->v_uid= -1;
		vp->v_gid= -1;
		vp->v_sdev= -1;
		vp->v_file= file;
		vp->v_line= line;
		return vp;
	}

  
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
      if (vp->v_ref_count > 0 && vp->v_inode_nr == numb
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
  if (vp == NIL_VNODE) {
      printf("VFSdup_vnode NIL_VNODE\n");
      return;
  }

  vp->v_ref_count++;
}


/*===========================================================================*
 *				put_vnode				     *
 *===========================================================================*/
PUBLIC void put_vnode(struct vnode *vp)
{
/* Decrease vnode's usage counter and decrease inode's usage counter in the 
 * corresponding FS process.
 */
  if (vp == NIL_VNODE) {
        /*printf("VFSput_vnode NIL_VNODE\n");*/
        return;
  }

  if (vp < &vnode[0] || vp >= &vnode[NR_VNODES])
  {
	printf("put_vnode: &vnode[0] = %p, &vnode[NR_VNODES] = %p, vp = %p\n",
		&vnode[0], &vnode[NR_VNODES], vp);
	panic(__FILE__, "put_vnode: bad vnode pointer", NO_NUM);
  }

  if (vp->v_ref_count > 1)
  {
	/* Decrease counter */
	vp->v_ref_count--;
	if (vp->v_fs_count > 256)
		vnode_clean_refs(vp);
	return;
  }

  if (vp->v_ref_count <= 0)
  {
	printf("put_vnode: bad v_ref_count %d\n", vp->v_ref_count);
	panic(__FILE__, "put_vnode failed", NO_NUM);
  }
  if (vp->v_fs_count <= 0)
  {
	printf("put_vnode: bad v_fs_count %d\n", vp->v_fs_count);
	panic(__FILE__, "put_vnode failed", NO_NUM);
  }

  /* Send request */
  if (req_putnode(vp->v_fs_e, vp->v_inode_nr, vp->v_fs_count) != OK)
      printf("VFSput_vnode Warning: inode doesn't exist\n"); 

  vp->v_fs_count= 0;
  vp->v_ref_count= 0;
  vp->v_pipe = NO_PIPE;
  vp->v_sdev = NO_DEV;
  vp->v_index = 0;
}


/*===========================================================================*
 *				vnode_clean_refs			     *
 *===========================================================================*/
PUBLIC void vnode_clean_refs(struct vnode *vp)
{
/* Tell the underlying FS to drop all reference but one. */
  if (vp == NIL_VNODE) {
        return;
  }

  if (vp->v_fs_count <= 1)
	return;	/* Nothing to do */
  if (req_putnode(vp->v_fs_e, vp->v_inode_nr, vp->v_fs_count-1) != OK)
	printf("vnode_clean_refs: req_putnode failed\n"); 
  vp->v_fs_count= 1;
}


#if 0
/*===========================================================================*
 *				mark_vn					     *
 *===========================================================================*/
PUBLIC void mark_vn(vp, file, line)
struct vnode *vp;
char *file;
int line;
{
	if (!vp)
		return;
	vp->v_file= file;
	vp->v_line= line;
}


/*===========================================================================*
 *				check_vrefs				     *
 *===========================================================================*/
PUBLIC int check_vrefs()
{
	int i, bad;
	int ispipe_flag, ispipe_mode;
	struct vnode *vp;
	struct vmnt *vmp;
	struct fproc *rfp;
	struct filp *f;

	/* Clear v_ref_check */
	for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; ++vp)
		vp->v_ref_check= 0;

	/* Count reference for processes */
	for (rfp=&fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
		if (rfp->fp_pid == PID_FREE)
			continue;
		vp= rfp->fp_rd;
		if (vp < &vnode[0] || vp >= &vnode[NR_VNODES])
			panic(__FILE__, "check_vrefs: bad vnode", NO_NUM);
		vp->v_ref_check++;
                
                vp= rfp->fp_wd;
		if (vp < &vnode[0] || vp >= &vnode[NR_VNODES])
			panic(__FILE__, "check_vrefs: bad vnode", NO_NUM);
		vp->v_ref_check++;

  	}

	/* Count references from filedescriptors */
	for (f = &filp[0]; f < &filp[NR_FILPS]; f++)
	{
		if (f->filp_count == 0)
			continue;
		vp= f->filp_vno;
		if (vp < &vnode[0] || vp >= &vnode[NR_VNODES])
			panic(__FILE__, "check_vrefs: bad vnode", NO_NUM);
		vp->v_ref_check++;
	}

	/* Count references to mount points */
	for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp)
	{
		if (vmp->m_dev == NO_DEV)
			continue;
		vp= vmp->m_mounted_on;
		if (vp < &vnode[0] || vp >= &vnode[NR_VNODES])
			panic(__FILE__, "check_vrefs: bad vnode", NO_NUM);
		vp->v_ref_check++;

		vp= vmp->m_root_node;
		if (vp < &vnode[0] || vp >= &vnode[NR_VNODES])
			panic(__FILE__, "check_vrefs: bad vnode", NO_NUM);
		vp->v_ref_check++;
	}

	/* Check references */
	bad= 0;
	for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; ++vp)
	{
		if (vp->v_ref_count != vp->v_ref_check)
		{
			printf(
"Bad reference count for inode %d on device 0x%x: found %d, listed %d\n",
				vp->v_inode_nr, vp->v_dev, vp->v_ref_check,
				vp->v_ref_count);
			printf("last marked at %s, %d\n",
				vp->v_file, vp->v_line);
			bad= 1;
		}

		/* Also check v_pipe */
		if (vp->v_ref_count != 0)
		{
			ispipe_flag= (vp->v_pipe == I_PIPE);
			ispipe_mode= ((vp->v_mode & I_TYPE) == I_NAMED_PIPE);
			if (ispipe_flag != ispipe_mode)
			{
				printf(
"Bad v_pipe for inode %d on device 0x%x: found %d, mode 0%o\n",
				vp->v_inode_nr, vp->v_dev, vp->v_pipe,
				vp->v_mode);
				printf("last marked at %s, %d\n",
					vp->v_file, vp->v_line);
				bad= 1;
			}
		}
	}
	return !bad;
}
#endif
