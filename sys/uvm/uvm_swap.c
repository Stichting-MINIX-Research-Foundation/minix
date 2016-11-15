/*	$NetBSD: uvm_swap.c,v 1.173 2015/07/30 09:55:57 maxv Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997, 2009 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: NetBSD: vm_swap.c,v 1.52 1997/12/02 13:47:37 pk Exp
 * from: Id: uvm_swap.c,v 1.1.2.42 1998/02/02 20:38:06 chuck Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_swap.c,v 1.173 2015/07/30 09:55:57 maxv Exp $");

#include "opt_uvmhist.h"
#include "opt_compat_netbsd.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/vmem.h>
#include <sys/blist.h>
#include <sys/mount.h>
#include <sys/pool.h>
#include <sys/kmem.h>
#include <sys/syscallargs.h>
#include <sys/swap.h>
#include <sys/kauth.h>
#include <sys/sysctl.h>
#include <sys/workqueue.h>

#include <uvm/uvm.h>

#include <miscfs/specfs/specdev.h>

/*
 * uvm_swap.c: manage configuration and i/o to swap space.
 */

/*
 * swap space is managed in the following way:
 *
 * each swap partition or file is described by a "swapdev" structure.
 * each "swapdev" structure contains a "swapent" structure which contains
 * information that is passed up to the user (via system calls).
 *
 * each swap partition is assigned a "priority" (int) which controls
 * swap parition usage.
 *
 * the system maintains a global data structure describing all swap
 * partitions/files.   there is a sorted LIST of "swappri" structures
 * which describe "swapdev"'s at that priority.   this LIST is headed
 * by the "swap_priority" global var.    each "swappri" contains a
 * TAILQ of "swapdev" structures at that priority.
 *
 * locking:
 *  - swap_syscall_lock (krwlock_t): this lock serializes the swapctl
 *    system call and prevents the swap priority list from changing
 *    while we are in the middle of a system call (e.g. SWAP_STATS).
 *  - uvm_swap_data_lock (kmutex_t): this lock protects all swap data
 *    structures including the priority list, the swapdev structures,
 *    and the swapmap arena.
 *
 * each swap device has the following info:
 *  - swap device in use (could be disabled, preventing future use)
 *  - swap enabled (allows new allocations on swap)
 *  - map info in /dev/drum
 *  - vnode pointer
 * for swap files only:
 *  - block size
 *  - max byte count in buffer
 *  - buffer
 *
 * userland controls and configures swap with the swapctl(2) system call.
 * the sys_swapctl performs the following operations:
 *  [1] SWAP_NSWAP: returns the number of swap devices currently configured
 *  [2] SWAP_STATS: given a pointer to an array of swapent structures
 *	(passed in via "arg") of a size passed in via "misc" ... we load
 *	the current swap config into the array. The actual work is done
 *	in the uvm_swap_stats() function.
 *  [3] SWAP_ON: given a pathname in arg (could be device or file) and a
 *	priority in "misc", start swapping on it.
 *  [4] SWAP_OFF: as SWAP_ON, but stops swapping to a device
 *  [5] SWAP_CTL: changes the priority of a swap device (new priority in
 *	"misc")
 */

/*
 * swapdev: describes a single swap partition/file
 *
 * note the following should be true:
 * swd_inuse <= swd_nblks  [number of blocks in use is <= total blocks]
 * swd_nblks <= swd_mapsize [because mapsize includes miniroot+disklabel]
 */
struct swapdev {
	dev_t			swd_dev;	/* device id */
	int			swd_flags;	/* flags:inuse/enable/fake */
	int			swd_priority;	/* our priority */
	int			swd_nblks;	/* blocks in this device */
	char			*swd_path;	/* saved pathname of device */
	int			swd_pathlen;	/* length of pathname */
	int			swd_npages;	/* #pages we can use */
	int			swd_npginuse;	/* #pages in use */
	int			swd_npgbad;	/* #pages bad */
	int			swd_drumoffset;	/* page0 offset in drum */
	int			swd_drumsize;	/* #pages in drum */
	blist_t			swd_blist;	/* blist for this swapdev */
	struct vnode		*swd_vp;	/* backing vnode */
	TAILQ_ENTRY(swapdev)	swd_next;	/* priority tailq */

	int			swd_bsize;	/* blocksize (bytes) */
	int			swd_maxactive;	/* max active i/o reqs */
	struct bufq_state	*swd_tab;	/* buffer list */
	int			swd_active;	/* number of active buffers */
};

/*
 * swap device priority entry; the list is kept sorted on `spi_priority'.
 */
struct swappri {
	int			spi_priority;     /* priority */
	TAILQ_HEAD(spi_swapdev, swapdev)	spi_swapdev;
	/* tailq of swapdevs at this priority */
	LIST_ENTRY(swappri)	spi_swappri;      /* global list of pri's */
};

/*
 * The following two structures are used to keep track of data transfers
 * on swap devices associated with regular files.
 * NOTE: this code is more or less a copy of vnd.c; we use the same
 * structure names here to ease porting..
 */
struct vndxfer {
	struct buf	*vx_bp;		/* Pointer to parent buffer */
	struct swapdev	*vx_sdp;
	int		vx_error;
	int		vx_pending;	/* # of pending aux buffers */
	int		vx_flags;
#define VX_BUSY		1
#define VX_DEAD		2
};

struct vndbuf {
	struct buf	vb_buf;
	struct vndxfer	*vb_xfer;
};

/*
 * NetBSD 1.3 swapctl(SWAP_STATS, ...) swapent structure; uses 32 bit
 * dev_t and has no se_path[] member.
 */
struct swapent13 {
	int32_t	se13_dev;		/* device id */
	int	se13_flags;		/* flags */
	int	se13_nblks;		/* total blocks */
	int	se13_inuse;		/* blocks in use */
	int	se13_priority;		/* priority of this device */
};

/*
 * NetBSD 5.0 swapctl(SWAP_STATS, ...) swapent structure; uses 32 bit
 * dev_t.
 */
struct swapent50 {
	int32_t	se50_dev;		/* device id */
	int	se50_flags;		/* flags */
	int	se50_nblks;		/* total blocks */
	int	se50_inuse;		/* blocks in use */
	int	se50_priority;		/* priority of this device */
	char	se50_path[PATH_MAX+1];	/* path name */
};

/*
 * We keep a of pool vndbuf's and vndxfer structures.
 */
static struct pool vndxfer_pool, vndbuf_pool;

/*
 * local variables
 */
static vmem_t *swapmap;	/* controls the mapping of /dev/drum */

/* list of all active swap devices [by priority] */
LIST_HEAD(swap_priority, swappri);
static struct swap_priority swap_priority;

/* locks */
static krwlock_t swap_syscall_lock;

/* workqueue and use counter for swap to regular files */
static int sw_reg_count = 0;
static struct workqueue *sw_reg_workqueue;

/* tuneables */
u_int uvm_swapisfull_factor = 99;

/*
 * prototypes
 */
static struct swapdev	*swapdrum_getsdp(int);

static struct swapdev	*swaplist_find(struct vnode *, bool);
static void		 swaplist_insert(struct swapdev *,
					 struct swappri *, int);
static void		 swaplist_trim(void);

static int swap_on(struct lwp *, struct swapdev *);
static int swap_off(struct lwp *, struct swapdev *);

static void sw_reg_strategy(struct swapdev *, struct buf *, int);
static void sw_reg_biodone(struct buf *);
static void sw_reg_iodone(struct work *wk, void *dummy);
static void sw_reg_start(struct swapdev *);

static int uvm_swap_io(struct vm_page **, int, int, int);

/*
 * uvm_swap_init: init the swap system data structures and locks
 *
 * => called at boot time from init_main.c after the filesystems
 *	are brought up (which happens after uvm_init())
 */
