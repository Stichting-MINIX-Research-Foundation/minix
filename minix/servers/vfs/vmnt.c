/* Virtual mount table related routines.
 *
 */

#include "fs.h"
#include "vmnt.h"
#include <assert.h>
#include <string.h>

static int is_vmnt_locked(struct vmnt *vmp);
static void clear_vmnt(struct vmnt *vmp);

/* Is vmp pointer reasonable? */
#define SANEVMP(v) ((((v) >= &vmnt[0] && (v) < &vmnt[NR_MNTS])))
#define BADVMP(v, f, l) printf("%s:%d: bad vmp %p\n", f, l, v)
/* vp check that panics */
#define ASSERTVMP(v) if(!SANEVMP(v)) { \
	BADVMP(v, __FILE__, __LINE__); panic("bad vmp"); }

#if LOCK_DEBUG
/*===========================================================================*
 *				check_vmnt_locks_by_me			     *
 *===========================================================================*/
void check_vmnt_locks_by_me(struct fproc *rfp)
{
/* Check whether this thread still has locks held on vmnts */
  struct vmnt *vmp;

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++) {
	if (tll_locked_by_me(&vmp->m_lock))
		panic("Thread %d still holds vmnt lock on vmp %p call_nr=%d\n",
		      mthread_self(), vmp, job_call_nr);
  }

  if (rfp->fp_vmnt_rdlocks != 0)
	panic("Thread %d still holds read locks on a vmnt (%d) call_nr=%d\n",
	      mthread_self(), rfp->fp_vmnt_rdlocks, job_call_nr);
}
#endif

/*===========================================================================*
 *				check_vmnt_locks			     *
 *===========================================================================*/
void check_vmnt_locks()
{
  struct vmnt *vmp;
  int count = 0;

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++)
	if (is_vmnt_locked(vmp)) {
		count++;
		printf("vmnt %p is %s, fs_e=%d dev=%llx\n", vmp, (tll_islocked(&vmp->m_lock) ? "locked":"pending locked"), vmp->m_fs_e, vmp->m_dev);
	}

  if (count) panic("%d locked vmnts\n", count);
#if 0
  printf("check_vmnt_locks OK\n");
#endif
}

/*===========================================================================*
 *                             mark_vmnt_free				     *
 *===========================================================================*/
void mark_vmnt_free(struct vmnt *vmp)
{
  ASSERTVMP(vmp);

  vmp->m_fs_e = NONE;
  vmp->m_dev = NO_DEV;
}

/*===========================================================================*
 *                             clear_vmnt				     *
 *===========================================================================*/
static void clear_vmnt(struct vmnt *vmp)
{
/* Reset vmp to initial parameters */
  ASSERTVMP(vmp);

  vmp->m_fs_e = NONE;
  vmp->m_dev = NO_DEV;
  vmp->m_flags = 0;
  vmp->m_mounted_on = NULL;
  vmp->m_root_node = NULL;
  vmp->m_label[0] = '\0';
  vmp->m_comm.c_max_reqs = 1;
  vmp->m_comm.c_cur_reqs = 0;
  vmp->m_comm.c_req_queue = NULL;
}

/*===========================================================================*
 *                             get_free_vmnt				     *
 *===========================================================================*/
struct vmnt *get_free_vmnt(void)
{
  struct vmnt *vmp;

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
	if (vmp->m_dev == NO_DEV) {
		clear_vmnt(vmp);
		return(vmp);
	}
  }

  return(NULL);
}

/*===========================================================================*
 *                             find_vmnt				     *
 *===========================================================================*/
struct vmnt *find_vmnt(endpoint_t fs_e)
{
/* Find the vmnt belonging to an FS with endpoint 'fs_e' iff it's in use */
  struct vmnt *vp;

  for (vp = &vmnt[0]; vp < &vmnt[NR_MNTS]; ++vp)
	if (vp->m_fs_e == fs_e && vp->m_dev != NO_DEV)
		return(vp);

  return(NULL);
}

/*===========================================================================*
 *                             init_vmnts				     *
 *===========================================================================*/
void init_vmnts(void)
{
/* Initialize vmnt table */
  struct vmnt *vmp;

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++) {
	clear_vmnt(vmp);
	tll_init(&vmp->m_lock);
  }
}

/*===========================================================================*
 *                             is_vmnt_locked				     *
 *===========================================================================*/
static int is_vmnt_locked(struct vmnt *vmp)
{
  ASSERTVMP(vmp);
  return(tll_islocked(&vmp->m_lock) || tll_haspendinglock(&vmp->m_lock));
}

