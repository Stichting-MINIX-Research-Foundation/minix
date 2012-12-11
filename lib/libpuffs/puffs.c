/*	$NetBSD: puffs.c,v 1.92.4.4 2009/10/27 20:37:38 bouyer Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program and the Ulla Tuominen Foundation.
 * The Google SoC project was mentored by Bill Studenmund.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* TODO: We don't support PUFFS_COMFD used in original puffs_mount,
 * add it to the docs if any.
 *
 */

#include "fs.h"
#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: puffs.c,v 1.92.4.4 2009/10/27 20:37:38 bouyer Exp $");
#endif /* !lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <minix/endpoint.h>
#include <minix/vfsif.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "puffs.h"
#include "puffs_priv.h"

#ifdef PUFFS_WITH_THREADS
#include <pthread.h>
pthread_mutex_t pu_lock = PTHREAD_MUTEX_INITIALIZER;
#endif


/* Declare some local functions. */
static void get_work(message *m_in);
static void reply(endpoint_t who, message *m_out);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

EXTERN int env_argc;
EXTERN char **env_argv;


#define PUFFS_MAX_ARGS 20

int __real_main(int argc, char* argv[]);
int __wrap_main(int argc, char* argv[]);

int __wrap_main(int argc, char *argv[])
{
  int i;
  int new_argc = 0;
  static char* new_argv[PUFFS_MAX_ARGS];
  char *name;

  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();

  global_kcred.pkcr_type = PUFFCRED_TYPE_INTERNAL;

  if (argc < 3) {
	panic("Unexpected arguments, use:\
		mount -t fs /dev/ /dir [-o option1,option2]\n");
  }

  name = argv[0] + strlen(argv[0]);
  while (*name != '/' && name != argv[0])
	  name--;
  if (name != argv[0])
	  name++;
  strcpy(fs_name, name);

  new_argv[new_argc] = argv[0];
  new_argc++;
  
  for (i = 1; i < argc; i++) {
	if (new_argc >= PUFFS_MAX_ARGS) {
		panic("Too many arguments, change PUFFS_MAX_ARGS");
	}
	new_argv[new_argc] = argv[i];
	new_argc++;
  }

  assert(new_argc > 0);

  get_work(&fs_m_in);

  return __real_main(new_argc, new_argv);
}