void
uvm_swap_init(void)
{
	UVMHIST_FUNC("uvm_swap_init");

	UVMHIST_CALLED(pdhist);
	/*
	 * first, init the swap list, its counter, and its lock.
	 * then get a handle on the vnode for /dev/drum by using
	 * the its dev_t number ("swapdev", from MD conf.c).
	 */

	LIST_INIT(&swap_priority);
	uvmexp.nswapdev = 0;
	rw_init(&swap_syscall_lock);
	mutex_init(&uvm_swap_data_lock, MUTEX_DEFAULT, IPL_NONE);

	if (bdevvp(swapdev, &swapdev_vp))
		panic("%s: can't get vnode for swap device", __func__);
	if (vn_lock(swapdev_vp, LK_EXCLUSIVE | LK_RETRY))
		panic("%s: can't lock swap device", __func__);
	if (VOP_OPEN(swapdev_vp, FREAD | FWRITE, NOCRED))
		panic("%s: can't open swap device", __func__);
	VOP_UNLOCK(swapdev_vp);

	/*
	 * create swap block resource map to map /dev/drum.   the range
	 * from 1 to INT_MAX allows 2 gigablocks of swap space.  note
	 * that block 0 is reserved (used to indicate an allocation
	 * failure, or no allocation).
	 */
	swapmap = vmem_create("swapmap", 1, INT_MAX - 1, 1, NULL, NULL, NULL, 0,
	    VM_NOSLEEP, IPL_NONE);
	if (swapmap == 0) {
		panic("%s: vmem_create failed", __func__);
	}

	pool_init(&vndxfer_pool, sizeof(struct vndxfer), 0, 0, 0, "swp vnx",
	    NULL, IPL_BIO);
	pool_init(&vndbuf_pool, sizeof(struct vndbuf), 0, 0, 0, "swp vnd",
	    NULL, IPL_BIO);

	UVMHIST_LOG(pdhist, "<- done", 0, 0, 0, 0);
}

/*
 * swaplist functions: functions that operate on the list of swap
 * devices on the system.
 */

/*
 * swaplist_insert: insert swap device "sdp" into the global list
 *
 * => caller must hold both swap_syscall_lock and uvm_swap_data_lock
 * => caller must provide a newly allocated swappri structure (we will
 *	FREE it if we don't need it... this it to prevent allocation
 *	blocking here while adding swap)
 */
static void
swaplist_insert(struct swapdev *sdp, struct swappri *newspp, int priority)
{
	struct swappri *spp, *pspp;
	UVMHIST_FUNC("swaplist_insert"); UVMHIST_CALLED(pdhist);

	/*
	 * find entry at or after which to insert the new device.
	 */
	pspp = NULL;
	LIST_FOREACH(spp, &swap_priority, spi_swappri) {
		if (priority <= spp->spi_priority)
			break;
		pspp = spp;
	}

	/*
	 * new priority?
	 */
	if (spp == NULL || spp->spi_priority != priority) {
		spp = newspp;  /* use newspp! */
		UVMHIST_LOG(pdhist, "created new swappri = %d",
			    priority, 0, 0, 0);

		spp->spi_priority = priority;
		TAILQ_INIT(&spp->spi_swapdev);

		if (pspp)
			LIST_INSERT_AFTER(pspp, spp, spi_swappri);
		else
			LIST_INSERT_HEAD(&swap_priority, spp, spi_swappri);
	} else {
	  	/* we don't need a new priority structure, free it */
		kmem_free(newspp, sizeof(*newspp));
	}

	/*
	 * priority found (or created).   now insert on the priority's
	 * tailq list and bump the total number of swapdevs.
	 */
	sdp->swd_priority = priority;
	TAILQ_INSERT_TAIL(&spp->spi_swapdev, sdp, swd_next);
	uvmexp.nswapdev++;
}

/*
 * swaplist_find: find and optionally remove a swap device from the
 *	global list.
 *
 * => caller must hold both swap_syscall_lock and uvm_swap_data_lock
 * => we return the swapdev we found (and removed)
 */
static struct swapdev *
swaplist_find(struct vnode *vp, bool remove)
{
	struct swapdev *sdp;
	struct swappri *spp;

	/*
	 * search the lists for the requested vp
	 */

	LIST_FOREACH(spp, &swap_priority, spi_swappri) {
		TAILQ_FOREACH(sdp, &spp->spi_swapdev, swd_next) {
			if (sdp->swd_vp == vp) {
				if (remove) {
					TAILQ_REMOVE(&spp->spi_swapdev,
					    sdp, swd_next);
					uvmexp.nswapdev--;
				}
				return(sdp);
			}
		}
	}
	return (NULL);
}

/*
 * swaplist_trim: scan priority list for empty priority entries and kill
 *	them.
 *
 * => caller must hold both swap_syscall_lock and uvm_swap_data_lock
 */
static void
swaplist_trim(void)
{
	struct swappri *spp, *nextspp;

	LIST_FOREACH_SAFE(spp, &swap_priority, spi_swappri, nextspp) {
		if (!TAILQ_EMPTY(&spp->spi_swapdev))
			continue;
		LIST_REMOVE(spp, spi_swappri);
		kmem_free(spp, sizeof(*spp));
	}
}

/*
 * swapdrum_getsdp: given a page offset in /dev/drum, convert it back
 *	to the "swapdev" that maps that section of the drum.
 *
 * => each swapdev takes one big contig chunk of the drum
 * => caller must hold uvm_swap_data_lock
 */
static struct swapdev *
swapdrum_getsdp(int pgno)
{
	struct swapdev *sdp;
	struct swappri *spp;

	LIST_FOREACH(spp, &swap_priority, spi_swappri) {
		TAILQ_FOREACH(sdp, &spp->spi_swapdev, swd_next) {
			if (sdp->swd_flags & SWF_FAKE)
				continue;
			if (pgno >= sdp->swd_drumoffset &&
			    pgno < (sdp->swd_drumoffset + sdp->swd_drumsize)) {
				return sdp;
			}
		}
	}
	return NULL;
}

void swapsys_lock(krw_t op)
{
	rw_enter(&swap_syscall_lock, op);
}

void swapsys_unlock(void)
{
	rw_exit(&swap_syscall_lock);
}

/*
 * sys_swapctl: main entry point for swapctl(2) system call
 * 	[with two helper functions: swap_on and swap_off]
 */
int
sys_swapctl(struct lwp *l, const struct sys_swapctl_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) cmd;
		syscallarg(void *) arg;
		syscallarg(int) misc;
	} */
	struct vnode *vp;
	struct nameidata nd;
	struct swappri *spp;
	struct swapdev *sdp;
	struct swapent *sep;
