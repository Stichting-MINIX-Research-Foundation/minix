/* Virtual mount table related routines.
 *
 */

#include "fs.h"
#include "threads.h"
#include "vmnt.h"
#include <assert.h>
#include "fproc.h"

FORWARD _PROTOTYPE( int is_vmnt_locked, (struct vmnt *vmp)		);
FORWARD _PROTOTYPE( void clear_vmnt, (struct vmnt *vmp)			);

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
PUBLIC void check_vmnt_locks_by_me(struct fproc *rfp)
{
/* Check whether this thread still has locks held on vmnts */
  struct vmnt *vmp;

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++) {
	if (tll_locked_by_me(&vmp->m_lock))
		panic("Thread %d still holds vmnt lock on vmp %p call_nr=%d\n",
		      mthread_self(), vmp, call_nr);
  }

  if (rfp->fp_vmnt_rdlocks != 0)
	panic("Thread %d still holds read locks on a vmnt (%d) call_nr=%d\n",
	      mthread_self(), rfp->fp_vmnt_rdlocks, call_nr);
}
#endif

/*===========================================================================*
 *				check_vmnt_locks			     *
 *===========================================================================*/
PUBLIC void check_vmnt_locks()
{
  struct vmnt *vmp;
  int count = 0;

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++)
	if (is_vmnt_locked(vmp)) {
		count++;
		printf("vmnt %p is %s, fs_e=%d dev=%d\n", vmp, (tll_islocked(&vmp->m_lock) ? "locked":"pending locked"), vmp->m_fs_e, vmp->m_dev);
	}

  if (count) panic("%d locked vmnts\n", count);
#if 0
  printf("check_vmnt_locks OK\n");
#endif
}

/*===========================================================================*
 *                             clear_vmnt				     *
 *===========================================================================*/
PRIVATE void clear_vmnt(struct vmnt *vmp)
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
PUBLIC struct vmnt *get_free_vmnt(void)
{
  struct vmnt *vp;

  for (vp = &vmnt[0]; vp < &vmnt[NR_MNTS]; ++vp)
      if (vp->m_dev == NO_DEV) return(vp);

  return(NULL);
}

/*===========================================================================*
 *                             find_vmnt				     *
 *===========================================================================*/
PUBLIC struct vmnt *find_vmnt(endpoint_t fs_e)
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
PUBLIC void init_vmnts(void)
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
PRIVATE int is_vmnt_locked(struct vmnt *vmp)
{
  ASSERTVMP(vmp);
  return(tll_islocked(&vmp->m_lock) || tll_haspendinglock(&vmp->m_lock));
}

/*===========================================================================*
 *                             lock_vmnt				     *
 *===========================================================================*/
PUBLIC int lock_vmnt(struct vmnt *vmp, tll_access_t locktype)
{
  int r;
  tll_access_t initial_locktype;

  ASSERTVMP(vmp);

  initial_locktype = (locktype == VMNT_EXCL) ? VMNT_WRITE : locktype;

  if (vmp->m_fs_e == who_e) return(EDEADLK);

  r = tll_lock(&vmp->m_lock, initial_locktype);

  if (r == EBUSY) return(r);

  if (initial_locktype != locktype) {
	tll_upgrade(&vmp->m_lock);
  }

#if LOCK_DEBUG
  if (locktype == VMNT_READ)
	fp->fp_vmnt_rdlocks++;
#endif

  return(OK);
}

/*===========================================================================*
 *                             unlock_vmnt				     *
 *===========================================================================*/
PUBLIC void unlock_vmnt(struct vmnt *vmp)
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
 *                             vmnt_unmap_by_endpoint			     *
 *===========================================================================*/
PUBLIC void vmnt_unmap_by_endpt(endpoint_t proc_e)
{
  struct vmnt *vmp;

  if ((vmp = find_vmnt(proc_e)) != NULL)
	clear_vmnt(vmp);

}