#define FILLOP(lower, upper)						\
do {									\
	if (pops->puffs_node_##lower)					\
		opmask[PUFFS_VN_##upper] = 1;				\
} while (/*CONSTCOND*/0)
static void
fillvnopmask(struct puffs_ops *pops, uint8_t *opmask)
{

	memset(opmask, 0, PUFFS_VN_MAX);

	FILLOP(create,   CREATE);
	FILLOP(mknod,    MKNOD);
	FILLOP(open,     OPEN);
	FILLOP(close,    CLOSE);
	FILLOP(access,   ACCESS);
	FILLOP(getattr,  GETATTR);
	FILLOP(setattr,  SETATTR);
	FILLOP(poll,     POLL); /* XXX: not ready in kernel */
	FILLOP(mmap,     MMAP);
	FILLOP(fsync,    FSYNC);
	FILLOP(seek,     SEEK);
	FILLOP(remove,   REMOVE);
	FILLOP(link,     LINK);
	FILLOP(rename,   RENAME);
	FILLOP(mkdir,    MKDIR);
	FILLOP(rmdir,    RMDIR);
	FILLOP(symlink,  SYMLINK);
	FILLOP(readdir,  READDIR);
	FILLOP(readlink, READLINK);
	FILLOP(reclaim,  RECLAIM);
	FILLOP(inactive, INACTIVE);
	FILLOP(print,    PRINT);
	FILLOP(read,     READ);
	FILLOP(write,    WRITE);
	FILLOP(abortop,  ABORTOP);
}
#undef FILLOP


/*ARGSUSED*/
static void
puffs_defaulterror(struct puffs_usermount *pu, uint8_t type,
	int error, const char *str, puffs_cookie_t cookie)
{

	lpuffs_debug("abort: type %d, error %d, cookie %p (%s)\n",
	    type, error, cookie, str);
	abort();
}


int
puffs_getstate(struct puffs_usermount *pu)
{

	return pu->pu_state & PU_STATEMASK;
}

void
puffs_setstacksize(struct puffs_usermount *pu, size_t ss)
{
	long psize, minsize;
	int stackshift;
	int bonus;

	assert(puffs_getstate(pu) == PUFFS_STATE_BEFOREMOUNT);

	psize = sysconf(_SC_PAGESIZE);
	minsize = 4*psize;
	if (ss < minsize || ss == PUFFS_STACKSIZE_MIN) {
		if (ss != PUFFS_STACKSIZE_MIN)
			lpuffs_debug("puffs_setstacksize: adjusting "
			    "stacksize to minimum %ld\n", minsize);
		ss = 4*psize;
	}
 
	stackshift = -1;
	bonus = 0;
	while (ss) {
		if (ss & 0x1)
			bonus++;
		ss >>= 1;
		stackshift++;
	}
	if (bonus > 1) {
		stackshift++;
		lpuffs_debug("puffs_setstacksize: using next power of two: "
		    "%d\n", 1<<stackshift);
	}

	pu->pu_cc_stackshift = stackshift;
}

struct puffs_pathobj *
puffs_getrootpathobj(struct puffs_usermount *pu)
{
	struct puffs_node *pnr;

	pnr = pu->pu_pn_root;
	if (pnr == NULL) {
		errno = ENOENT;
		return NULL;
	}

	return &pnr->pn_po;
}

void
puffs_setroot(struct puffs_usermount *pu, struct puffs_node *pn)
{

	pu->pu_pn_root = pn;
}

struct puffs_node *
puffs_getroot(struct puffs_usermount *pu)
{

	return pu->pu_pn_root;
}

void
puffs_setrootinfo(struct puffs_usermount *pu, enum vtype vt,
	vsize_t vsize, dev_t rdev)
{
	struct puffs_kargs *pargs = pu->pu_kargp;

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT) {
		warnx("puffs_setrootinfo: call has effect only "
		    "before mount\n");
		return;
	}

	pargs->pa_root_vtype = vt;
	pargs->pa_root_vsize = vsize;
	pargs->pa_root_rdev = rdev;
}

void *
puffs_getspecific(struct puffs_usermount *pu)
{

	return pu->pu_privdata;
}

void
puffs_setspecific(struct puffs_usermount *pu, void *privdata)
{

	pu->pu_privdata = privdata;
}

void
puffs_setmntinfo(struct puffs_usermount *pu,
	const char *mntfromname, const char *puffsname)
{
	struct puffs_kargs *pargs = pu->pu_kargp;

	(void)strlcpy(pargs->pa_mntfromname, mntfromname,
	    sizeof(pargs->pa_mntfromname));
	(void)strlcpy(pargs->pa_typename, puffsname,
	    sizeof(pargs->pa_typename));
}

size_t
puffs_getmaxreqlen(struct puffs_usermount *pu)
{

	return pu->pu_maxreqlen;
}

void
puffs_setmaxreqlen(struct puffs_usermount *pu, size_t reqlen)
{

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT)
		warnx("puffs_setmaxreqlen: call has effect only "
		    "before mount\n");

	pu->pu_kargp->pa_maxmsglen = reqlen;
}

void
puffs_setfhsize(struct puffs_usermount *pu, size_t fhsize, int flags)
{

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT)
		warnx("puffs_setfhsize: call has effect only before mount\n");

	pu->pu_kargp->pa_fhsize = fhsize;
	pu->pu_kargp->pa_fhflags = flags;
}

void
puffs_setncookiehash(struct puffs_usermount *pu, int nhash)
{

	if (puffs_getstate(pu) != PUFFS_STATE_BEFOREMOUNT)
		warnx("puffs_setfhsize: call has effect only before mount\n");

	pu->pu_kargp->pa_nhashbuckets = nhash;
}

void
puffs_set_pathbuild(struct puffs_usermount *pu, pu_pathbuild_fn fn)
{

	pu->pu_pathbuild = fn;
}