#define SWAP_PATH_MAX (PATH_MAX + 1)
	char	*userpath;
	size_t	len = 0;
	int	error, misc;
	int	priority;
	UVMHIST_FUNC("sys_swapctl"); UVMHIST_CALLED(pdhist);

	/*
	 * we handle the non-priv NSWAP and STATS request first.
	 *
	 * SWAP_NSWAP: return number of config'd swap devices
	 * [can also be obtained with uvmexp sysctl]
	 */
	if (SCARG(uap, cmd) == SWAP_NSWAP) {
		const int nswapdev = uvmexp.nswapdev;
		UVMHIST_LOG(pdhist, "<- done SWAP_NSWAP=%d", nswapdev, 0, 0, 0);
		*retval = nswapdev;
		return 0;
	}

	misc = SCARG(uap, misc);
	userpath = kmem_alloc(SWAP_PATH_MAX, KM_SLEEP);

	/*
	 * ensure serialized syscall access by grabbing the swap_syscall_lock
	 */
	rw_enter(&swap_syscall_lock, RW_WRITER);

	/*
	 * SWAP_STATS: get stats on current # of configured swap devs
	 *
	 * note that the swap_priority list can't change as long
	 * as we are holding the swap_syscall_lock.  we don't want
	 * to grab the uvm_swap_data_lock because we may fault&sleep during
	 * copyout() and we don't want to be holding that lock then!
	 */
	if (SCARG(uap, cmd) == SWAP_STATS
#if defined(COMPAT_50)
	    || SCARG(uap, cmd) == SWAP_STATS50
#endif
#if defined(COMPAT_13)
	    || SCARG(uap, cmd) == SWAP_STATS13
#endif
	    ) {
		if (misc < 0) {
			error = EINVAL;
			goto out;
		}
		if (misc == 0 || uvmexp.nswapdev == 0) {
			error = 0;
			goto out;
		}
		/* Make sure userland cannot exhaust kernel memory */
		if ((size_t)misc > (size_t)uvmexp.nswapdev)
			misc = uvmexp.nswapdev;
		KASSERT(misc > 0);
#if defined(COMPAT_13)
		if (SCARG(uap, cmd) == SWAP_STATS13)
			len = sizeof(struct swapent13) * misc;
		else
#endif
#if defined(COMPAT_50)
		if (SCARG(uap, cmd) == SWAP_STATS50)
			len = sizeof(struct swapent50) * misc;
		else
#endif
			len = sizeof(struct swapent) * misc;
		sep = (struct swapent *)kmem_alloc(len, KM_SLEEP);

		uvm_swap_stats(SCARG(uap, cmd), sep, misc, retval);
		error = copyout(sep, SCARG(uap, arg), len);

		kmem_free(sep, len);
		UVMHIST_LOG(pdhist, "<- done SWAP_STATS", 0, 0, 0, 0);
		goto out;
	}
	if (SCARG(uap, cmd) == SWAP_GETDUMPDEV) {
		dev_t	*devp = (dev_t *)SCARG(uap, arg);

		error = copyout(&dumpdev, devp, sizeof(dumpdev));
		goto out;
	}

	/*
	 * all other requests require superuser privs.   verify.
	 */
	if ((error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_SWAPCTL,
	    0, NULL, NULL, NULL)))
		goto out;

	if (SCARG(uap, cmd) == SWAP_DUMPOFF) {
		/* drop the current dump device */
		dumpdev = NODEV;
		dumpcdev = NODEV;
		cpu_dumpconf();
		goto out;
	}

	/*
	 * at this point we expect a path name in arg.   we will
	 * use namei() to gain a vnode reference (vref), and lock
	 * the vnode (VOP_LOCK).
	 *
	 * XXX: a NULL arg means use the root vnode pointer (e.g. for
	 * miniroot)
	 */
	if (SCARG(uap, arg) == NULL) {
		vp = rootvp;		/* miniroot */
		vref(vp);
		if (vn_lock(vp, LK_EXCLUSIVE)) {
			vrele(vp);
			error = EBUSY;
			goto out;
		}
		if (SCARG(uap, cmd) == SWAP_ON &&
		    copystr("miniroot", userpath, SWAP_PATH_MAX, &len))
			panic("swapctl: miniroot copy failed");
	} else {
		struct pathbuf *pb;

		/*
		 * This used to allow copying in one extra byte
		 * (SWAP_PATH_MAX instead of PATH_MAX) for SWAP_ON.
		 * This was completely pointless because if anyone
		 * used that extra byte namei would fail with
		 * ENAMETOOLONG anyway, so I've removed the excess
		 * logic. - dholland 20100215
		 */

		error = pathbuf_copyin(SCARG(uap, arg), &pb);
		if (error) {
			goto out;
		}
		if (SCARG(uap, cmd) == SWAP_ON) {
			/* get a copy of the string */
			pathbuf_copystring(pb, userpath, SWAP_PATH_MAX);
			len = strlen(userpath) + 1;
		}
		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | TRYEMULROOT, pb);
		if ((error = namei(&nd))) {
			pathbuf_destroy(pb);
			goto out;
		}
		vp = nd.ni_vp;
		pathbuf_destroy(pb);
	}
	/* note: "vp" is referenced and locked */

	error = 0;		/* assume no error */
	switch(SCARG(uap, cmd)) {

	case SWAP_DUMPDEV:
		if (vp->v_type != VBLK) {
			error = ENOTBLK;
			break;
		}
		if (bdevsw_lookup(vp->v_rdev)) {
			dumpdev = vp->v_rdev;
			dumpcdev = devsw_blk2chr(dumpdev);
		} else
			dumpdev = NODEV;
		cpu_dumpconf();
		break;

	case SWAP_CTL:
		/*
		 * get new priority, remove old entry (if any) and then
		 * reinsert it in the correct place.  finally, prune out
		 * any empty priority structures.
		 */
		priority = SCARG(uap, misc);
		spp = kmem_alloc(sizeof(*spp), KM_SLEEP);
		mutex_enter(&uvm_swap_data_lock);
		if ((sdp = swaplist_find(vp, true)) == NULL) {
			error = ENOENT;
		} else {
			swaplist_insert(sdp, spp, priority);
			swaplist_trim();
		}
		mutex_exit(&uvm_swap_data_lock);
		if (error)
			kmem_free(spp, sizeof(*spp));
		break;

	case SWAP_ON:

		/*
		 * check for duplicates.   if none found, then insert a
		 * dummy entry on the list to prevent someone else from
		 * trying to enable this device while we are working on
		 * it.
		 */

		priority = SCARG(uap, misc);
		sdp = kmem_zalloc(sizeof(*sdp), KM_SLEEP);
		spp = kmem_alloc(sizeof(*spp), KM_SLEEP);
		sdp->swd_flags = SWF_FAKE;
		sdp->swd_vp = vp;
		sdp->swd_dev = (vp->v_type == VBLK) ? vp->v_rdev : NODEV;
		bufq_alloc(&sdp->swd_tab, "disksort", BUFQ_SORT_RAWBLOCK);
		mutex_enter(&uvm_swap_data_lock);
		if (swaplist_find(vp, false) != NULL) {
			error = EBUSY;
			mutex_exit(&uvm_swap_data_lock);
			bufq_free(sdp->swd_tab);
			kmem_free(sdp, sizeof(*sdp));
			kmem_free(spp, sizeof(*spp));
			break;
		}
		swaplist_insert(sdp, spp, priority);
		mutex_exit(&uvm_swap_data_lock);

		KASSERT(len > 0);
		sdp->swd_pathlen = len;
		sdp->swd_path = kmem_alloc(len, KM_SLEEP);
		if (copystr(userpath, sdp->swd_path, len, 0) != 0)
			panic("swapctl: copystr");

		/*
		 * we've now got a FAKE placeholder in the swap list.
		 * now attempt to enable swap on it.  if we fail, undo
		 * what we've done and kill the fake entry we just inserted.
		 * if swap_on is a success, it will clear the SWF_FAKE flag
		 */

		if ((error = swap_on(l, sdp)) != 0) {
			mutex_enter(&uvm_swap_data_lock);
			(void) swaplist_find(vp, true);  /* kill fake entry */
			swaplist_trim();
			mutex_exit(&uvm_swap_data_lock);
			bufq_free(sdp->swd_tab);
			kmem_free(sdp->swd_path, sdp->swd_pathlen);
			kmem_free(sdp, sizeof(*sdp));
			break;
		}
		break;

	case SWAP_OFF:
		mutex_enter(&uvm_swap_data_lock);
		if ((sdp = swaplist_find(vp, false)) == NULL) {
			mutex_exit(&uvm_swap_data_lock);
			error = ENXIO;
			break;
		}

		/*
		 * If a device isn't in use or enabled, we
		 * can't stop swapping from it (again).
		 */
		if ((sdp->swd_flags & (SWF_INUSE|SWF_ENABLE)) == 0) {
			mutex_exit(&uvm_swap_data_lock);
			error = EBUSY;
			break;
		}

		/*
		 * do the real work.
		 */
		error = swap_off(l, sdp);
		break;

	default:
		error = EINVAL;
	}

	/*
	 * done!  release the ref gained by namei() and unlock.
	 */
	vput(vp);
