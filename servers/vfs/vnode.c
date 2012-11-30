/* This file contains the routines related to vnodes.
 * The entry points are:
 *
 *  get_vnode - increase counter and get details of an inode
 *  get_free_vnode - get a pointer to a free vnode obj
 *  find_vnode - find a vnode according to the FS endpoint and the inode num.
 *  dup_vnode - duplicate vnode (i.e. increase counter)
 *  put_vnode - drop vnode (i.e. decrease counter)
 */

#include "fs.h"
#include "threads.h"
#include "vnode.h"
#include "vmnt.h"
#include "fproc.h"
#include "file.h"
#include <minix/vfsif.h>
#include <assert.h>

/* Is vnode pointer reasonable? */
#if NDEBUG
#define SANEVP(v)
#define CHECKVN(v)
#define ASSERTVP(v)
#else
#define SANEVP(v) ((((v) >= &vnode[0] && (v) < &vnode[NR_VNODES])))

#define BADVP(v, f, l) printf("%s:%d: bad vp %p\n", f, l, v)

/* vp check that returns 0 for use in check_vrefs() */
#define CHECKVN(v) if(!SANEVP(v)) {				\
	BADVP(v, __FILE__, __LINE__);	\
	return 0;	\
}

/* vp check that panics */
#define ASSERTVP(v) if(!SANEVP(v)) { \
	BADVP(v, __FILE__, __LINE__); panic("bad vp"); }
#endif

#if LOCK_DEBUG
/*===========================================================================*
 *				check_vnode_locks_by_me			     *
 *===========================================================================*/
void check_vnode_locks_by_me(struct fproc *rfp)
{
/* Check whether this thread still has locks held on vnodes */
  struct vnode *vp;

  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; vp++) {
	if (tll_locked_by_me(&vp->v_lock)) {
		panic("Thread %d still holds vnode lock on vp %x call_nr=%d\n",
		      mthread_self(), vp, job_call_nr);
	}
  }

  if (rfp->fp_vp_rdlocks != 0)
	panic("Thread %d still holds read locks on a vnode (%d) call_nr=%d\n",
	      mthread_self(), rfp->fp_vp_rdlocks, job_call_nr);
}
#endif

/*===========================================================================*
 *				check_vnode_locks			     *
 *===========================================================================*/
void check_vnode_locks()
{
  struct vnode *vp;
  int count = 0;

  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; vp++)
	if (is_vnode_locked(vp)) {
		count++;
	}

  if (count) panic("%d locked vnodes\n", count);
#if 0
  printf("check_vnode_locks OK\n");
#endif
}

/*===========================================================================*
 *				get_free_vnode				     *
 *===========================================================================*/
struct vnode *get_free_vnode()
{
/* Find a free vnode slot in the vnode table (it's not actually allocated) */
  struct vnode *vp;

  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; ++vp) {
	if (vp->v_ref_count == 0 && !is_vnode_locked(vp)) {
		vp->v_uid  = -1;
		vp->v_gid  = -1;
		vp->v_sdev = NO_DEV;
		vp->v_mapfs_e = NONE;
		vp->v_mapfs_count = 0;
		vp->v_mapinode_nr = 0;
		return(vp);
	}
  }

  err_code = ENFILE;
  return(NULL);
}


/*===========================================================================*
 *				find_vnode				     *
 *===========================================================================*/
struct vnode *find_vnode(int fs_e, ino_t ino)
{
/* Find a specified (FS endpoint and inode number) vnode in the
 * vnode table */
  struct vnode *vp;

  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; ++vp)
	if (vp->v_ref_count > 0 && vp->v_inode_nr == ino && vp->v_fs_e == fs_e)
		return(vp);

  return(NULL);
}

/*===========================================================================*
 *				is_vnode_locked				     *
 *===========================================================================*/
int is_vnode_locked(struct vnode *vp)
{
/* Find out whether a thread holds a lock on this vnode or is trying to obtain
 * a lock. */
  ASSERTVP(vp);

  return(tll_islocked(&vp->v_lock) || tll_haspendinglock(&vp->v_lock));
}

/*===========================================================================*
 *				init_vnodes				     *
 *===========================================================================*/
void init_vnodes(void)
{
  struct vnode *vp;

  for (vp = &vnode[0]; vp < &vnode[NR_VNODES]; ++vp) {
	vp->v_fs_e = NONE;
	vp->v_mapfs_e = NONE;
	vp->v_inode_nr = 0;
	vp->v_ref_count = 0;
	vp->v_fs_count = 0;
	vp->v_mapfs_count = 0;
	tll_init(&vp->v_lock);
  }
}

/*===========================================================================*
 *				lock_vnode				     *
 *===========================================================================*/