void
puffs_set_pathtransform(struct puffs_usermount *pu, pu_pathtransform_fn fn)
{

	pu->pu_pathtransform = fn;
}

void
puffs_set_pathcmp(struct puffs_usermount *pu, pu_pathcmp_fn fn)
{

	pu->pu_pathcmp = fn;
}

void
puffs_set_pathfree(struct puffs_usermount *pu, pu_pathfree_fn fn)
{

	pu->pu_pathfree = fn;
}

void
puffs_set_namemod(struct puffs_usermount *pu, pu_namemod_fn fn)
{

	pu->pu_namemod = fn;
}

void
puffs_set_errnotify(struct puffs_usermount *pu, pu_errnotify_fn fn)
{

	pu->pu_errnotify = fn;
}

void
puffs_set_cmap(struct puffs_usermount *pu, pu_cmap_fn fn)
{

	pu->pu_cmap = fn;
}

void
puffs_ml_setloopfn(struct puffs_usermount *pu, puffs_ml_loop_fn lfn)
{

	pu->pu_ml_lfn = lfn;
}

void
puffs_ml_settimeout(struct puffs_usermount *pu, struct timespec *ts)
{

	if (ts == NULL) {
		pu->pu_ml_timep = NULL;
	} else {
		pu->pu_ml_timeout = *ts;
		pu->pu_ml_timep = &pu->pu_ml_timeout;
	}
}

void
puffs_set_prepost(struct puffs_usermount *pu,
	pu_prepost_fn pre, pu_prepost_fn pst)
{

	pu->pu_oppre = pre;
	pu->pu_oppost = pst;
}

int
puffs_mount(struct puffs_usermount *pu, const char *dir, int mntflags,
	puffs_cookie_t cookie)
{
        endpoint_t src;
        int error, ind;

        pu->pu_kargp->pa_root_cookie = cookie;
	
        src = fs_m_in.m_source;
        error = OK;
        caller_uid = INVAL_UID; /* To trap errors */
        caller_gid = INVAL_GID;
        req_nr = fs_m_in.m_type;

        if (req_nr < VFS_BASE) {
                fs_m_in.m_type += VFS_BASE;
                req_nr = fs_m_in.m_type;
        }
        ind = req_nr - VFS_BASE;

        assert(ind == REQ_READ_SUPER);

        if (ind < 0 || ind >= NREQS) {
                error = EINVAL;
        } else {
                error = (*fs_call_vec[ind])();
        }

        fs_m_out.m_type = error;
	if (IS_VFS_FS_TRANSID(last_request_transid)) {
		/* If a transaction ID was set, reset it */
		fs_m_out.m_type = TRNS_ADD_ID(fs_m_out.m_type,
					      last_request_transid);
	}
        reply(src, &fs_m_out);

        if (error) {
                free(pu->pu_kargp);
                pu->pu_kargp = NULL;
                errno = error;
                return -1;
        }

        PU_SETSTATE(pu, PUFFS_STATE_RUNNING);
        return 0;
}