out:
	rw_exit(&swap_syscall_lock);
	kmem_free(userpath, SWAP_PATH_MAX);

	UVMHIST_LOG(pdhist, "<- done!  error=%d", error, 0, 0, 0);
	return (error);
}

/*
 * uvm_swap_stats: implements swapctl(SWAP_STATS). The function is kept
 * away from sys_swapctl() in order to allow COMPAT_* swapctl()
 * emulation to use it directly without going through sys_swapctl().
 * The problem with using sys_swapctl() there is that it involves
 * copying the swapent array to the stackgap, and this array's size
 * is not known at build time. Hence it would not be possible to
 * ensure it would fit in the stackgap in any case.
 */
void
uvm_swap_stats(int cmd, struct swapent *sep, int sec, register_t *retval)
{
	struct swappri *spp;
	struct swapdev *sdp;
	int count = 0;

	KASSERT(rw_lock_held(&swap_syscall_lock));

	LIST_FOREACH(spp, &swap_priority, spi_swappri) {
		TAILQ_FOREACH(sdp, &spp->spi_swapdev, swd_next) {
			int inuse;

			if (sec-- <= 0)
				break;

			/*
			 * backwards compatibility for system call.
			 * For NetBSD 1.3 and 5.0, we have to use
			 * the 32 bit dev_t.  For 5.0 and -current
			 * we have to add the path.
			 */
			inuse = btodb((uint64_t)sdp->swd_npginuse <<
			    PAGE_SHIFT);

#if defined(COMPAT_13) || defined(COMPAT_50)
			if (cmd == SWAP_STATS) {
#endif
				sep->se_dev = sdp->swd_dev;
				sep->se_flags = sdp->swd_flags;
				sep->se_nblks = sdp->swd_nblks;
				sep->se_inuse = inuse;
				sep->se_priority = sdp->swd_priority;
				KASSERT(sdp->swd_pathlen <
				    sizeof(sep->se_path));
				strcpy(sep->se_path, sdp->swd_path);
				sep++;
#if defined(COMPAT_13)
			} else if (cmd == SWAP_STATS13) {
				struct swapent13 *sep13 =
				    (struct swapent13 *)sep;

				sep13->se13_dev = sdp->swd_dev;
				sep13->se13_flags = sdp->swd_flags;
				sep13->se13_nblks = sdp->swd_nblks;
				sep13->se13_inuse = inuse;
				sep13->se13_priority = sdp->swd_priority;
				sep = (struct swapent *)(sep13 + 1);
#endif
#if defined(COMPAT_50)
			} else if (cmd == SWAP_STATS50) {
				struct swapent50 *sep50 =
				    (struct swapent50 *)sep;

				sep50->se50_dev = sdp->swd_dev;
				sep50->se50_flags = sdp->swd_flags;
				sep50->se50_nblks = sdp->swd_nblks;
				sep50->se50_inuse = inuse;
				sep50->se50_priority = sdp->swd_priority;
				KASSERT(sdp->swd_pathlen <
				    sizeof(sep50->se50_path));
				strcpy(sep50->se50_path, sdp->swd_path);
				sep = (struct swapent *)(sep50 + 1);
#endif
#if defined(COMPAT_13) || defined(COMPAT_50)
			}
#endif
			count++;
		}
	}
	*retval = count;
}

/*
 * swap_on: attempt to enable a swapdev for swapping.   note that the
 *	swapdev is already on the global list, but disabled (marked
 *	SWF_FAKE).
 *
 * => we avoid the start of the disk (to protect disk labels)
 * => we also avoid the miniroot, if we are swapping to root.
 * => caller should leave uvm_swap_data_lock unlocked, we may lock it
 *	if needed.
 */
static int
swap_on(struct lwp *l, struct swapdev *sdp)
{
	struct vnode *vp;
	int error, npages, nblocks, size;
	long addr;
	vmem_addr_t result;
	struct vattr va;
	dev_t dev;
	UVMHIST_FUNC("swap_on"); UVMHIST_CALLED(pdhist);

	/*
	 * we want to enable swapping on sdp.   the swd_vp contains
	 * the vnode we want (locked and ref'd), and the swd_dev
	 * contains the dev_t of the file, if it a block device.
	 */

	vp = sdp->swd_vp;
	dev = sdp->swd_dev;

	/*
	 * open the swap file (mostly useful for block device files to
	 * let device driver know what is up).
	 *
	 * we skip the open/close for root on swap because the root
	 * has already been opened when root was mounted (mountroot).
	 */
	if (vp != rootvp) {
		if ((error = VOP_OPEN(vp, FREAD|FWRITE, l->l_cred)))
			return (error);
	}

	/* XXX this only works for block devices */
	UVMHIST_LOG(pdhist, "  dev=%d, major(dev)=%d", dev, major(dev), 0,0);

	/*
	 * we now need to determine the size of the swap area.   for
	 * block specials we can call the d_psize function.
	 * for normal files, we must stat [get attrs].
	 *
	 * we put the result in nblks.
	 * for normal files, we also want the filesystem block size
	 * (which we get with statfs).
	 */
	switch (vp->v_type) {
	case VBLK:
		if ((nblocks = bdev_size(dev)) == -1) {
			error = ENXIO;
			goto bad;
		}
		break;

	case VREG:
		if ((error = VOP_GETATTR(vp, &va, l->l_cred)))
			goto bad;
		nblocks = (int)btodb(va.va_size);
		sdp->swd_bsize = 1 << vp->v_mount->mnt_fs_bshift;
		/*
		 * limit the max # of outstanding I/O requests we issue
		 * at any one time.   take it easy on NFS servers.
		 */
		if (vp->v_tag == VT_NFS)
			sdp->swd_maxactive = 2; /* XXX */
		else
			sdp->swd_maxactive = 8; /* XXX */
		break;

	default:
		error = ENXIO;
		goto bad;
	}

	/*
	 * save nblocks in a safe place and convert to pages.
	 */

	sdp->swd_nblks = nblocks;
	npages = dbtob((uint64_t)nblocks) >> PAGE_SHIFT;

	/*
	 * for block special files, we want to make sure that leave
	 * the disklabel and bootblocks alone, so we arrange to skip
	 * over them (arbitrarily choosing to skip PAGE_SIZE bytes).
	 * note that because of this the "size" can be less than the
	 * actual number of blocks on the device.
	 */
	if (vp->v_type == VBLK) {
		/* we use pages 1 to (size - 1) [inclusive] */
		size = npages - 1;
		addr = 1;
	} else {
		/* we use pages 0 to (size - 1) [inclusive] */
		size = npages;
		addr = 0;
	}

	/*
	 * make sure we have enough blocks for a reasonable sized swap
	 * area.   we want at least one page.
	 */

	if (size < 1) {
		UVMHIST_LOG(pdhist, "  size <= 1!!", 0, 0, 0, 0);
		error = EINVAL;
		goto bad;
	}

	UVMHIST_LOG(pdhist, "  dev=%x: size=%d addr=%ld\n", dev, size, addr, 0);

	/*
	 * now we need to allocate an extent to manage this swap device
	 */

	sdp->swd_blist = blist_create(npages);
	/* mark all expect the `saved' region free. */
	blist_free(sdp->swd_blist, addr, size);

	/*
	 * if the vnode we are swapping to is the root vnode
	 * (i.e. we are swapping to the miniroot) then we want
	 * to make sure we don't overwrite it.   do a statfs to
	 * find its size and skip over it.
	 */
	if (vp == rootvp) {
		struct mount *mp;
		struct statvfs *sp;
		int rootblocks, rootpages;

		mp = rootvnode->v_mount;
		sp = &mp->mnt_stat;
		rootblocks = sp->f_blocks * btodb(sp->f_frsize);
		/*
		 * XXX: sp->f_blocks isn't the total number of
		 * blocks in the filesystem, it's the number of
		 * data blocks.  so, our rootblocks almost
		 * definitely underestimates the total size
		 * of the filesystem - how badly depends on the
		 * details of the filesystem type.  there isn't
		 * an obvious way to deal with this cleanly
		 * and perfectly, so for now we just pad our
		 * rootblocks estimate with an extra 5 percent.
		 */
		rootblocks += (rootblocks >> 5) +
			(rootblocks >> 6) +
			(rootblocks >> 7);
		rootpages = round_page(dbtob(rootblocks)) >> PAGE_SHIFT;
		if (rootpages > size)
			panic("swap_on: miniroot larger than swap?");

		if (rootpages != blist_fill(sdp->swd_blist, addr, rootpages)) {
			panic("swap_on: unable to preserve miniroot");
		}

		size -= rootpages;
		printf("Preserved %d pages of miniroot ", rootpages);
		printf("leaving %d pages of swap\n", size);
	}

	/*
	 * add a ref to vp to reflect usage as a swap device.
	 */
	vref(vp);

	/*
	 * now add the new swapdev to the drum and enable.
	 */
	error = vmem_alloc(swapmap, npages, VM_BESTFIT | VM_SLEEP, &result);
	if (error != 0)
		panic("swapdrum_add");
	/*
	 * If this is the first regular swap create the workqueue.
	 * => Protected by swap_syscall_lock.
	 */
	if (vp->v_type != VBLK) {
		if (sw_reg_count++ == 0) {
			KASSERT(sw_reg_workqueue == NULL);
			if (workqueue_create(&sw_reg_workqueue, "swapiod",
			    sw_reg_iodone, NULL, PRIBIO, IPL_BIO, 0) != 0)
				panic("%s: workqueue_create failed", __func__);
		}
	}

	sdp->swd_drumoffset = (int)result;
	sdp->swd_drumsize = npages;
	sdp->swd_npages = size;
	mutex_enter(&uvm_swap_data_lock);
	sdp->swd_flags &= ~SWF_FAKE;	/* going live */
	sdp->swd_flags |= (SWF_INUSE|SWF_ENABLE);
	uvmexp.swpages += size;
	uvmexp.swpgavail += size;
	mutex_exit(&uvm_swap_data_lock);
	return (0);

	/*
	 * failure: clean up and return error.
	 */

bad:
	if (sdp->swd_blist) {
		blist_destroy(sdp->swd_blist);
	}
	if (vp != rootvp) {
		(void)VOP_CLOSE(vp, FREAD|FWRITE, l->l_cred);
	}
	return (error);
}