int lock_vnode(struct vnode *vp, tll_access_t locktype)
{
  int r;

  ASSERTVP(vp);

  r = tll_lock(&vp->v_lock, locktype);

#if LOCK_DEBUG
  if (locktype == VNODE_READ) {
	fp->fp_vp_rdlocks++;
  }
#endif

  if (r == EBUSY) return(r);
  return(OK);
}

/*===========================================================================*
 *				unlock_vnode				     *
 *===========================================================================*/
void unlock_vnode(struct vnode *vp)
{
#if LOCK_DEBUG
  int i;
  register struct vnode *rvp;
  struct worker_thread *w;
#endif
  ASSERTVP(vp);

#if LOCK_DEBUG
  /* Decrease read-only lock counter when not locked as VNODE_OPCL or
   * VNODE_WRITE */
  if (!tll_locked_by_me(&vp->v_lock)) {
	fp->fp_vp_rdlocks--;
  }

  for (i = 0; i < NR_VNODES; i++) {
	rvp = &vnode[i];

	w = rvp->v_lock.t_write;
	assert(w != self);
	while (w && w->w_next != NULL) {
		w = w->w_next;
		assert(w != self);
	}

	w = rvp->v_lock.t_serial;
	assert(w != self);
	while (w && w->w_next != NULL) {
		w = w->w_next;
		assert(w != self);
	}
  }
#endif

  tll_unlock(&vp->v_lock);
}

/*===========================================================================*
 *				vnode				     *
 *===========================================================================*/
void upgrade_vnode_lock(struct vnode *vp)
{
  ASSERTVP(vp);
  tll_upgrade(&vp->v_lock);
}

/*===========================================================================*
 *				dup_vnode				     *
 *===========================================================================*/
void dup_vnode(struct vnode *vp)
{
/* dup_vnode() is called to increment the vnode and therefore the
 * referred inode's counter.
 */
  ASSERTVP(vp);
  vp->v_ref_count++;
}


/*===========================================================================*
 *				put_vnode				     *
 *===========================================================================*/
void put_vnode(struct vnode *vp)
{
/* Decrease vnode's usage counter and decrease inode's usage counter in the
 * corresponding FS process. Decreasing the fs_count each time we decrease the
 * ref count would lead to poor performance. Instead, only decrease fs_count
 * when the ref count hits zero. However, this could lead to fs_count to wrap.
 * To prevent this, we drop the counter to 1 when the counter hits 256.
 * We maintain fs_count as a sanity check to make sure VFS and the FS are in
 * sync.
 */
  int r, lock_vp;

  ASSERTVP(vp);

  /* Lock vnode. It's quite possible this thread already has a lock on this
   * vnode. That's no problem, because the reference counter will not decrease
   * to zero in that case. However, if the counter does decrease to zero *and*
   * is already locked, we have a consistency problem somewhere. */
  lock_vp = lock_vnode(vp, VNODE_OPCL);

  if (vp->v_ref_count > 1) {
	/* Decrease counter */
	vp->v_ref_count--;
	if (vp->v_fs_count > 256)
		vnode_clean_refs(vp);
	if (lock_vp != EBUSY) unlock_vnode(vp);
	return;
  }

  /* If we already had a lock, there is a consistency problem */
  assert(lock_vp != EBUSY);
  upgrade_vnode_lock(vp); /* Acquire exclusive access */

  /* A vnode that's not in use can't be put back. */
  if (vp->v_ref_count <= 0)
	panic("put_vnode failed: bad v_ref_count %d\n", vp->v_ref_count);

  /* fs_count should indicate that the file is in use. */
  if (vp->v_fs_count <= 0)
	panic("put_vnode failed: bad v_fs_count %d\n", vp->v_fs_count);

  /* Tell FS we don't need this inode to be open anymore. */
  r = req_putnode(vp->v_fs_e, vp->v_inode_nr, vp->v_fs_count);

  if (r != OK) {
	printf("VFS: putnode failed: %d\n", r);
	util_stacktrace();
  }

  /* This inode could've been mapped. If so, tell mapped FS to close it as
   * well. If mapped onto same FS, this putnode is not needed. */
  if (vp->v_mapfs_e != NONE && vp->v_mapfs_e != vp->v_fs_e)
	req_putnode(vp->v_mapfs_e, vp->v_mapinode_nr, vp->v_mapfs_count);

  vp->v_fs_count = 0;
  vp->v_ref_count = 0;
  vp->v_mapfs_count = 0;

  unlock_vnode(vp);
}


/*===========================================================================*
 *				vnode_clean_refs			     *
 *===========================================================================*/
void vnode_clean_refs(struct vnode *vp)
{
/* Tell the underlying FS to drop all reference but one. */

  if (vp == NULL) return;
  if (vp->v_fs_count <= 1) return;	/* Nothing to do */

  /* Drop all references except one */
  req_putnode(vp->v_fs_e, vp->v_inode_nr, vp->v_fs_count - 1);
  vp->v_fs_count = 1;
}