/*===========================================================================*
 *                             lock_vmnt				     *
 *===========================================================================*/
int lock_vmnt(struct vmnt *vmp, tll_access_t locktype)
{
  int r;
  tll_access_t initial_locktype;

  ASSERTVMP(vmp);

  initial_locktype = (locktype == VMNT_EXCL) ? VMNT_WRITE : locktype;

  if (vmp->m_fs_e == who_e) return(EDEADLK);

  r = tll_lock(&vmp->m_lock, initial_locktype);

  if (r == EBUSY) return(r);

  if (initial_locktype != locktype) {
	upgrade_vmnt_lock(vmp);
  }

#if LOCK_DEBUG
  if (locktype == VMNT_READ)
	fp->fp_vmnt_rdlocks++;
#endif

  return(OK);
}

/*===========================================================================*
 *                             vmnt_unmap_by_endpoint			     *
 *===========================================================================*/
void vmnt_unmap_by_endpt(endpoint_t proc_e)
{
  struct vmnt *vmp;

  if ((vmp = find_vmnt(proc_e)) != NULL) {
	mark_vmnt_free(vmp);
	fs_cancel(vmp);
	invalidate_filp_by_endpt(proc_e);
	if (vmp->m_mounted_on) {
		/* Only put mount point when it was actually used as mount
		 * point. That is, the mount was succesful. */
		put_vnode(vmp->m_mounted_on);
	}
  }
}

/*===========================================================================*
 *                             unlock_vmnt				     *
 *===========================================================================*/
void unlock_vmnt(struct vmnt *vmp)
{
  ASSERTVMP(vmp);

#if LOCK_DEBUG
  /* Decrease read-only lock counter when not locked as VMNT_WRITE or
   * VMNT_EXCL */
  if (!tll_locked_by_me(&vmp->m_lock))
	fp->fp_vmnt_rdlocks--;
#endif

  tll_unlock(&vmp->m_lock);

#if LOCK_DEBUG
  assert(!tll_locked_by_me(&vmp->m_lock));
#endif

}

/*===========================================================================*
 *                             downgrade_vmnt_lock			     *
 *===========================================================================*/
void downgrade_vmnt_lock(struct vmnt *vmp)
{
  ASSERTVMP(vmp);
  tll_downgrade(&vmp->m_lock);

#if LOCK_DEBUG
  /* If we're no longer the owner of a lock, we downgraded to VMNT_READ */
  if (!tll_locked_by_me(&vmp->m_lock)) {
	fp->fp_vmnt_rdlocks++;
  }
#endif
}

/*===========================================================================*
 *                             upgrade_vmnt_lock			     *
 *===========================================================================*/
void upgrade_vmnt_lock(struct vmnt *vmp)
{
  ASSERTVMP(vmp);
  tll_upgrade(&vmp->m_lock);
}

/*===========================================================================*
 *                             fetch_vmnt_paths				     *
 *===========================================================================*/
void fetch_vmnt_paths(void)
{
  struct vmnt *vmp;
  struct vnode *cur_wd;
  char orig_path[PATH_MAX];

  cur_wd = fp->fp_wd;

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++) {
	if (vmp->m_dev == NO_DEV)
		continue;
	if (vmp->m_fs_e == PFS_PROC_NR)
		continue;

	strlcpy(orig_path, vmp->m_mount_path, PATH_MAX);

	/* Find canonical path */
	if (canonical_path(vmp->m_mount_path, fp) != OK) {
		/* We failed to find it (moved somewhere else?). Let's try
		 * again by starting at the node on which we are mounted:
		 * pretend that node is our working directory and look for the
		 * canonical path of the relative path to the mount point
		 * (which should be in our 'working directory').
		 */
		char *mp;
		int len;

		fp->fp_wd = vmp->m_mounted_on;	/* Change our working dir */

		/* Isolate the mount point name of the full path */
		len = strlen(vmp->m_mount_path);
		if (vmp->m_mount_path[len - 1] == '/') {
			vmp->m_mount_path[len - 1] = '\0';
		}
		mp = strrchr(vmp->m_mount_path, '/');
		strlcpy(vmp->m_mount_path, mp+1, NAME_MAX+1);

		if (canonical_path(vmp->m_mount_path, fp) != OK) {
			/* Our second try failed too. Maybe an FS has crashed
			 * and we're missing part of the tree. Revert path.
			 */
			strlcpy(vmp->m_mount_path, orig_path, PATH_MAX);
		}
		fp->fp_wd = cur_wd;		/* Revert working dir */
	}
  }
}