/*
 * swap_off: stop swapping on swapdev
 *
 * => swap data should be locked, we will unlock.
 */
static int
swap_off(struct lwp *l, struct swapdev *sdp)
{
	int npages = sdp->swd_npages;
	int error = 0;

	UVMHIST_FUNC("swap_off"); UVMHIST_CALLED(pdhist);
	UVMHIST_LOG(pdhist, "  dev=%x, npages=%d", sdp->swd_dev,npages,0,0);

	/* disable the swap area being removed */
	sdp->swd_flags &= ~SWF_ENABLE;
	uvmexp.swpgavail -= npages;
	mutex_exit(&uvm_swap_data_lock);

	/*
	 * the idea is to find all the pages that are paged out to this
	 * device, and page them all in.  in uvm, swap-backed pageable
	 * memory can take two forms: aobjs and anons.  call the
	 * swapoff hook for each subsystem to bring in pages.
	 */

	if (uao_swap_off(sdp->swd_drumoffset,
			 sdp->swd_drumoffset + sdp->swd_drumsize) ||
	    amap_swap_off(sdp->swd_drumoffset,
			  sdp->swd_drumoffset + sdp->swd_drumsize)) {
		error = ENOMEM;
	} else if (sdp->swd_npginuse > sdp->swd_npgbad) {
		error = EBUSY;
	}

	if (error) {
		mutex_enter(&uvm_swap_data_lock);
		sdp->swd_flags |= SWF_ENABLE;
		uvmexp.swpgavail += npages;
		mutex_exit(&uvm_swap_data_lock);

		return error;
	}

	/*
	 * If this is the last regular swap destroy the workqueue.
	 * => Protected by swap_syscall_lock.
	 */
	if (sdp->swd_vp->v_type != VBLK) {
		KASSERT(sw_reg_count > 0);
		KASSERT(sw_reg_workqueue != NULL);
		if (--sw_reg_count == 0) {
			workqueue_destroy(sw_reg_workqueue);
			sw_reg_workqueue = NULL;
		}
	}

	/*
	 * done with the vnode.
	 * drop our ref on the vnode before calling VOP_CLOSE()
	 * so that spec_close() can tell if this is the last close.
	 */
	vrele(sdp->swd_vp);
	if (sdp->swd_vp != rootvp) {
		(void) VOP_CLOSE(sdp->swd_vp, FREAD|FWRITE, l->l_cred);
	}

	mutex_enter(&uvm_swap_data_lock);
	uvmexp.swpages -= npages;
	uvmexp.swpginuse -= sdp->swd_npgbad;

	if (swaplist_find(sdp->swd_vp, true) == NULL)
		panic("%s: swapdev not in list", __func__);
	swaplist_trim();
	mutex_exit(&uvm_swap_data_lock);

	/*
	 * free all resources!
	 */
	vmem_free(swapmap, sdp->swd_drumoffset, sdp->swd_drumsize);
	blist_destroy(sdp->swd_blist);
	bufq_free(sdp->swd_tab);
	kmem_free(sdp, sizeof(*sdp));
	return (0);
}

void
uvm_swap_shutdown(struct lwp *l)
{
	struct swapdev *sdp;
	struct swappri *spp;
	struct vnode *vp;
	int error;

	printf("turning of swap...");
	rw_enter(&swap_syscall_lock, RW_WRITER);
	mutex_enter(&uvm_swap_data_lock);
again:
	LIST_FOREACH(spp, &swap_priority, spi_swappri)
		TAILQ_FOREACH(sdp, &spp->spi_swapdev, swd_next) {
			if (sdp->swd_flags & SWF_FAKE)
				continue;
			if ((sdp->swd_flags & (SWF_INUSE|SWF_ENABLE)) == 0)
				continue;
#ifdef DEBUG
			printf("\nturning off swap on %s...",
			    sdp->swd_path);
#endif
			if (vn_lock(vp = sdp->swd_vp, LK_EXCLUSIVE)) {
				error = EBUSY;
				vp = NULL;
			} else
				error = 0;
			if (!error) {
				error = swap_off(l, sdp);
				mutex_enter(&uvm_swap_data_lock);
			}
			if (error) {
				printf("stopping swap on %s failed "
				    "with error %d\n", sdp->swd_path, error);
				TAILQ_REMOVE(&spp->spi_swapdev, sdp,
				    swd_next);
				uvmexp.nswapdev--;
				swaplist_trim();
				if (vp)
					vput(vp);
			}
			goto again;
		}
	printf(" done\n");
	mutex_exit(&uvm_swap_data_lock);
	rw_exit(&swap_syscall_lock);
}


/*
 * /dev/drum interface and i/o functions
 */

/*
 * swstrategy: perform I/O on the drum
 *
 * => we must map the i/o request from the drum to the correct swapdev.
 */