/*ARGSUSED*/
struct puffs_usermount *
_puffs_init(int dummy, struct puffs_ops *pops, const char *mntfromname,
	const char *puffsname, void *priv, uint32_t pflags)
{
	struct puffs_usermount *pu;
	struct puffs_kargs *pargs;
	int sverrno;

	if (puffsname == PUFFS_DEFER)
		puffsname = "n/a";
	if (mntfromname == PUFFS_DEFER)
		mntfromname = "n/a";
	if (priv == PUFFS_DEFER)
		priv = NULL;

	pu = malloc(sizeof(struct puffs_usermount));
	if (pu == NULL)
		goto failfree;
	memset(pu, 0, sizeof(struct puffs_usermount));

	pargs = pu->pu_kargp = malloc(sizeof(struct puffs_kargs));
	if (pargs == NULL)
		goto failfree;
	memset(pargs, 0, sizeof(struct puffs_kargs));

	pargs->pa_vers = PUFFSDEVELVERS | PUFFSVERSION;
	pargs->pa_flags = PUFFS_FLAG_KERN(pflags);
	fillvnopmask(pops, pargs->pa_vnopmask);
	puffs_setmntinfo(pu, mntfromname, puffsname);

	puffs_zerostatvfs(&pargs->pa_svfsb);
	pargs->pa_root_cookie = NULL;
	pargs->pa_root_vtype = VDIR;
	pargs->pa_root_vsize = 0;
	pargs->pa_root_rdev = 0;
	pargs->pa_maxmsglen = 0;

	pu->pu_flags = pflags;
	buildpath = pu->pu_flags & PUFFS_FLAG_BUILDPATH; /* XXX */
	pu->pu_ops = *pops;
	free(pops); /* XXX */

	pu->pu_privdata = priv;
	pu->pu_cc_stackshift = PUFFS_CC_STACKSHIFT_DEFAULT;
	LIST_INIT(&pu->pu_pnodelst);
	LIST_INIT(&pu->pu_pnode_removed_lst);
	LIST_INIT(&pu->pu_ios);
	LIST_INIT(&pu->pu_ios_rmlist);
	LIST_INIT(&pu->pu_ccmagazin);
	TAILQ_INIT(&pu->pu_sched);

	/* defaults for some user-settable translation functions */
	pu->pu_cmap = NULL; /* identity translation */

        pu->pu_pathbuild = puffs_stdpath_buildpath;
        pu->pu_pathfree = puffs_stdpath_freepath;
        pu->pu_pathcmp = puffs_stdpath_cmppath;
        pu->pu_pathtransform = NULL;
        pu->pu_namemod = NULL;

        pu->pu_errnotify = puffs_defaulterror;

	PU_SETSTATE(pu, PUFFS_STATE_BEFOREMOUNT);

	global_pu = pu;

	return pu;

 failfree:
	/* can't unmount() from here for obvious reasons */
	sverrno = errno;
	free(pu);
	errno = sverrno;
	return NULL;
}

void
puffs_cancel(struct puffs_usermount *pu, int error)
{
	assert(puffs_getstate(pu) < PUFFS_STATE_RUNNING);
	free(pu);
}

/*ARGSUSED1*/
int
puffs_exit(struct puffs_usermount *pu, int force)
{
	struct puffs_node *pn;
        
        lpuffs_debug("puffs_exit\n");

	while ((pn = LIST_FIRST(&pu->pu_pnodelst)) != NULL)
		puffs_pn_put(pn);

	while ((pn = LIST_FIRST(&pu->pu_pnode_removed_lst)) != NULL)
		puffs_pn_put(pn);

	puffs__cc_exit(pu);
	if (pu->pu_state & PU_HASKQ)
		close(pu->pu_kq);
	free(pu);

	return 0; /* always succesful for now, WILL CHANGE */
}

/*
 * Actual mainloop.  This is called from a context which can block.
 * It is called either from puffs_mainloop (indirectly, via
 * puffs_cc_continue() or from puffs_cc_yield()).
 */
void
puffs__theloop(struct puffs_cc *pcc)
{
	struct puffs_usermount *pu = pcc->pcc_pu;
	int error, ind;

	while (!unmountdone || !exitsignaled) {
		endpoint_t src;

		/*
		 * Schedule existing requests.
		 */
		while ((pcc = TAILQ_FIRST(&pu->pu_sched)) != NULL) {
			lpuffs_debug("scheduling existing tasks\n");
			TAILQ_REMOVE(&pu->pu_sched, pcc, pcc_schedent);
			puffs__goto(pcc);
		}

		if (pu->pu_ml_lfn) {
                        lpuffs_debug("Calling user mainloop handler\n");
			pu->pu_ml_lfn(pu);
		}

		/* Wait for request message. */
		get_work(&fs_m_in);

		src = fs_m_in.m_source;
		error = OK;
		caller_uid = INVAL_UID; /* To trap errors */
		caller_gid = INVAL_GID;
		req_nr = fs_m_in.m_type;

		if (req_nr < VFS_BASE) {
			fs_m_in.m_type += VFS_BASE;
			req_nr = fs_m_in.m_type;
		}
		ind = req_nr - VFS_BASE;

		if (ind < 0 || ind >= NREQS) {
			error = EINVAL;
		} else {
			error = (*fs_call_vec[ind])();
		}

		fs_m_out.m_type = error;
		if (IS_VFS_FS_TRANSID(last_request_transid)) {
			/* If a transaction ID was set, reset it */
			fs_m_out.m_type = TRNS_ADD_ID(fs_m_out.m_type, last_request_transid);
		}
		reply(src, &fs_m_out);
	}

	if (puffs__cc_restoremain(pu) == -1)
		warn("cannot restore main context.  impending doom");

	/* May get here, if puffs_fakecc is set to 1. Currently librefuse sets it.
	 * Now we just return to the caller.
	 */
}