static void
swstrategy(struct buf *bp)
{
	struct swapdev *sdp;
	struct vnode *vp;
	int pageno, bn;
	UVMHIST_FUNC("swstrategy"); UVMHIST_CALLED(pdhist);

	/*
	 * convert block number to swapdev.   note that swapdev can't
	 * be yanked out from under us because we are holding resources
	 * in it (i.e. the blocks we are doing I/O on).
	 */
	pageno = dbtob((int64_t)bp->b_blkno) >> PAGE_SHIFT;
	mutex_enter(&uvm_swap_data_lock);
	sdp = swapdrum_getsdp(pageno);
	mutex_exit(&uvm_swap_data_lock);
	if (sdp == NULL) {
		bp->b_error = EINVAL;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		UVMHIST_LOG(pdhist, "  failed to get swap device", 0, 0, 0, 0);
		return;
	}

	/*
	 * convert drum page number to block number on this swapdev.
	 */

	pageno -= sdp->swd_drumoffset;	/* page # on swapdev */
	bn = btodb((uint64_t)pageno << PAGE_SHIFT); /* convert to diskblock */

	UVMHIST_LOG(pdhist, "  %s: mapoff=%x bn=%x bcount=%ld",
		((bp->b_flags & B_READ) == 0) ? "write" : "read",
		sdp->swd_drumoffset, bn, bp->b_bcount);

	/*
	 * for block devices we finish up here.
	 * for regular files we have to do more work which we delegate
	 * to sw_reg_strategy().
	 */

	vp = sdp->swd_vp;		/* swapdev vnode pointer */
	switch (vp->v_type) {
	default:
		panic("%s: vnode type 0x%x", __func__, vp->v_type);

	case VBLK:

		/*
		 * must convert "bp" from an I/O on /dev/drum to an I/O
		 * on the swapdev (sdp).
		 */
		bp->b_blkno = bn;		/* swapdev block number */
		bp->b_dev = sdp->swd_dev;	/* swapdev dev_t */

		/*
		 * if we are doing a write, we have to redirect the i/o on
		 * drum's v_numoutput counter to the swapdevs.
		 */
		if ((bp->b_flags & B_READ) == 0) {
			mutex_enter(bp->b_objlock);
			vwakeup(bp);	/* kills one 'v_numoutput' on drum */
			mutex_exit(bp->b_objlock);
			mutex_enter(vp->v_interlock);
			vp->v_numoutput++;	/* put it on swapdev */
			mutex_exit(vp->v_interlock);
		}

		/*
		 * finally plug in swapdev vnode and start I/O
		 */
		bp->b_vp = vp;
		bp->b_objlock = vp->v_interlock;
		VOP_STRATEGY(vp, bp);
		return;

	case VREG:
		/*
		 * delegate to sw_reg_strategy function.
		 */
		sw_reg_strategy(sdp, bp, bn);
		return;
	}
	/* NOTREACHED */
}

/*
 * swread: the read function for the drum (just a call to physio)
 */
/*ARGSUSED*/
static int
swread(dev_t dev, struct uio *uio, int ioflag)
{
	UVMHIST_FUNC("swread"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "  dev=%x offset=%qx", dev, uio->uio_offset, 0, 0);
	return (physio(swstrategy, NULL, dev, B_READ, minphys, uio));
}

/*
 * swwrite: the write function for the drum (just a call to physio)
 */
/*ARGSUSED*/
static int
swwrite(dev_t dev, struct uio *uio, int ioflag)
{
	UVMHIST_FUNC("swwrite"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "  dev=%x offset=%qx", dev, uio->uio_offset, 0, 0);
	return (physio(swstrategy, NULL, dev, B_WRITE, minphys, uio));
}

const struct bdevsw swap_bdevsw = {
	.d_open = nullopen,
	.d_close = nullclose,
	.d_strategy = swstrategy,
	.d_ioctl = noioctl,
	.d_dump = nodump,
	.d_psize = nosize,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

const struct cdevsw swap_cdevsw = {
	.d_open = nullopen,
	.d_close = nullclose,
	.d_read = swread,
	.d_write = swwrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER,
};

/*
 * sw_reg_strategy: handle swap i/o to regular files
 */
static void
sw_reg_strategy(struct swapdev *sdp, struct buf *bp, int bn)
{
	struct vnode	*vp;
	struct vndxfer	*vnx;
	daddr_t		nbn;
	char 		*addr;
	off_t		byteoff;
	int		s, off, nra, error, sz, resid;
	UVMHIST_FUNC("sw_reg_strategy"); UVMHIST_CALLED(pdhist);

	/*
	 * allocate a vndxfer head for this transfer and point it to
	 * our buffer.
	 */
	vnx = pool_get(&vndxfer_pool, PR_WAITOK);
	vnx->vx_flags = VX_BUSY;
	vnx->vx_error = 0;
	vnx->vx_pending = 0;
	vnx->vx_bp = bp;
	vnx->vx_sdp = sdp;

	/*
	 * setup for main loop where we read filesystem blocks into
	 * our buffer.
	 */
	error = 0;
	bp->b_resid = bp->b_bcount;	/* nothing transfered yet! */
	addr = bp->b_data;		/* current position in buffer */
	byteoff = dbtob((uint64_t)bn);

	for (resid = bp->b_resid; resid; resid -= sz) {
		struct vndbuf	*nbp;

		/*
		 * translate byteoffset into block number.  return values:
		 *   vp = vnode of underlying device
		 *  nbn = new block number (on underlying vnode dev)
		 *  nra = num blocks we can read-ahead (excludes requested
		 *	block)
		 */
		nra = 0;
		error = VOP_BMAP(sdp->swd_vp, byteoff / sdp->swd_bsize,
				 	&vp, &nbn, &nra);

		if (error == 0 && nbn == (daddr_t)-1) {
			/*
			 * this used to just set error, but that doesn't
			 * do the right thing.  Instead, it causes random
			 * memory errors.  The panic() should remain until
			 * this condition doesn't destabilize the system.
			 */
#if 1
			panic("%s: swap to sparse file", __func__);
#else
			error = EIO;	/* failure */
#endif
		}

		/*
		 * punt if there was an error or a hole in the file.
		 * we must wait for any i/o ops we have already started
		 * to finish before returning.
		 *
		 * XXX we could deal with holes here but it would be
		 * a hassle (in the write case).
		 */
		if (error) {
			s = splbio();
			vnx->vx_error = error;	/* pass error up */
			goto out;
		}

		/*
		 * compute the size ("sz") of this transfer (in bytes).
		 */
		off = byteoff % sdp->swd_bsize;
		sz = (1 + nra) * sdp->swd_bsize - off;
		if (sz > resid)
			sz = resid;

		UVMHIST_LOG(pdhist, "sw_reg_strategy: "
			    "vp %p/%p offset 0x%x/0x%x",
			    sdp->swd_vp, vp, byteoff, nbn);

		/*
		 * now get a buf structure.   note that the vb_buf is
		 * at the front of the nbp structure so that you can
		 * cast pointers between the two structure easily.
		 */
		nbp = pool_get(&vndbuf_pool, PR_WAITOK);
		buf_init(&nbp->vb_buf);
		nbp->vb_buf.b_flags    = bp->b_flags;
		nbp->vb_buf.b_cflags   = bp->b_cflags;
		nbp->vb_buf.b_oflags   = bp->b_oflags;
		nbp->vb_buf.b_bcount   = sz;
		nbp->vb_buf.b_bufsize  = sz;
		nbp->vb_buf.b_error    = 0;
		nbp->vb_buf.b_data     = addr;
		nbp->vb_buf.b_lblkno   = 0;
		nbp->vb_buf.b_blkno    = nbn + btodb(off);
		nbp->vb_buf.b_rawblkno = nbp->vb_buf.b_blkno;
		nbp->vb_buf.b_iodone   = sw_reg_biodone;
		nbp->vb_buf.b_vp       = vp;
		nbp->vb_buf.b_objlock  = vp->v_interlock;
		if (vp->v_type == VBLK) {
			nbp->vb_buf.b_dev = vp->v_rdev;
		}

		nbp->vb_xfer = vnx;	/* patch it back in to vnx */

		/*
		 * Just sort by block number
		 */
		s = splbio();
		if (vnx->vx_error != 0) {
			buf_destroy(&nbp->vb_buf);
			pool_put(&vndbuf_pool, nbp);
			goto out;
		}
		vnx->vx_pending++;

		/* sort it in and start I/O if we are not over our limit */
		/* XXXAD locking */
		bufq_put(sdp->swd_tab, &nbp->vb_buf);
		sw_reg_start(sdp);
		splx(s);

		/*
		 * advance to the next I/O
		 */
		byteoff += sz;
		addr += sz;
	}

	s = splbio();

out: /* Arrive here at splbio */
	vnx->vx_flags &= ~VX_BUSY;
	if (vnx->vx_pending == 0) {
		error = vnx->vx_error;
		pool_put(&vndxfer_pool, vnx);
		bp->b_error = error;
		biodone(bp);
	}
	splx(s);
}

/*
 * sw_reg_start: start an I/O request on the requested swapdev
 *
 * => reqs are sorted by b_rawblkno (above)
 */
static void
sw_reg_start(struct swapdev *sdp)
{
	struct buf	*bp;
	struct vnode	*vp;
	UVMHIST_FUNC("sw_reg_start"); UVMHIST_CALLED(pdhist);

	/* recursion control */
	if ((sdp->swd_flags & SWF_BUSY) != 0)
		return;

	sdp->swd_flags |= SWF_BUSY;

	while (sdp->swd_active < sdp->swd_maxactive) {
		bp = bufq_get(sdp->swd_tab);
		if (bp == NULL)
			break;
		sdp->swd_active++;

		UVMHIST_LOG(pdhist,
		    "sw_reg_start:  bp %p vp %p blkno %p cnt %lx",
		    bp, bp->b_vp, bp->b_blkno, bp->b_bcount);
		vp = bp->b_vp;
		KASSERT(bp->b_objlock == vp->v_interlock);
		if ((bp->b_flags & B_READ) == 0) {
			mutex_enter(vp->v_interlock);
			vp->v_numoutput++;
			mutex_exit(vp->v_interlock);
		}
		VOP_STRATEGY(vp, bp);
	}
	sdp->swd_flags &= ~SWF_BUSY;
}

/*
 * sw_reg_biodone: one of our i/o's has completed
 */
static void
sw_reg_biodone(struct buf *bp)
{
	workqueue_enqueue(sw_reg_workqueue, &bp->b_work, NULL);
}

/*
 * sw_reg_iodone: one of our i/o's has completed and needs post-i/o cleanup
 *
 * => note that we can recover the vndbuf struct by casting the buf ptr
 */
static void
sw_reg_iodone(struct work *wk, void *dummy)
{
	struct vndbuf *vbp = (void *)wk;
	struct vndxfer *vnx = vbp->vb_xfer;
	struct buf *pbp = vnx->vx_bp;		/* parent buffer */
	struct swapdev	*sdp = vnx->vx_sdp;
	int s, resid, error;
	KASSERT(&vbp->vb_buf.b_work == wk);
	UVMHIST_FUNC("sw_reg_iodone"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "  vbp=%p vp=%p blkno=%x addr=%p",
	    vbp, vbp->vb_buf.b_vp, vbp->vb_buf.b_blkno, vbp->vb_buf.b_data);
	UVMHIST_LOG(pdhist, "  cnt=%lx resid=%lx",
	    vbp->vb_buf.b_bcount, vbp->vb_buf.b_resid, 0, 0);

	/*
	 * protect vbp at splbio and update.
	 */

	s = splbio();
	resid = vbp->vb_buf.b_bcount - vbp->vb_buf.b_resid;
	pbp->b_resid -= resid;
	vnx->vx_pending--;

	if (vbp->vb_buf.b_error != 0) {
		/* pass error upward */
		error = vbp->vb_buf.b_error ? vbp->vb_buf.b_error : EIO;
		UVMHIST_LOG(pdhist, "  got error=%d !", error, 0, 0, 0);
		vnx->vx_error = error;
	}

	/*
	 * kill vbp structure
	 */
	buf_destroy(&vbp->vb_buf);
	pool_put(&vndbuf_pool, vbp);

	/*
	 * wrap up this transaction if it has run to completion or, in
	 * case of an error, when all auxiliary buffers have returned.
	 */
	if (vnx->vx_error != 0) {
		/* pass error upward */
		error = vnx->vx_error;
		if ((vnx->vx_flags & VX_BUSY) == 0 && vnx->vx_pending == 0) {
			pbp->b_error = error;
			biodone(pbp);
			pool_put(&vndxfer_pool, vnx);
		}
	} else if (pbp->b_resid == 0) {
		KASSERT(vnx->vx_pending == 0);
		if ((vnx->vx_flags & VX_BUSY) == 0) {
			UVMHIST_LOG(pdhist, "  iodone error=%d !",
			    pbp, vnx->vx_error, 0, 0);
			biodone(pbp);
			pool_put(&vndxfer_pool, vnx);
		}
	}

	/*
	 * done!   start next swapdev I/O if one is pending
	 */
	sdp->swd_active--;
	sw_reg_start(sdp);
	splx(s);
}


/*
 * uvm_swap_alloc: allocate space on swap
 *
 * => allocation is done "round robin" down the priority list, as we
 *	allocate in a priority we "rotate" the circle queue.
 * => space can be freed with uvm_swap_free
 * => we return the page slot number in /dev/drum (0 == invalid slot)
 * => we lock uvm_swap_data_lock
 * => XXXMRG: "LESSOK" INTERFACE NEEDED TO EXTENT SYSTEM
 */
int
uvm_swap_alloc(int *nslots /* IN/OUT */, bool lessok)
{
	struct swapdev *sdp;
	struct swappri *spp;
	UVMHIST_FUNC("uvm_swap_alloc"); UVMHIST_CALLED(pdhist);

	/*
	 * no swap devices configured yet?   definite failure.
	 */
	if (uvmexp.nswapdev < 1)
		return 0;

	/*
	 * XXXJAK: BEGIN HACK
	 *
	 * blist_alloc() in subr_blist.c will panic if we try to allocate
	 * too many slots.
	 */
	if (*nslots > BLIST_MAX_ALLOC) {
		if (__predict_false(lessok == false))
			return 0;
		*nslots = BLIST_MAX_ALLOC;
	}
	/* XXXJAK: END HACK */

	/*
	 * lock data lock, convert slots into blocks, and enter loop
	 */
	mutex_enter(&uvm_swap_data_lock);

ReTry:	/* XXXMRG */
	LIST_FOREACH(spp, &swap_priority, spi_swappri) {
		TAILQ_FOREACH(sdp, &spp->spi_swapdev, swd_next) {
			uint64_t result;

			/* if it's not enabled, then we can't swap from it */
			if ((sdp->swd_flags & SWF_ENABLE) == 0)
				continue;
			if (sdp->swd_npginuse + *nslots > sdp->swd_npages)
				continue;
			result = blist_alloc(sdp->swd_blist, *nslots);
			if (result == BLIST_NONE) {
				continue;
			}
			KASSERT(result < sdp->swd_drumsize);

			/*
			 * successful allocation!  now rotate the tailq.
			 */
			TAILQ_REMOVE(&spp->spi_swapdev, sdp, swd_next);
			TAILQ_INSERT_TAIL(&spp->spi_swapdev, sdp, swd_next);
			sdp->swd_npginuse += *nslots;
			uvmexp.swpginuse += *nslots;
			mutex_exit(&uvm_swap_data_lock);
			/* done!  return drum slot number */
			UVMHIST_LOG(pdhist,
			    "success!  returning %d slots starting at %d",
			    *nslots, result + sdp->swd_drumoffset, 0, 0);
			return (result + sdp->swd_drumoffset);
		}
	}

	/* XXXMRG: BEGIN HACK */
	if (*nslots > 1 && lessok) {
		*nslots = 1;
		/* XXXMRG: ugh!  blist should support this for us */
		goto ReTry;
	}
	/* XXXMRG: END HACK */

	mutex_exit(&uvm_swap_data_lock);
	return 0;
}

/*
 * uvm_swapisfull: return true if most of available swap is allocated
 * and in use.  we don't count some small portion as it may be inaccessible
 * to us at any given moment, for example if there is lock contention or if
 * pages are busy.
 */
bool
uvm_swapisfull(void)
{
	int swpgonly;
	bool rv;

	mutex_enter(&uvm_swap_data_lock);
	KASSERT(uvmexp.swpgonly <= uvmexp.swpages);
	swpgonly = (int)((uint64_t)uvmexp.swpgonly * 100 /
	    uvm_swapisfull_factor);
	rv = (swpgonly >= uvmexp.swpgavail);
	mutex_exit(&uvm_swap_data_lock);

	return (rv);
}

/*
 * uvm_swap_markbad: keep track of swap ranges where we've had i/o errors
 *
 * => we lock uvm_swap_data_lock
 */
void
uvm_swap_markbad(int startslot, int nslots)
{
	struct swapdev *sdp;
	UVMHIST_FUNC("uvm_swap_markbad"); UVMHIST_CALLED(pdhist);

	mutex_enter(&uvm_swap_data_lock);
	sdp = swapdrum_getsdp(startslot);
	KASSERT(sdp != NULL);

	/*
	 * we just keep track of how many pages have been marked bad
	 * in this device, to make everything add up in swap_off().
	 * we assume here that the range of slots will all be within
	 * one swap device.
	 */

	KASSERT(uvmexp.swpgonly >= nslots);
	uvmexp.swpgonly -= nslots;
	sdp->swd_npgbad += nslots;
	UVMHIST_LOG(pdhist, "now %d bad", sdp->swd_npgbad, 0,0,0);
	mutex_exit(&uvm_swap_data_lock);
}

/*
 * uvm_swap_free: free swap slots
 *
 * => this can be all or part of an allocation made by uvm_swap_alloc
 * => we lock uvm_swap_data_lock
 */
void
uvm_swap_free(int startslot, int nslots)
{
	struct swapdev *sdp;
	UVMHIST_FUNC("uvm_swap_free"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "freeing %d slots starting at %d", nslots,
	    startslot, 0, 0);

	/*
	 * ignore attempts to free the "bad" slot.
	 */

	if (startslot == SWSLOT_BAD) {
		return;
	}

	/*
	 * convert drum slot offset back to sdp, free the blocks
	 * in the extent, and return.   must hold pri lock to do
	 * lookup and access the extent.
	 */

	mutex_enter(&uvm_swap_data_lock);
	sdp = swapdrum_getsdp(startslot);
	KASSERT(uvmexp.nswapdev >= 1);
	KASSERT(sdp != NULL);
	KASSERT(sdp->swd_npginuse >= nslots);
	blist_free(sdp->swd_blist, startslot - sdp->swd_drumoffset, nslots);
	sdp->swd_npginuse -= nslots;
	uvmexp.swpginuse -= nslots;
	mutex_exit(&uvm_swap_data_lock);
}

/*
 * uvm_swap_put: put any number of pages into a contig place on swap
 *
 * => can be sync or async
 */

int
uvm_swap_put(int swslot, struct vm_page **ppsp, int npages, int flags)
{
	int error;

	error = uvm_swap_io(ppsp, swslot, npages, B_WRITE |
	    ((flags & PGO_SYNCIO) ? 0 : B_ASYNC));
	return error;
}

/*
 * uvm_swap_get: get a single page from swap
 *
 * => usually a sync op (from fault)
 */

int
uvm_swap_get(struct vm_page *page, int swslot, int flags)
{
	int error;

	uvmexp.nswget++;
	KASSERT(flags & PGO_SYNCIO);
	if (swslot == SWSLOT_BAD) {
		return EIO;
	}

	error = uvm_swap_io(&page, swslot, 1, B_READ |
	    ((flags & PGO_SYNCIO) ? 0 : B_ASYNC));
	if (error == 0) {

		/*
		 * this page is no longer only in swap.
		 */

		mutex_enter(&uvm_swap_data_lock);
		KASSERT(uvmexp.swpgonly > 0);
		uvmexp.swpgonly--;
		mutex_exit(&uvm_swap_data_lock);
	}
	return error;
}

/*
 * uvm_swap_io: do an i/o operation to swap
 */

static int
uvm_swap_io(struct vm_page **pps, int startslot, int npages, int flags)
{
	daddr_t startblk;
	struct	buf *bp;
	vaddr_t kva;
	int	error, mapinflags;
	bool write, async;
	UVMHIST_FUNC("uvm_swap_io"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "<- called, startslot=%d, npages=%d, flags=%d",
	    startslot, npages, flags, 0);

	write = (flags & B_READ) == 0;
	async = (flags & B_ASYNC) != 0;

	/*
	 * allocate a buf for the i/o.
	 */

	KASSERT(curlwp != uvm.pagedaemon_lwp || (write && async));
	bp = getiobuf(swapdev_vp, curlwp != uvm.pagedaemon_lwp);
	if (bp == NULL) {
		uvm_aio_aiodone_pages(pps, npages, true, ENOMEM);
		return ENOMEM;
	}

	/*
	 * convert starting drum slot to block number
	 */

	startblk = btodb((uint64_t)startslot << PAGE_SHIFT);

	/*
	 * first, map the pages into the kernel.
	 */

	mapinflags = !write ?
		UVMPAGER_MAPIN_WAITOK|UVMPAGER_MAPIN_READ :
		UVMPAGER_MAPIN_WAITOK|UVMPAGER_MAPIN_WRITE;
	kva = uvm_pagermapin(pps, npages, mapinflags);

	/*
	 * fill in the bp/sbp.   we currently route our i/o through
	 * /dev/drum's vnode [swapdev_vp].
	 */

	bp->b_cflags = BC_BUSY | BC_NOCACHE;
	bp->b_flags = (flags & (B_READ|B_ASYNC));
	bp->b_proc = &proc0;	/* XXX */
	bp->b_vnbufs.le_next = NOLIST;
	bp->b_data = (void *)kva;
	bp->b_blkno = startblk;
	bp->b_bufsize = bp->b_bcount = npages << PAGE_SHIFT;

	/*
	 * bump v_numoutput (counter of number of active outputs).
	 */

	if (write) {
		mutex_enter(swapdev_vp->v_interlock);
		swapdev_vp->v_numoutput++;
		mutex_exit(swapdev_vp->v_interlock);
	}

	/*
	 * for async ops we must set up the iodone handler.
	 */

	if (async) {
		bp->b_iodone = uvm_aio_biodone;
		UVMHIST_LOG(pdhist, "doing async!", 0, 0, 0, 0);
		if (curlwp == uvm.pagedaemon_lwp)
			BIO_SETPRIO(bp, BPRIO_TIMECRITICAL);
		else
			BIO_SETPRIO(bp, BPRIO_TIMELIMITED);
	} else {
		bp->b_iodone = NULL;
		BIO_SETPRIO(bp, BPRIO_TIMECRITICAL);
	}
	UVMHIST_LOG(pdhist,
	    "about to start io: data = %p blkno = 0x%x, bcount = %ld",
	    bp->b_data, bp->b_blkno, bp->b_bcount, 0);

	/*
	 * now we start the I/O, and if async, return.
	 */

	VOP_STRATEGY(swapdev_vp, bp);
	if (async)
		return 0;

	/*
	 * must be sync i/o.   wait for it to finish
	 */

	error = biowait(bp);

	/*
	 * kill the pager mapping
	 */

	uvm_pagermapout(kva, npages);

	/*
	 * now dispose of the buf and we're done.
	 */

	if (write) {
		mutex_enter(swapdev_vp->v_interlock);
		vwakeup(bp);
		mutex_exit(swapdev_vp->v_interlock);
	}
	putiobuf(bp);
	UVMHIST_LOG(pdhist, "<- done (sync)  error=%d", error, 0, 0, 0);

	return (error);
}