int
puffs_mainloop(struct puffs_usermount *pu)
{
	struct puffs_cc *pcc;
	int sverrno;

	assert(puffs_getstate(pu) >= PUFFS_STATE_RUNNING);

	pu->pu_state |= PU_HASKQ | PU_INLOOP;

	/*
	 * Create alternate execution context and jump to it.  Note
	 * that we come "out" of savemain twice.  Where we come out
	 * of it depends on the architecture.  If the return address is
	 * stored on the stack, we jump out from puffs_cc_continue(),
	 * for a register return address from puffs__cc_savemain().
	 * PU_MAINRESTORE makes sure we DTRT in both cases.
	 */
	if (puffs__cc_create(pu, puffs__theloop, &pcc) == -1) {
		goto out;
	}
	if (puffs__cc_savemain(pu) == -1) {
		goto out;
	}
	if ((pu->pu_state & PU_MAINRESTORE) == 0)
		puffs_cc_continue(pcc);

	errno = 0;

 out:
	/* store the real error for a while */
	sverrno = errno;

	errno = sverrno;
	if (errno)
		return -1;
	else
		return 0;
}


/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fail);

  /* No live update support for now. */

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the Minix file server. */
  SELF_E = getprocnr();
  return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  exitsignaled = 1;
  fs_sync();

  /* If unmounting has already been performed, exit immediately.
   * We might not get another message.
   */
  if (unmountdone) {
        if (puffs__cc_restoremain(global_pu) == -1)
                warn("cannot restore main context.  impending doom");
	/* May happen if puffs_fakecc is set to 1. Currently librefuse sets it.
	 * There is a chance, that main loop hangs in receive() and we will
	 * never get any new message, so we have to exit() here.
	 */
	exit(0);
  }
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
static void get_work(m_in)
message *m_in;				/* pointer to message */
{
  int r, srcok = 0;
  endpoint_t src;

  do {
	if ((r = sef_receive(ANY, m_in)) != OK) 	/* wait for message */
		panic("sef_receive failed: %d", r);
	src = m_in->m_source;

	if(src == VFS_PROC_NR) {
		if(unmountdone)
			lpuffs_debug("libpuffs: unmounted: unexpected message from FS\n");
		else
			srcok = 1;		/* Normal FS request. */

	} else
		lpuffs_debug("libpuffs: unexpected source %d\n", src);
  } while(!srcok);

  assert((src == VFS_PROC_NR && !unmountdone));

  last_request_transid = TRNS_GET_ID(fs_m_in.m_type);
  fs_m_in.m_type = TRNS_DEL_ID(fs_m_in.m_type);
  if (fs_m_in.m_type == 0) {
	  assert(!IS_VFS_FS_TRANSID(last_request_transid));
	  fs_m_in.m_type = last_request_transid;  /* Backwards compat. */
	  last_request_transid = 0;
  } else
	  assert(IS_VFS_FS_TRANSID(last_request_transid));
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
static void reply(
  endpoint_t who,
  message *m_out                       	/* report result */
)
{
  if (OK != send(who, m_out))    /* send the message */
	lpuffs_debug("libpuffs(%d) was unable to send reply\n", SELF_E);

  last_request_transid = 0;
}

