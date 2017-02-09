/* $NetBSD: subr_autoconf.c,v 1.235 2015/04/13 16:46:33 riastradh Exp $ */

/*
 * Copyright (c) 1996, 2000 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * --(license Id: LICENSE.proto,v 1.1 2000/06/13 21:40:26 cgd Exp )--
 */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Header: subr_autoconf.c,v 1.12 93/02/01 19:31:48 torek Exp  (LBL)
 *
 *	@(#)subr_autoconf.c	8.3 (Berkeley) 5/17/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_autoconf.c,v 1.235 2015/04/13 16:46:33 riastradh Exp $");

#ifdef _KERNEL_OPT
#include "opt_ddb.h"
#include "drvctl.h"
#endif

#include <sys/param.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/kthread.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/callout.h>
#include <sys/devmon.h>
#include <sys/cpu.h>
#include <sys/sysctl.h>

#include <sys/disk.h>

#include <sys/rndsource.h>

#include <machine/limits.h>

/*
 * Autoconfiguration subroutines.
 */

/*
 * Device autoconfiguration timings are mixed into the entropy pool.
 */
extern krndsource_t rnd_autoconf_source;

/*
 * ioconf.c exports exactly two names: cfdata and cfroots.  All system
 * devices and drivers are found via these tables.
 */
extern struct cfdata cfdata[];
extern const short cfroots[];

/*
 * List of all cfdriver structures.  We use this to detect duplicates
 * when other cfdrivers are loaded.
 */
struct cfdriverlist allcfdrivers = LIST_HEAD_INITIALIZER(&allcfdrivers);
extern struct cfdriver * const cfdriver_list_initial[];

/*
 * Initial list of cfattach's.
 */
extern const struct cfattachinit cfattachinit[];

/*
 * List of cfdata tables.  We always have one such list -- the one
 * built statically when the kernel was configured.
 */
struct cftablelist allcftables = TAILQ_HEAD_INITIALIZER(allcftables);
static struct cftable initcftable;

#define	ROOT ((device_t)NULL)

struct matchinfo {
	cfsubmatch_t fn;
	device_t parent;
	const int *locs;
	void	*aux;
	struct	cfdata *match;
	int	pri;
};

struct alldevs_foray {
	int			af_s;
	struct devicelist	af_garbage;
};

static char *number(char *, int);
static void mapply(struct matchinfo *, cfdata_t);
static device_t config_devalloc(const device_t, const cfdata_t, const int *);
static void config_devdelete(device_t);
static void config_devunlink(device_t, struct devicelist *);
static void config_makeroom(int, struct cfdriver *);
static void config_devlink(device_t);
static void config_alldevs_unlock(int);
static int config_alldevs_lock(void);
static void config_alldevs_enter(struct alldevs_foray *);
static void config_alldevs_exit(struct alldevs_foray *);
static void config_add_attrib_dict(device_t);

static void config_collect_garbage(struct devicelist *);
static void config_dump_garbage(struct devicelist *);

static void pmflock_debug(device_t, const char *, int);

static device_t deviter_next1(deviter_t *);
static void deviter_reinit(deviter_t *);

struct deferred_config {
	TAILQ_ENTRY(deferred_config) dc_queue;
	device_t dc_dev;
	void (*dc_func)(device_t);
};

TAILQ_HEAD(deferred_config_head, deferred_config);

struct deferred_config_head deferred_config_queue =
	TAILQ_HEAD_INITIALIZER(deferred_config_queue);
struct deferred_config_head interrupt_config_queue =
	TAILQ_HEAD_INITIALIZER(interrupt_config_queue);
int interrupt_config_threads = 8;
struct deferred_config_head mountroot_config_queue =
	TAILQ_HEAD_INITIALIZER(mountroot_config_queue);
int mountroot_config_threads = 2;
static lwp_t **mountroot_config_lwpids;
static size_t mountroot_config_lwpids_size;
static bool root_is_mounted = false;

static void config_process_deferred(struct deferred_config_head *, device_t);

/* Hooks to finalize configuration once all real devices have been found. */
struct finalize_hook {
	TAILQ_ENTRY(finalize_hook) f_list;
	int (*f_func)(device_t);
	device_t f_dev;
};
static TAILQ_HEAD(, finalize_hook) config_finalize_list =
	TAILQ_HEAD_INITIALIZER(config_finalize_list);
static int config_finalize_done;

/* list of all devices */
static struct devicelist alldevs = TAILQ_HEAD_INITIALIZER(alldevs);
static kmutex_t alldevs_mtx;
static volatile bool alldevs_garbage = false;
static volatile devgen_t alldevs_gen = 1;
static volatile int alldevs_nread = 0;
static volatile int alldevs_nwrite = 0;

static int config_pending;		/* semaphore for mountroot */
static kmutex_t config_misc_lock;
static kcondvar_t config_misc_cv;

static bool detachall = false;

#define	STREQ(s1, s2)			\
	(*(s1) == *(s2) && strcmp((s1), (s2)) == 0)

static bool config_initialized = false;	/* config_init() has been called. */

static int config_do_twiddle;
static callout_t config_twiddle_ch;

static void sysctl_detach_setup(struct sysctllog **);

typedef int (*cfdriver_fn)(struct cfdriver *);
static int
frob_cfdrivervec(struct cfdriver * const *cfdriverv,
	cfdriver_fn drv_do, cfdriver_fn drv_undo,
	const char *style, bool dopanic)
{
	void (*pr)(const char *, ...) __printflike(1, 2) =
	    dopanic ? panic : printf;
	int i, error = 0, e2 __diagused;

	for (i = 0; cfdriverv[i] != NULL; i++) {
		if ((error = drv_do(cfdriverv[i])) != 0) {
			pr("configure: `%s' driver %s failed: %d",
			    cfdriverv[i]->cd_name, style, error);
			goto bad;
		}
	}

	KASSERT(error == 0);
	return 0;

 bad:
	printf("\n");
	for (i--; i >= 0; i--) {
		e2 = drv_undo(cfdriverv[i]);
		KASSERT(e2 == 0);
	}

	return error;
}

typedef int (*cfattach_fn)(const char *, struct cfattach *);
static int
frob_cfattachvec(const struct cfattachinit *cfattachv,
	cfattach_fn att_do, cfattach_fn att_undo,
	const char *style, bool dopanic)
{
	const struct cfattachinit *cfai = NULL;
	void (*pr)(const char *, ...) __printflike(1, 2) =
	    dopanic ? panic : printf;
	int j = 0, error = 0, e2 __diagused;

	for (cfai = &cfattachv[0]; cfai->cfai_name != NULL; cfai++) {
		for (j = 0; cfai->cfai_list[j] != NULL; j++) {
			if ((error = att_do(cfai->cfai_name,
			    cfai->cfai_list[j])) != 0) {
				pr("configure: attachment `%s' "
				    "of `%s' driver %s failed: %d",
				    cfai->cfai_list[j]->ca_name,
				    cfai->cfai_name, style, error);
				goto bad;
			}
		}
	}

	KASSERT(error == 0);
	return 0;

 bad:
	/*
	 * Rollback in reverse order.  dunno if super-important, but
	 * do that anyway.  Although the code looks a little like
	 * someone did a little integration (in the math sense).
	 */
	printf("\n");
	if (cfai) {
		bool last;

		for (last = false; last == false; ) {
			if (cfai == &cfattachv[0])
				last = true;
			for (j--; j >= 0; j--) {
				e2 = att_undo(cfai->cfai_name,
				    cfai->cfai_list[j]);
				KASSERT(e2 == 0);
			}
			if (!last) {
				cfai--;
				for (j = 0; cfai->cfai_list[j] != NULL; j++)
					;
			}
		}
	}

	return error;
}

/*
 * Initialize the autoconfiguration data structures.  Normally this
 * is done by configure(), but some platforms need to do this very
 * early (to e.g. initialize the console).
 */
void
config_init(void)
{

	KASSERT(config_initialized == false);

	mutex_init(&alldevs_mtx, MUTEX_DEFAULT, IPL_VM);

	mutex_init(&config_misc_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&config_misc_cv, "cfgmisc");

	callout_init(&config_twiddle_ch, CALLOUT_MPSAFE);

	frob_cfdrivervec(cfdriver_list_initial,
	    config_cfdriver_attach, NULL, "bootstrap", true);
	frob_cfattachvec(cfattachinit,
	    config_cfattach_attach, NULL, "bootstrap", true);

	initcftable.ct_cfdata = cfdata;
	TAILQ_INSERT_TAIL(&allcftables, &initcftable, ct_list);

	config_initialized = true;
}

/*
 * Init or fini drivers and attachments.  Either all or none
 * are processed (via rollback).  It would be nice if this were
 * atomic to outside consumers, but with the current state of
 * locking ...
 */
int
config_init_component(struct cfdriver * const *cfdriverv,
	const struct cfattachinit *cfattachv, struct cfdata *cfdatav)
{
	int error;

	if ((error = frob_cfdrivervec(cfdriverv,
	    config_cfdriver_attach, config_cfdriver_detach, "init", false))!= 0)
		return error;
	if ((error = frob_cfattachvec(cfattachv,
	    config_cfattach_attach, config_cfattach_detach,
	    "init", false)) != 0) {
		frob_cfdrivervec(cfdriverv,
	            config_cfdriver_detach, NULL, "init rollback", true);
		return error;
	}
	if ((error = config_cfdata_attach(cfdatav, 1)) != 0) {
		frob_cfattachvec(cfattachv,
		    config_cfattach_detach, NULL, "init rollback", true);
		frob_cfdrivervec(cfdriverv,
	            config_cfdriver_detach, NULL, "init rollback", true);
		return error;
	}

	return 0;
}

int
config_fini_component(struct cfdriver * const *cfdriverv,
	const struct cfattachinit *cfattachv, struct cfdata *cfdatav)
{
	int error;

	if ((error = config_cfdata_detach(cfdatav)) != 0)
		return error;
	if ((error = frob_cfattachvec(cfattachv,
	    config_cfattach_detach, config_cfattach_attach,
	    "fini", false)) != 0) {
		if (config_cfdata_attach(cfdatav, 0) != 0)
			panic("config_cfdata fini rollback failed");
		return error;
	}
	if ((error = frob_cfdrivervec(cfdriverv,
	    config_cfdriver_detach, config_cfdriver_attach,
	    "fini", false)) != 0) {
		frob_cfattachvec(cfattachv,
	            config_cfattach_attach, NULL, "fini rollback", true);
		if (config_cfdata_attach(cfdatav, 0) != 0)
			panic("config_cfdata fini rollback failed");
		return error;
	}

	return 0;
}

void
config_init_mi(void)
{

	if (!config_initialized)
		config_init();

	sysctl_detach_setup(NULL);
}

void
config_deferred(device_t dev)
{
	config_process_deferred(&deferred_config_queue, dev);
	config_process_deferred(&interrupt_config_queue, dev);
	config_process_deferred(&mountroot_config_queue, dev);
}

static void
config_interrupts_thread(void *cookie)
{
	struct deferred_config *dc;

	while ((dc = TAILQ_FIRST(&interrupt_config_queue)) != NULL) {
		TAILQ_REMOVE(&interrupt_config_queue, dc, dc_queue);
		(*dc->dc_func)(dc->dc_dev);
		config_pending_decr(dc->dc_dev);
		kmem_free(dc, sizeof(*dc));
	}
	kthread_exit(0);
}

void
config_create_interruptthreads(void)
{
	int i;

	for (i = 0; i < interrupt_config_threads; i++) {
		(void)kthread_create(PRI_NONE, 0, NULL,
		    config_interrupts_thread, NULL, NULL, "configintr");
	}
}

static void
config_mountroot_thread(void *cookie)
{
	struct deferred_config *dc;

	while ((dc = TAILQ_FIRST(&mountroot_config_queue)) != NULL) {
		TAILQ_REMOVE(&mountroot_config_queue, dc, dc_queue);
		(*dc->dc_func)(dc->dc_dev);
		kmem_free(dc, sizeof(*dc));
	}
	kthread_exit(0);
}

void
config_create_mountrootthreads(void)
{
	int i;

	if (!root_is_mounted)
		root_is_mounted = true;

	mountroot_config_lwpids_size = sizeof(mountroot_config_lwpids) *
				       mountroot_config_threads;
	mountroot_config_lwpids = kmem_alloc(mountroot_config_lwpids_size,
					     KM_NOSLEEP);
	KASSERT(mountroot_config_lwpids);
	for (i = 0; i < mountroot_config_threads; i++) {
		mountroot_config_lwpids[i] = 0;
		(void)kthread_create(PRI_NONE, KTHREAD_MUSTJOIN, NULL,
				     config_mountroot_thread, NULL,
				     &mountroot_config_lwpids[i],
				     "configroot");
	}
}

void
config_finalize_mountroot(void)
{
	int i, error;

	for (i = 0; i < mountroot_config_threads; i++) {
		if (mountroot_config_lwpids[i] == 0)
			continue;

		error = kthread_join(mountroot_config_lwpids[i]);
		if (error)
			printf("%s: thread %x joined with error %d\n",
			       __func__, i, error);
	}
	kmem_free(mountroot_config_lwpids, mountroot_config_lwpids_size);
}

/*
 * Announce device attach/detach to userland listeners.
 */
static void
devmon_report_device(device_t dev, bool isattach)
{
#if NDRVCTL > 0
	prop_dictionary_t ev;
	const char *parent;
	const char *what;
	device_t pdev = device_parent(dev);

	ev = prop_dictionary_create();
	if (ev == NULL)
		return;

	what = (isattach ? "device-attach" : "device-detach");
	parent = (pdev == NULL ? "root" : device_xname(pdev));
	if (!prop_dictionary_set_cstring(ev, "device", device_xname(dev)) ||
	    !prop_dictionary_set_cstring(ev, "parent", parent)) {
		prop_object_release(ev);
		return;
	}

	devmon_insert(what, ev);
#endif
}

/*
 * Add a cfdriver to the system.
 */
int
config_cfdriver_attach(struct cfdriver *cd)
{
	struct cfdriver *lcd;

	/* Make sure this driver isn't already in the system. */
	LIST_FOREACH(lcd, &allcfdrivers, cd_list) {
		if (STREQ(lcd->cd_name, cd->cd_name))
			return EEXIST;
	}

	LIST_INIT(&cd->cd_attach);
	LIST_INSERT_HEAD(&allcfdrivers, cd, cd_list);

	return 0;
}

/*
 * Remove a cfdriver from the system.
 */
int
config_cfdriver_detach(struct cfdriver *cd)
{
	struct alldevs_foray af;
	int i, rc = 0;

	config_alldevs_enter(&af);
	/* Make sure there are no active instances. */
	for (i = 0; i < cd->cd_ndevs; i++) {
		if (cd->cd_devs[i] != NULL) {
			rc = EBUSY;
			break;
		}
	}
	config_alldevs_exit(&af);

	if (rc != 0)
		return rc;

	/* ...and no attachments loaded. */
	if (LIST_EMPTY(&cd->cd_attach) == 0)
		return EBUSY;

	LIST_REMOVE(cd, cd_list);

	KASSERT(cd->cd_devs == NULL);

	return 0;
}

/*
 * Look up a cfdriver by name.
 */
struct cfdriver *
config_cfdriver_lookup(const char *name)
{
	struct cfdriver *cd;

	LIST_FOREACH(cd, &allcfdrivers, cd_list) {
		if (STREQ(cd->cd_name, name))
			return cd;
	}

	return NULL;
}

/*
 * Add a cfattach to the specified driver.
 */
int
config_cfattach_attach(const char *driver, struct cfattach *ca)
{
	struct cfattach *lca;
	struct cfdriver *cd;

	cd = config_cfdriver_lookup(driver);
	if (cd == NULL)
		return ESRCH;

	/* Make sure this attachment isn't already on this driver. */
	LIST_FOREACH(lca, &cd->cd_attach, ca_list) {
		if (STREQ(lca->ca_name, ca->ca_name))
			return EEXIST;
	}

	LIST_INSERT_HEAD(&cd->cd_attach, ca, ca_list);

	return 0;
}

/*
 * Remove a cfattach from the specified driver.
 */
int
config_cfattach_detach(const char *driver, struct cfattach *ca)
{
	struct alldevs_foray af;
	struct cfdriver *cd;
	device_t dev;
	int i, rc = 0;

	cd = config_cfdriver_lookup(driver);
	if (cd == NULL)
		return ESRCH;

	config_alldevs_enter(&af);
	/* Make sure there are no active instances. */
	for (i = 0; i < cd->cd_ndevs; i++) {
		if ((dev = cd->cd_devs[i]) == NULL)
			continue;
		if (dev->dv_cfattach == ca) {
			rc = EBUSY;
			break;
		}
	}
	config_alldevs_exit(&af);

	if (rc != 0)
		return rc;

	LIST_REMOVE(ca, ca_list);

	return 0;
}

/*
 * Look up a cfattach by name.
 */
static struct cfattach *
config_cfattach_lookup_cd(struct cfdriver *cd, const char *atname)
{
	struct cfattach *ca;

	LIST_FOREACH(ca, &cd->cd_attach, ca_list) {
		if (STREQ(ca->ca_name, atname))
			return ca;
	}

	return NULL;
}

/*
 * Look up a cfattach by driver/attachment name.
 */
struct cfattach *
config_cfattach_lookup(const char *name, const char *atname)
{
	struct cfdriver *cd;

	cd = config_cfdriver_lookup(name);
	if (cd == NULL)
		return NULL;

	return config_cfattach_lookup_cd(cd, atname);
}

/*
 * Apply the matching function and choose the best.  This is used
 * a few times and we want to keep the code small.
 */
static void
mapply(struct matchinfo *m, cfdata_t cf)
{
	int pri;

	if (m->fn != NULL) {
		pri = (*m->fn)(m->parent, cf, m->locs, m->aux);
	} else {
		pri = config_match(m->parent, cf, m->aux);
	}
	if (pri > m->pri) {
		m->match = cf;
		m->pri = pri;
	}
}

int
config_stdsubmatch(device_t parent, cfdata_t cf, const int *locs, void *aux)
{
	const struct cfiattrdata *ci;
	const struct cflocdesc *cl;
	int nlocs, i;

	ci = cfiattr_lookup(cfdata_ifattr(cf), parent->dv_cfdriver);
	KASSERT(ci);
	nlocs = ci->ci_loclen;
	KASSERT(!nlocs || locs);
	for (i = 0; i < nlocs; i++) {
		cl = &ci->ci_locdesc[i];
		if (cl->cld_defaultstr != NULL &&
		    cf->cf_loc[i] == cl->cld_default)
			continue;
		if (cf->cf_loc[i] == locs[i])
			continue;
		return 0;
	}

	return config_match(parent, cf, aux);
}

/*
 * Helper function: check whether the driver supports the interface attribute
 * and return its descriptor structure.
 */
static const struct cfiattrdata *
cfdriver_get_iattr(const struct cfdriver *cd, const char *ia)
{
	const struct cfiattrdata * const *cpp;

	if (cd->cd_attrs == NULL)
		return 0;

	for (cpp = cd->cd_attrs; *cpp; cpp++) {
		if (STREQ((*cpp)->ci_name, ia)) {
			/* Match. */
			return *cpp;
		}
	}
	return 0;
}

/*
 * Lookup an interface attribute description by name.
 * If the driver is given, consider only its supported attributes.
 */
const struct cfiattrdata *
cfiattr_lookup(const char *name, const struct cfdriver *cd)
{
	const struct cfdriver *d;
	const struct cfiattrdata *ia;

	if (cd)
		return cfdriver_get_iattr(cd, name);

	LIST_FOREACH(d, &allcfdrivers, cd_list) {
		ia = cfdriver_get_iattr(d, name);
		if (ia)
			return ia;
	}
	return 0;
}

/*
 * Determine if `parent' is a potential parent for a device spec based
 * on `cfp'.
 */
static int
cfparent_match(const device_t parent, const struct cfparent *cfp)
{
	struct cfdriver *pcd;

	/* We don't match root nodes here. */
	if (cfp == NULL)
		return 0;

	pcd = parent->dv_cfdriver;
	KASSERT(pcd != NULL);

	/*
	 * First, ensure this parent has the correct interface
	 * attribute.
	 */
	if (!cfdriver_get_iattr(pcd, cfp->cfp_iattr))
		return 0;

	/*
	 * If no specific parent device instance was specified (i.e.
	 * we're attaching to the attribute only), we're done!
	 */
	if (cfp->cfp_parent == NULL)
		return 1;

	/*
	 * Check the parent device's name.
	 */
	if (STREQ(pcd->cd_name, cfp->cfp_parent) == 0)
		return 0;	/* not the same parent */

	/*
	 * Make sure the unit number matches.
	 */
	if (cfp->cfp_unit == DVUNIT_ANY ||	/* wildcard */
	    cfp->cfp_unit == parent->dv_unit)
		return 1;

	/* Unit numbers don't match. */
	return 0;
}

/*
 * Helper for config_cfdata_attach(): check all devices whether it could be
 * parent any attachment in the config data table passed, and rescan.
 */
static void
rescan_with_cfdata(const struct cfdata *cf)
{
	device_t d;
	const struct cfdata *cf1;
	deviter_t di;
  

	/*
	 * "alldevs" is likely longer than a modules's cfdata, so make it
	 * the outer loop.
	 */
	for (d = deviter_first(&di, 0); d != NULL; d = deviter_next(&di)) {

		if (!(d->dv_cfattach->ca_rescan))
			continue;

		for (cf1 = cf; cf1->cf_name; cf1++) {

			if (!cfparent_match(d, cf1->cf_pspec))
				continue;

			(*d->dv_cfattach->ca_rescan)(d,
				cfdata_ifattr(cf1), cf1->cf_loc);

			config_deferred(d);
		}
	}
	deviter_release(&di);
}

/*
 * Attach a supplemental config data table and rescan potential
 * parent devices if required.
 */
int
config_cfdata_attach(cfdata_t cf, int scannow)
{
	struct cftable *ct;

	ct = kmem_alloc(sizeof(*ct), KM_SLEEP);
	ct->ct_cfdata = cf;
	TAILQ_INSERT_TAIL(&allcftables, ct, ct_list);

	if (scannow)
		rescan_with_cfdata(cf);

	return 0;
}

/*
 * Helper for config_cfdata_detach: check whether a device is
 * found through any attachment in the config data table.
 */
static int
dev_in_cfdata(device_t d, cfdata_t cf)
{
	const struct cfdata *cf1;

	for (cf1 = cf; cf1->cf_name; cf1++)
		if (d->dv_cfdata == cf1)
			return 1;

	return 0;
}

/*
 * Detach a supplemental config data table. Detach all devices found
 * through that table (and thus keeping references to it) before.
 */
int
config_cfdata_detach(cfdata_t cf)
{
	device_t d;
	int error = 0;
	struct cftable *ct;
	deviter_t di;

	for (d = deviter_first(&di, DEVITER_F_RW); d != NULL;
	     d = deviter_next(&di)) {
		if (!dev_in_cfdata(d, cf))
			continue;
		if ((error = config_detach(d, 0)) != 0)
			break;
	}
	deviter_release(&di);
	if (error) {
		aprint_error_dev(d, "unable to detach instance\n");
		return error;
	}

	TAILQ_FOREACH(ct, &allcftables, ct_list) {
		if (ct->ct_cfdata == cf) {
			TAILQ_REMOVE(&allcftables, ct, ct_list);
			kmem_free(ct, sizeof(*ct));
			return 0;
		}
	}

	/* not found -- shouldn't happen */
	return EINVAL;
}

/*
 * Invoke the "match" routine for a cfdata entry on behalf of
 * an external caller, usually a "submatch" routine.
 */
int
config_match(device_t parent, cfdata_t cf, void *aux)
{
	struct cfattach *ca;

	ca = config_cfattach_lookup(cf->cf_name, cf->cf_atname);
	if (ca == NULL) {
		/* No attachment for this entry, oh well. */
		return 0;
	}

	return (*ca->ca_match)(parent, cf, aux);
}

/*
 * Iterate over all potential children of some device, calling the given
 * function (default being the child's match function) for each one.
 * Nonzero returns are matches; the highest value returned is considered
 * the best match.  Return the `found child' if we got a match, or NULL
 * otherwise.  The `aux' pointer is simply passed on through.
 *
 * Note that this function is designed so that it can be used to apply
 * an arbitrary function to all potential children (its return value
 * can be ignored).
 */
cfdata_t
config_search_loc(cfsubmatch_t fn, device_t parent,
		  const char *ifattr, const int *locs, void *aux)
{
	struct cftable *ct;
	cfdata_t cf;
	struct matchinfo m;

	KASSERT(config_initialized);
	KASSERT(!ifattr || cfdriver_get_iattr(parent->dv_cfdriver, ifattr));

	m.fn = fn;
	m.parent = parent;
	m.locs = locs;
	m.aux = aux;
	m.match = NULL;
	m.pri = 0;

	TAILQ_FOREACH(ct, &allcftables, ct_list) {
		for (cf = ct->ct_cfdata; cf->cf_name; cf++) {

			/* We don't match root nodes here. */
			if (!cf->cf_pspec)
				continue;

			/*
			 * Skip cf if no longer eligible, otherwise scan
			 * through parents for one matching `parent', and
			 * try match function.
			 */
			if (cf->cf_fstate == FSTATE_FOUND)
				continue;
			if (cf->cf_fstate == FSTATE_DNOTFOUND ||
			    cf->cf_fstate == FSTATE_DSTAR)
				continue;

			/*
			 * If an interface attribute was specified,
			 * consider only children which attach to
			 * that attribute.
			 */
			if (ifattr && !STREQ(ifattr, cfdata_ifattr(cf)))
				continue;

			if (cfparent_match(parent, cf->cf_pspec))
				mapply(&m, cf);
		}
	}
	return m.match;
}

cfdata_t
config_search_ia(cfsubmatch_t fn, device_t parent, const char *ifattr,
    void *aux)
{

	return config_search_loc(fn, parent, ifattr, NULL, aux);
}

/*
 * Find the given root device.
 * This is much like config_search, but there is no parent.
 * Don't bother with multiple cfdata tables; the root node
 * must always be in the initial table.
 */
cfdata_t
config_rootsearch(cfsubmatch_t fn, const char *rootname, void *aux)
{
	cfdata_t cf;
	const short *p;
	struct matchinfo m;

	m.fn = fn;
	m.parent = ROOT;
	m.aux = aux;
	m.match = NULL;
	m.pri = 0;
	m.locs = 0;
	/*
	 * Look at root entries for matching name.  We do not bother
	 * with found-state here since only one root should ever be
	 * searched (and it must be done first).
	 */
	for (p = cfroots; *p >= 0; p++) {
		cf = &cfdata[*p];
		if (strcmp(cf->cf_name, rootname) == 0)
			mapply(&m, cf);
	}
	return m.match;
}

static const char * const msgs[3] = { "", " not configured\n", " unsupported\n" };

/*
 * The given `aux' argument describes a device that has been found
 * on the given parent, but not necessarily configured.  Locate the
 * configuration data for that device (using the submatch function
 * provided, or using candidates' cd_match configuration driver
 * functions) and attach it, and return its device_t.  If the device was
 * not configured, call the given `print' function and return NULL.
 */
device_t
config_found_sm_loc(device_t parent,
		const char *ifattr, const int *locs, void *aux,
		cfprint_t print, cfsubmatch_t submatch)
{
	cfdata_t cf;

	if ((cf = config_search_loc(submatch, parent, ifattr, locs, aux)))
		return(config_attach_loc(parent, cf, locs, aux, print));
	if (print) {
		if (config_do_twiddle && cold)
			twiddle();
		aprint_normal("%s", msgs[(*print)(aux, device_xname(parent))]);
	}

	/*
	 * This has the effect of mixing in a single timestamp to the
	 * entropy pool.  Experiments indicate the estimator will almost
	 * always attribute one bit of entropy to this sample; analysis
	 * of device attach/detach timestamps on FreeBSD indicates 4
	 * bits of entropy/sample so this seems appropriately conservative.
	 */
	rnd_add_uint32(&rnd_autoconf_source, 0);
	return NULL;
}

device_t
config_found_ia(device_t parent, const char *ifattr, void *aux,
    cfprint_t print)
{

	return config_found_sm_loc(parent, ifattr, NULL, aux, print, NULL);
}

device_t
config_found(device_t parent, void *aux, cfprint_t print)
{

	return config_found_sm_loc(parent, NULL, NULL, aux, print, NULL);
}

/*
 * As above, but for root devices.
 */
device_t
config_rootfound(const char *rootname, void *aux)
{
	cfdata_t cf;

	if ((cf = config_rootsearch(NULL, rootname, aux)) != NULL)
		return config_attach(ROOT, cf, aux, NULL);
	aprint_error("root device %s not configured\n", rootname);
	return NULL;
}

/* just like sprintf(buf, "%d") except that it works from the end */
static char *
number(char *ep, int n)
{

	*--ep = 0;
	while (n >= 10) {
		*--ep = (n % 10) + '0';
		n /= 10;
	}
	*--ep = n + '0';
	return ep;
}

/*
 * Expand the size of the cd_devs array if necessary.
 *
 * The caller must hold alldevs_mtx. config_makeroom() may release and
 * re-acquire alldevs_mtx, so callers should re-check conditions such
 * as alldevs_nwrite == 0 and alldevs_nread == 0 when config_makeroom()
 * returns.
 */
static void
config_makeroom(int n, struct cfdriver *cd)
{
	int ondevs, nndevs;
	device_t *osp, *nsp;

	alldevs_nwrite++;

	for (nndevs = MAX(4, cd->cd_ndevs); nndevs <= n; nndevs += nndevs)
		;

	while (n >= cd->cd_ndevs) {
		/*
		 * Need to expand the array.
		 */
		ondevs = cd->cd_ndevs;
		osp = cd->cd_devs;

		/* Release alldevs_mtx around allocation, which may
		 * sleep.
		 */
		mutex_exit(&alldevs_mtx);
		nsp = kmem_alloc(sizeof(device_t[nndevs]), KM_SLEEP);
		if (nsp == NULL)
			panic("%s: could not expand cd_devs", __func__);
		mutex_enter(&alldevs_mtx);

		/* If another thread moved the array while we did
		 * not hold alldevs_mtx, try again.
		 */
		if (cd->cd_devs != osp) {
			mutex_exit(&alldevs_mtx);
			kmem_free(nsp, sizeof(device_t[nndevs]));
			mutex_enter(&alldevs_mtx);
			continue;
		}

		memset(nsp + ondevs, 0, sizeof(device_t[nndevs - ondevs]));
		if (ondevs != 0)
			memcpy(nsp, cd->cd_devs, sizeof(device_t[ondevs]));

		cd->cd_ndevs = nndevs;
		cd->cd_devs = nsp;
		if (ondevs != 0) {
			mutex_exit(&alldevs_mtx);
			kmem_free(osp, sizeof(device_t[ondevs]));
			mutex_enter(&alldevs_mtx);
		}
	}
	alldevs_nwrite--;
}

/*
 * Put dev into the devices list.
 */
static void
config_devlink(device_t dev)
{
	int s;

	s = config_alldevs_lock();

	KASSERT(device_cfdriver(dev)->cd_devs[dev->dv_unit] == dev);

	dev->dv_add_gen = alldevs_gen;
	/* It is safe to add a device to the tail of the list while
	 * readers and writers are in the list.
	 */
	TAILQ_INSERT_TAIL(&alldevs, dev, dv_list);
	config_alldevs_unlock(s);
}

static void
config_devfree(device_t dev)
{
	int priv = (dev->dv_flags & DVF_PRIV_ALLOC);

	if (dev->dv_cfattach->ca_devsize > 0)
		kmem_free(dev->dv_private, dev->dv_cfattach->ca_devsize);
	if (priv)
		kmem_free(dev, sizeof(*dev));
}

/*
 * Caller must hold alldevs_mtx.
 */
static void
config_devunlink(device_t dev, struct devicelist *garbage)
{
	struct device_garbage *dg = &dev->dv_garbage;
	cfdriver_t cd = device_cfdriver(dev);
	int i;

	KASSERT(mutex_owned(&alldevs_mtx));

 	/* Unlink from device list.  Link to garbage list. */
	TAILQ_REMOVE(&alldevs, dev, dv_list);
	TAILQ_INSERT_TAIL(garbage, dev, dv_list);

	/* Remove from cfdriver's array. */
	cd->cd_devs[dev->dv_unit] = NULL;

	/*
	 * If the device now has no units in use, unlink its softc array.
	 */
	for (i = 0; i < cd->cd_ndevs; i++) {
		if (cd->cd_devs[i] != NULL)
			break;
	}
	/* Nothing found.  Unlink, now.  Deallocate, later. */
	if (i == cd->cd_ndevs) {
		dg->dg_ndevs = cd->cd_ndevs;
		dg->dg_devs = cd->cd_devs;
		cd->cd_devs = NULL;
		cd->cd_ndevs = 0;
	}
}

static void
config_devdelete(device_t dev)
{
	struct device_garbage *dg = &dev->dv_garbage;
	device_lock_t dvl = device_getlock(dev);

	if (dg->dg_devs != NULL)
		kmem_free(dg->dg_devs, sizeof(device_t[dg->dg_ndevs]));

	cv_destroy(&dvl->dvl_cv);
	mutex_destroy(&dvl->dvl_mtx);

	KASSERT(dev->dv_properties != NULL);
	prop_object_release(dev->dv_properties);

	if (dev->dv_activity_handlers)
		panic("%s with registered handlers", __func__);

	if (dev->dv_locators) {
		size_t amount = *--dev->dv_locators;
		kmem_free(dev->dv_locators, amount);
	}

	config_devfree(dev);
}

static int
config_unit_nextfree(cfdriver_t cd, cfdata_t cf)
{
	int unit;

	if (cf->cf_fstate == FSTATE_STAR) {
		for (unit = cf->cf_unit; unit < cd->cd_ndevs; unit++)
			if (cd->cd_devs[unit] == NULL)
				break;
		/*
		 * unit is now the unit of the first NULL device pointer,
		 * or max(cd->cd_ndevs,cf->cf_unit).
		 */
	} else {
		unit = cf->cf_unit;
		if (unit < cd->cd_ndevs && cd->cd_devs[unit] != NULL)
			unit = -1;
	}
	return unit;
}

static int
config_unit_alloc(device_t dev, cfdriver_t cd, cfdata_t cf)
{
	struct alldevs_foray af;
	int unit;

	config_alldevs_enter(&af);
	for (;;) {
		unit = config_unit_nextfree(cd, cf);
		if (unit == -1)
			break;
		if (unit < cd->cd_ndevs) {
			cd->cd_devs[unit] = dev;
			dev->dv_unit = unit;
			break;
		}
		config_makeroom(unit, cd);
	}
	config_alldevs_exit(&af);

	return unit;
}

static device_t
config_devalloc(const device_t parent, const cfdata_t cf, const int *locs)
{
	cfdriver_t cd;
	cfattach_t ca;
	size_t lname, lunit;
	const char *xunit;
	int myunit;
	char num[10];
	device_t dev;
	void *dev_private;
	const struct cfiattrdata *ia;
	device_lock_t dvl;

	cd = config_cfdriver_lookup(cf->cf_name);
	if (cd == NULL)
		return NULL;

	ca = config_cfattach_lookup_cd(cd, cf->cf_atname);
	if (ca == NULL)
		return NULL;

	if ((ca->ca_flags & DVF_PRIV_ALLOC) == 0 &&
	    ca->ca_devsize < sizeof(struct device))
		panic("config_devalloc: %s (%zu < %zu)", cf->cf_atname,
		    ca->ca_devsize, sizeof(struct device));

	/* get memory for all device vars */
	KASSERT((ca->ca_flags & DVF_PRIV_ALLOC) || ca->ca_devsize >= sizeof(struct device));
	if (ca->ca_devsize > 0) {
		dev_private = kmem_zalloc(ca->ca_devsize, KM_SLEEP);
		if (dev_private == NULL)
			panic("config_devalloc: memory allocation for device softc failed");
	} else {
		KASSERT(ca->ca_flags & DVF_PRIV_ALLOC);
		dev_private = NULL;
	}

	if ((ca->ca_flags & DVF_PRIV_ALLOC) != 0) {
		dev = kmem_zalloc(sizeof(*dev), KM_SLEEP);
	} else {
		dev = dev_private;
#ifdef DIAGNOSTIC
		printf("%s has not been converted to device_t\n", cd->cd_name);
#endif
	}
	if (dev == NULL)
		panic("config_devalloc: memory allocation for device_t failed");

	dev->dv_class = cd->cd_class;
	dev->dv_cfdata = cf;
	dev->dv_cfdriver = cd;
	dev->dv_cfattach = ca;
	dev->dv_activity_count = 0;
	dev->dv_activity_handlers = NULL;
	dev->dv_private = dev_private;
	dev->dv_flags = ca->ca_flags;	/* inherit flags from class */

	myunit = config_unit_alloc(dev, cd, cf);
	if (myunit == -1) {
		config_devfree(dev);
		return NULL;
	}

	/* compute length of name and decimal expansion of unit number */
	lname = strlen(cd->cd_name);
	xunit = number(&num[sizeof(num)], myunit);
	lunit = &num[sizeof(num)] - xunit;
	if (lname + lunit > sizeof(dev->dv_xname))
		panic("config_devalloc: device name too long");

	dvl = device_getlock(dev);

	mutex_init(&dvl->dvl_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&dvl->dvl_cv, "pmfsusp");

	memcpy(dev->dv_xname, cd->cd_name, lname);
	memcpy(dev->dv_xname + lname, xunit, lunit);
	dev->dv_parent = parent;
	if (parent != NULL)
		dev->dv_depth = parent->dv_depth + 1;
	else
		dev->dv_depth = 0;
	dev->dv_flags |= DVF_ACTIVE;	/* always initially active */
	if (locs) {
		KASSERT(parent); /* no locators at root */
		ia = cfiattr_lookup(cfdata_ifattr(cf), parent->dv_cfdriver);
		dev->dv_locators =
		    kmem_alloc(sizeof(int [ia->ci_loclen + 1]), KM_SLEEP);
		*dev->dv_locators++ = sizeof(int [ia->ci_loclen + 1]);
		memcpy(dev->dv_locators, locs, sizeof(int [ia->ci_loclen]));
	}
	dev->dv_properties = prop_dictionary_create();
	KASSERT(dev->dv_properties != NULL);

	prop_dictionary_set_cstring_nocopy(dev->dv_properties,
	    "device-driver", dev->dv_cfdriver->cd_name);
	prop_dictionary_set_uint16(dev->dv_properties,
	    "device-unit", dev->dv_unit);

	if (dev->dv_cfdriver->cd_attrs != NULL)
		config_add_attrib_dict(dev);

	return dev;
}

/*
 * Create an array of device attach attributes and add it
 * to the device's dv_properties dictionary.
 *
 * <key>interface-attributes</key>
 * <array>
 *    <dict>
 *       <key>attribute-name</key>
 *       <string>foo</string>
 *       <key>locators</key>
 *       <array>
 *          <dict>
 *             <key>loc-name</key>
 *             <string>foo-loc1</string>
 *          </dict>
 *          <dict>
 *             <key>loc-name</key>
 *             <string>foo-loc2</string>
 *             <key>default</key>
 *             <string>foo-loc2-default</string>
 *          </dict>
 *          ...
 *       </array>
 *    </dict>
 *    ...
 * </array>
 */

static void
config_add_attrib_dict(device_t dev)
{
	int i, j;
	const struct cfiattrdata *ci;
	prop_dictionary_t attr_dict, loc_dict;
	prop_array_t attr_array, loc_array;

	if ((attr_array = prop_array_create()) == NULL)
		return;

	for (i = 0; ; i++) {
		if ((ci = dev->dv_cfdriver->cd_attrs[i]) == NULL)
			break;
		if ((attr_dict = prop_dictionary_create()) == NULL)
			break;
		prop_dictionary_set_cstring_nocopy(attr_dict, "attribute-name",
		    ci->ci_name);

		/* Create an array of the locator names and defaults */

		if (ci->ci_loclen != 0 &&
		    (loc_array = prop_array_create()) != NULL) {
			for (j = 0; j < ci->ci_loclen; j++) {
				loc_dict = prop_dictionary_create();
				if (loc_dict == NULL)
					continue;
				prop_dictionary_set_cstring_nocopy(loc_dict,
				    "loc-name", ci->ci_locdesc[j].cld_name);
				if (ci->ci_locdesc[j].cld_defaultstr != NULL)
					prop_dictionary_set_cstring_nocopy(
					    loc_dict, "default",
					    ci->ci_locdesc[j].cld_defaultstr);
				prop_array_set(loc_array, j, loc_dict);
				prop_object_release(loc_dict);
			}
			prop_dictionary_set_and_rel(attr_dict, "locators",
			    loc_array);
		}
		prop_array_add(attr_array, attr_dict);
		prop_object_release(attr_dict);
	}
	if (i == 0)
		prop_object_release(attr_array);
	else
		prop_dictionary_set_and_rel(dev->dv_properties,
		    "interface-attributes", attr_array);

	return;
}

/*
 * Attach a found device.
 */
device_t
config_attach_loc(device_t parent, cfdata_t cf,
	const int *locs, void *aux, cfprint_t print)
{
	device_t dev;
	struct cftable *ct;
	const char *drvname;

	dev = config_devalloc(parent, cf, locs);
	if (!dev)
		panic("config_attach: allocation of device softc failed");

	/* XXX redundant - see below? */
	if (cf->cf_fstate != FSTATE_STAR) {
		KASSERT(cf->cf_fstate == FSTATE_NOTFOUND);
		cf->cf_fstate = FSTATE_FOUND;
	}

	config_devlink(dev);

	if (config_do_twiddle && cold)
		twiddle();
	else
		aprint_naive("Found ");
	/*
	 * We want the next two printfs for normal, verbose, and quiet,
	 * but not silent (in which case, we're twiddling, instead).
	 */
	if (parent == ROOT) {
		aprint_naive("%s (root)", device_xname(dev));
		aprint_normal("%s (root)", device_xname(dev));
	} else {
		aprint_naive("%s at %s", device_xname(dev), device_xname(parent));
		aprint_normal("%s at %s", device_xname(dev), device_xname(parent));
		if (print)
			(void) (*print)(aux, NULL);
	}

	/*
	 * Before attaching, clobber any unfound devices that are
	 * otherwise identical.
	 * XXX code above is redundant?
	 */
	drvname = dev->dv_cfdriver->cd_name;
	TAILQ_FOREACH(ct, &allcftables, ct_list) {
		for (cf = ct->ct_cfdata; cf->cf_name; cf++) {
			if (STREQ(cf->cf_name, drvname) &&
			    cf->cf_unit == dev->dv_unit) {
				if (cf->cf_fstate == FSTATE_NOTFOUND)
					cf->cf_fstate = FSTATE_FOUND;
			}
		}
	}
	device_register(dev, aux);

	/* Let userland know */
	devmon_report_device(dev, true);

	(*dev->dv_cfattach->ca_attach)(parent, dev, aux);

	if (!device_pmf_is_registered(dev))
		aprint_debug_dev(dev, "WARNING: power management not supported\n");

	config_process_deferred(&deferred_config_queue, dev);

	device_register_post_config(dev, aux);
	return dev;
}

device_t
config_attach(device_t parent, cfdata_t cf, void *aux, cfprint_t print)
{

	return config_attach_loc(parent, cf, NULL, aux, print);
}

/*
 * As above, but for pseudo-devices.  Pseudo-devices attached in this
 * way are silently inserted into the device tree, and their children
 * attached.
 *
 * Note that because pseudo-devices are attached silently, any information
 * the attach routine wishes to print should be prefixed with the device
 * name by the attach routine.
 */
device_t
config_attach_pseudo(cfdata_t cf)
{
	device_t dev;

	dev = config_devalloc(ROOT, cf, NULL);
	if (!dev)
		return NULL;

	/* XXX mark busy in cfdata */

	if (cf->cf_fstate != FSTATE_STAR) {
		KASSERT(cf->cf_fstate == FSTATE_NOTFOUND);
		cf->cf_fstate = FSTATE_FOUND;
	}

	config_devlink(dev);

#if 0	/* XXXJRT not yet */
	device_register(dev, NULL);	/* like a root node */
#endif

	/* Let userland know */
	devmon_report_device(dev, true);

	(*dev->dv_cfattach->ca_attach)(ROOT, dev, NULL);

	config_process_deferred(&deferred_config_queue, dev);
	return dev;
}

/*
 * Caller must hold alldevs_mtx.
 */
static void
config_collect_garbage(struct devicelist *garbage)
{
	device_t dv;

	KASSERT(!cpu_intr_p());
	KASSERT(!cpu_softintr_p());
	KASSERT(mutex_owned(&alldevs_mtx));

	while (alldevs_nwrite == 0 && alldevs_nread == 0 && alldevs_garbage) {
		TAILQ_FOREACH(dv, &alldevs, dv_list) {
			if (dv->dv_del_gen != 0)
				break;
		}
		if (dv == NULL) {
			alldevs_garbage = false;
			break;
		}
		config_devunlink(dv, garbage);
	}
	KASSERT(mutex_owned(&alldevs_mtx));
}

static void
config_dump_garbage(struct devicelist *garbage)
{
	device_t dv;

	while ((dv = TAILQ_FIRST(garbage)) != NULL) {
		TAILQ_REMOVE(garbage, dv, dv_list);
		config_devdelete(dv);
	}
}

/*
 * Detach a device.  Optionally forced (e.g. because of hardware
 * removal) and quiet.  Returns zero if successful, non-zero
 * (an error code) otherwise.
 *
 * Note that this code wants to be run from a process context, so
 * that the detach can sleep to allow processes which have a device
 * open to run and unwind their stacks.
 */
int
config_detach(device_t dev, int flags)
{
	struct alldevs_foray af;
	struct cftable *ct;
	cfdata_t cf;
	const struct cfattach *ca;
	struct cfdriver *cd;
#ifdef DIAGNOSTIC
	device_t d;
#endif
	int rv = 0, s;

#ifdef DIAGNOSTIC
	cf = dev->dv_cfdata;
	if (cf != NULL && cf->cf_fstate != FSTATE_FOUND &&
	    cf->cf_fstate != FSTATE_STAR)
		panic("config_detach: %s: bad device fstate %d",
		    device_xname(dev), cf ? cf->cf_fstate : -1);
#endif
	cd = dev->dv_cfdriver;
	KASSERT(cd != NULL);

	ca = dev->dv_cfattach;
	KASSERT(ca != NULL);

	s = config_alldevs_lock();
	if (dev->dv_del_gen != 0) {
		config_alldevs_unlock(s);
#ifdef DIAGNOSTIC
		printf("%s: %s is already detached\n", __func__,
		    device_xname(dev));
#endif /* DIAGNOSTIC */
		return ENOENT;
	}
	alldevs_nwrite++;
	config_alldevs_unlock(s);

	if (!detachall &&
	    (flags & (DETACH_SHUTDOWN|DETACH_FORCE)) == DETACH_SHUTDOWN &&
	    (dev->dv_flags & DVF_DETACH_SHUTDOWN) == 0) {
		rv = EOPNOTSUPP;
	} else if (ca->ca_detach != NULL) {
		rv = (*ca->ca_detach)(dev, flags);
	} else
		rv = EOPNOTSUPP;

	/*
	 * If it was not possible to detach the device, then we either
	 * panic() (for the forced but failed case), or return an error.
	 *
	 * If it was possible to detach the device, ensure that the
	 * device is deactivated.
	 */
	if (rv == 0)
		dev->dv_flags &= ~DVF_ACTIVE;
	else if ((flags & DETACH_FORCE) == 0)
		goto out;
	else {
		panic("config_detach: forced detach of %s failed (%d)",
		    device_xname(dev), rv);
	}

	/*
	 * The device has now been successfully detached.
	 */

	/* Let userland know */
	devmon_report_device(dev, false);

#ifdef DIAGNOSTIC
	/*
	 * Sanity: If you're successfully detached, you should have no
	 * children.  (Note that because children must be attached
	 * after parents, we only need to search the latter part of
	 * the list.)
	 */
	for (d = TAILQ_NEXT(dev, dv_list); d != NULL;
	    d = TAILQ_NEXT(d, dv_list)) {
		if (d->dv_parent == dev && d->dv_del_gen == 0) {
			printf("config_detach: detached device %s"
			    " has children %s\n", device_xname(dev), device_xname(d));
			panic("config_detach");
		}
	}
#endif

	/* notify the parent that the child is gone */
	if (dev->dv_parent) {
		device_t p = dev->dv_parent;
		if (p->dv_cfattach->ca_childdetached)
			(*p->dv_cfattach->ca_childdetached)(p, dev);
	}

	/*
	 * Mark cfdata to show that the unit can be reused, if possible.
	 */
	TAILQ_FOREACH(ct, &allcftables, ct_list) {
		for (cf = ct->ct_cfdata; cf->cf_name; cf++) {
			if (STREQ(cf->cf_name, cd->cd_name)) {
				if (cf->cf_fstate == FSTATE_FOUND &&
				    cf->cf_unit == dev->dv_unit)
					cf->cf_fstate = FSTATE_NOTFOUND;
			}
		}
	}

	if (dev->dv_cfdata != NULL && (flags & DETACH_QUIET) == 0)
		aprint_normal_dev(dev, "detached\n");

out:
	config_alldevs_enter(&af);
	KASSERT(alldevs_nwrite != 0);
	--alldevs_nwrite;
	if (rv == 0 && dev->dv_del_gen == 0) {
		if (alldevs_nwrite == 0 && alldevs_nread == 0)
			config_devunlink(dev, &af.af_garbage);
		else {
			dev->dv_del_gen = alldevs_gen;
			alldevs_garbage = true;
		}
	}
	config_alldevs_exit(&af);

	return rv;
}

int
config_detach_children(device_t parent, int flags)
{
	device_t dv;
	deviter_t di;
	int error = 0;

	for (dv = deviter_first(&di, DEVITER_F_RW); dv != NULL;
	     dv = deviter_next(&di)) {
		if (device_parent(dv) != parent)
			continue;
		if ((error = config_detach(dv, flags)) != 0)
			break;
	}
	deviter_release(&di);
	return error;
}

device_t
shutdown_first(struct shutdown_state *s)
{
	if (!s->initialized) {
		deviter_init(&s->di, DEVITER_F_SHUTDOWN|DEVITER_F_LEAVES_FIRST);
		s->initialized = true;
	}
	return shutdown_next(s);
}

device_t
shutdown_next(struct shutdown_state *s)
{
	device_t dv;

	while ((dv = deviter_next(&s->di)) != NULL && !device_is_active(dv))
		;

	if (dv == NULL)
		s->initialized = false;

	return dv;
}

bool
config_detach_all(int how)
{
	static struct shutdown_state s;
	device_t curdev;
	bool progress = false;

	if ((how & RB_NOSYNC) != 0)
		return false;

	for (curdev = shutdown_first(&s); curdev != NULL;
	     curdev = shutdown_next(&s)) {
		aprint_debug(" detaching %s, ", device_xname(curdev));
		if (config_detach(curdev, DETACH_SHUTDOWN) == 0) {
			progress = true;
			aprint_debug("success.");
		} else
			aprint_debug("failed.");
	}
	return progress;
}

static bool
device_is_ancestor_of(device_t ancestor, device_t descendant)
{
	device_t dv;

	for (dv = descendant; dv != NULL; dv = device_parent(dv)) {
		if (device_parent(dv) == ancestor)
			return true;
	}
	return false;
}

int
config_deactivate(device_t dev)
{
	deviter_t di;
	const struct cfattach *ca;
	device_t descendant;
	int s, rv = 0, oflags;

	for (descendant = deviter_first(&di, DEVITER_F_ROOT_FIRST);
	     descendant != NULL;
	     descendant = deviter_next(&di)) {
		if (dev != descendant &&
		    !device_is_ancestor_of(dev, descendant))
			continue;

		if ((descendant->dv_flags & DVF_ACTIVE) == 0)
			continue;

		ca = descendant->dv_cfattach;
		oflags = descendant->dv_flags;

		descendant->dv_flags &= ~DVF_ACTIVE;
		if (ca->ca_activate == NULL)
			continue;
		s = splhigh();
		rv = (*ca->ca_activate)(descendant, DVACT_DEACTIVATE);
		splx(s);
		if (rv != 0)
			descendant->dv_flags = oflags;
	}
	deviter_release(&di);
	return rv;
}

/*
 * Defer the configuration of the specified device until all
 * of its parent's devices have been attached.
 */
void
config_defer(device_t dev, void (*func)(device_t))
{
	struct deferred_config *dc;

	if (dev->dv_parent == NULL)
		panic("config_defer: can't defer config of a root device");

#ifdef DIAGNOSTIC
	TAILQ_FOREACH(dc, &deferred_config_queue, dc_queue) {
		if (dc->dc_dev == dev)
			panic("config_defer: deferred twice");
	}
#endif

	dc = kmem_alloc(sizeof(*dc), KM_SLEEP);
	if (dc == NULL)
		panic("config_defer: unable to allocate callback");

	dc->dc_dev = dev;
	dc->dc_func = func;
	TAILQ_INSERT_TAIL(&deferred_config_queue, dc, dc_queue);
	config_pending_incr(dev);
}

/*
 * Defer some autoconfiguration for a device until after interrupts
 * are enabled.
 */
void
config_interrupts(device_t dev, void (*func)(device_t))
{
	struct deferred_config *dc;

	/*
	 * If interrupts are enabled, callback now.
	 */
	if (cold == 0) {
		(*func)(dev);
		return;
	}

#ifdef DIAGNOSTIC
	TAILQ_FOREACH(dc, &interrupt_config_queue, dc_queue) {
		if (dc->dc_dev == dev)
			panic("config_interrupts: deferred twice");
	}
#endif

	dc = kmem_alloc(sizeof(*dc), KM_SLEEP);
	if (dc == NULL)
		panic("config_interrupts: unable to allocate callback");

	dc->dc_dev = dev;
	dc->dc_func = func;
	TAILQ_INSERT_TAIL(&interrupt_config_queue, dc, dc_queue);
	config_pending_incr(dev);
}

/*
 * Defer some autoconfiguration for a device until after root file system
 * is mounted (to load firmware etc).
 */
void
config_mountroot(device_t dev, void (*func)(device_t))
{
	struct deferred_config *dc;

	/*
	 * If root file system is mounted, callback now.
	 */
	if (root_is_mounted) {
		(*func)(dev);
		return;
	}

#ifdef DIAGNOSTIC
	TAILQ_FOREACH(dc, &mountroot_config_queue, dc_queue) {
		if (dc->dc_dev == dev)
			panic("%s: deferred twice", __func__);
	}
#endif

	dc = kmem_alloc(sizeof(*dc), KM_SLEEP);
	if (dc == NULL)
		panic("%s: unable to allocate callback", __func__);

	dc->dc_dev = dev;
	dc->dc_func = func;
	TAILQ_INSERT_TAIL(&mountroot_config_queue, dc, dc_queue);
}

/*
 * Process a deferred configuration queue.
 */
static void
config_process_deferred(struct deferred_config_head *queue,
    device_t parent)
{
	struct deferred_config *dc, *ndc;

	for (dc = TAILQ_FIRST(queue); dc != NULL; dc = ndc) {
		ndc = TAILQ_NEXT(dc, dc_queue);
		if (parent == NULL || dc->dc_dev->dv_parent == parent) {
			TAILQ_REMOVE(queue, dc, dc_queue);
			(*dc->dc_func)(dc->dc_dev);
			config_pending_decr(dc->dc_dev);
			kmem_free(dc, sizeof(*dc));
		}
	}
}

/*
 * Manipulate the config_pending semaphore.
 */
void
config_pending_incr(device_t dev)
{

	mutex_enter(&config_misc_lock);
	config_pending++;
#ifdef DEBUG_AUTOCONF
	printf("%s: %s %d\n", __func__, device_xname(dev), config_pending);
#endif
	mutex_exit(&config_misc_lock);
}

void
config_pending_decr(device_t dev)
{

#ifdef DIAGNOSTIC
	if (config_pending == 0)
		panic("config_pending_decr: config_pending == 0");
#endif
	mutex_enter(&config_misc_lock);
	config_pending--;
#ifdef DEBUG_AUTOCONF
	printf("%s: %s %d\n", __func__, device_xname(dev), config_pending);
#endif
	if (config_pending == 0)
		cv_broadcast(&config_misc_cv);
	mutex_exit(&config_misc_lock);
}

/*
 * Register a "finalization" routine.  Finalization routines are
 * called iteratively once all real devices have been found during
 * autoconfiguration, for as long as any one finalizer has done
 * any work.
 */
int
config_finalize_register(device_t dev, int (*fn)(device_t))
{
	struct finalize_hook *f;

	/*
	 * If finalization has already been done, invoke the
	 * callback function now.
	 */
	if (config_finalize_done) {
		while ((*fn)(dev) != 0)
			/* loop */ ;
	}

	/* Ensure this isn't already on the list. */
	TAILQ_FOREACH(f, &config_finalize_list, f_list) {
		if (f->f_func == fn && f->f_dev == dev)
			return EEXIST;
	}

	f = kmem_alloc(sizeof(*f), KM_SLEEP);
	f->f_func = fn;
	f->f_dev = dev;
	TAILQ_INSERT_TAIL(&config_finalize_list, f, f_list);

	return 0;
}

void
config_finalize(void)
{
	struct finalize_hook *f;
	struct pdevinit *pdev;
	extern struct pdevinit pdevinit[];
	int errcnt, rv;

	/*
	 * Now that device driver threads have been created, wait for
	 * them to finish any deferred autoconfiguration.
	 */
	mutex_enter(&config_misc_lock);
	while (config_pending != 0)
		cv_wait(&config_misc_cv, &config_misc_lock);
	mutex_exit(&config_misc_lock);

	KERNEL_LOCK(1, NULL);

	/* Attach pseudo-devices. */
	for (pdev = pdevinit; pdev->pdev_attach != NULL; pdev++)
		(*pdev->pdev_attach)(pdev->pdev_count);

	/* Run the hooks until none of them does any work. */
	do {
		rv = 0;
		TAILQ_FOREACH(f, &config_finalize_list, f_list)
			rv |= (*f->f_func)(f->f_dev);
	} while (rv != 0);

	config_finalize_done = 1;

	/* Now free all the hooks. */
	while ((f = TAILQ_FIRST(&config_finalize_list)) != NULL) {
		TAILQ_REMOVE(&config_finalize_list, f, f_list);
		kmem_free(f, sizeof(*f));
	}

	KERNEL_UNLOCK_ONE(NULL);

	errcnt = aprint_get_error_count();
	if ((boothowto & (AB_QUIET|AB_SILENT)) != 0 &&
	    (boothowto & AB_VERBOSE) == 0) {
		mutex_enter(&config_misc_lock);
		if (config_do_twiddle) {
			config_do_twiddle = 0;
			printf_nolog(" done.\n");
		}
		mutex_exit(&config_misc_lock);
		if (errcnt != 0) {
			printf("WARNING: %d error%s while detecting hardware; "
			    "check system log.\n", errcnt,
			    errcnt == 1 ? "" : "s");
		}
	}
}

void
config_twiddle_init(void)
{

	if ((boothowto & (AB_SILENT|AB_VERBOSE)) == AB_SILENT) {
		config_do_twiddle = 1;
	}
	callout_setfunc(&config_twiddle_ch, config_twiddle_fn, NULL);
}

void
config_twiddle_fn(void *cookie)
{

	mutex_enter(&config_misc_lock);
	if (config_do_twiddle) {
		twiddle();
		callout_schedule(&config_twiddle_ch, mstohz(100));
	}
	mutex_exit(&config_misc_lock);
}

static int
config_alldevs_lock(void)
{
	mutex_enter(&alldevs_mtx);
	return 0;
}

static void
config_alldevs_enter(struct alldevs_foray *af)
{
	TAILQ_INIT(&af->af_garbage);
	af->af_s = config_alldevs_lock();
	config_collect_garbage(&af->af_garbage);
} 

static void
config_alldevs_exit(struct alldevs_foray *af)
{
	config_alldevs_unlock(af->af_s);
	config_dump_garbage(&af->af_garbage);
}

/*ARGSUSED*/
static void
config_alldevs_unlock(int s)
{
	mutex_exit(&alldevs_mtx);
}

/*
 * device_lookup:
 *
 *	Look up a device instance for a given driver.
 */
device_t
device_lookup(cfdriver_t cd, int unit)
{
	device_t dv;
	int s;

	s = config_alldevs_lock();
	KASSERT(mutex_owned(&alldevs_mtx));
	if (unit < 0 || unit >= cd->cd_ndevs)
		dv = NULL;
	else if ((dv = cd->cd_devs[unit]) != NULL && dv->dv_del_gen != 0)
		dv = NULL;
	config_alldevs_unlock(s);

	return dv;
}

/*
 * device_lookup_private:
 *
 *	Look up a softc instance for a given driver.
 */
void *
device_lookup_private(cfdriver_t cd, int unit)
{

	return device_private(device_lookup(cd, unit));
}

/*
 * device_find_by_xname:
 *
 *	Returns the device of the given name or NULL if it doesn't exist.
 */
device_t
device_find_by_xname(const char *name)
{
	device_t dv;
	deviter_t di;

	for (dv = deviter_first(&di, 0); dv != NULL; dv = deviter_next(&di)) {
		if (strcmp(device_xname(dv), name) == 0)
			break;
	}
	deviter_release(&di);

	return dv;
}

/*
 * device_find_by_driver_unit:
 *
 *	Returns the device of the given driver name and unit or
 *	NULL if it doesn't exist.
 */
device_t
device_find_by_driver_unit(const char *name, int unit)
{
	struct cfdriver *cd;

	if ((cd = config_cfdriver_lookup(name)) == NULL)
		return NULL;
	return device_lookup(cd, unit);
}

/*
 * Power management related functions.
 */

bool
device_pmf_is_registered(device_t dev)
{
	return (dev->dv_flags & DVF_POWER_HANDLERS) != 0;
}

bool
device_pmf_driver_suspend(device_t dev, const pmf_qual_t *qual)
{
	if ((dev->dv_flags & DVF_DRIVER_SUSPENDED) != 0)
		return true;
	if ((dev->dv_flags & DVF_CLASS_SUSPENDED) == 0)
		return false;
	if (pmf_qual_depth(qual) <= DEVACT_LEVEL_DRIVER &&
	    dev->dv_driver_suspend != NULL &&
	    !(*dev->dv_driver_suspend)(dev, qual))
		return false;

	dev->dv_flags |= DVF_DRIVER_SUSPENDED;
	return true;
}

bool
device_pmf_driver_resume(device_t dev, const pmf_qual_t *qual)
{
	if ((dev->dv_flags & DVF_DRIVER_SUSPENDED) == 0)
		return true;
	if ((dev->dv_flags & DVF_BUS_SUSPENDED) != 0)
		return false;
	if (pmf_qual_depth(qual) <= DEVACT_LEVEL_DRIVER &&
	    dev->dv_driver_resume != NULL &&
	    !(*dev->dv_driver_resume)(dev, qual))
		return false;

	dev->dv_flags &= ~DVF_DRIVER_SUSPENDED;
	return true;
}

bool
device_pmf_driver_shutdown(device_t dev, int how)
{

	if (*dev->dv_driver_shutdown != NULL &&
	    !(*dev->dv_driver_shutdown)(dev, how))
		return false;
	return true;
}

bool
device_pmf_driver_register(device_t dev,
    bool (*suspend)(device_t, const pmf_qual_t *),
    bool (*resume)(device_t, const pmf_qual_t *),
    bool (*shutdown)(device_t, int))
{
	dev->dv_driver_suspend = suspend;
	dev->dv_driver_resume = resume;
	dev->dv_driver_shutdown = shutdown;
	dev->dv_flags |= DVF_POWER_HANDLERS;
	return true;
}

static const char *
curlwp_name(void)
{
	if (curlwp->l_name != NULL)
		return curlwp->l_name;
	else
		return curlwp->l_proc->p_comm;
}

void
device_pmf_driver_deregister(device_t dev)
{
	device_lock_t dvl = device_getlock(dev);

	dev->dv_driver_suspend = NULL;
	dev->dv_driver_resume = NULL;

	mutex_enter(&dvl->dvl_mtx);
	dev->dv_flags &= ~DVF_POWER_HANDLERS;
	while (dvl->dvl_nlock > 0 || dvl->dvl_nwait > 0) {
		/* Wake a thread that waits for the lock.  That
		 * thread will fail to acquire the lock, and then
		 * it will wake the next thread that waits for the
		 * lock, or else it will wake us.
		 */
		cv_signal(&dvl->dvl_cv);
		pmflock_debug(dev, __func__, __LINE__);
		cv_wait(&dvl->dvl_cv, &dvl->dvl_mtx);
		pmflock_debug(dev, __func__, __LINE__);
	}
	mutex_exit(&dvl->dvl_mtx);
}

bool
device_pmf_driver_child_register(device_t dev)
{
	device_t parent = device_parent(dev);

	if (parent == NULL || parent->dv_driver_child_register == NULL)
		return true;
	return (*parent->dv_driver_child_register)(dev);
}

void
device_pmf_driver_set_child_register(device_t dev,
    bool (*child_register)(device_t))
{
	dev->dv_driver_child_register = child_register;
}

static void
pmflock_debug(device_t dev, const char *func, int line)
{
	device_lock_t dvl = device_getlock(dev);

	aprint_debug_dev(dev, "%s.%d, %s dvl_nlock %d dvl_nwait %d dv_flags %x\n",
	    func, line, curlwp_name(), dvl->dvl_nlock, dvl->dvl_nwait,
	    dev->dv_flags);
}

static bool
device_pmf_lock1(device_t dev)
{
	device_lock_t dvl = device_getlock(dev);

	while (device_pmf_is_registered(dev) &&
	    dvl->dvl_nlock > 0 && dvl->dvl_holder != curlwp) {
		dvl->dvl_nwait++;
		pmflock_debug(dev, __func__, __LINE__);
		cv_wait(&dvl->dvl_cv, &dvl->dvl_mtx);
		pmflock_debug(dev, __func__, __LINE__);
		dvl->dvl_nwait--;
	}
	if (!device_pmf_is_registered(dev)) {
		pmflock_debug(dev, __func__, __LINE__);
		/* We could not acquire the lock, but some other thread may
		 * wait for it, also.  Wake that thread.
		 */
		cv_signal(&dvl->dvl_cv);
		return false;
	}
	dvl->dvl_nlock++;
	dvl->dvl_holder = curlwp;
	pmflock_debug(dev, __func__, __LINE__);
	return true;
}

bool
device_pmf_lock(device_t dev)
{
	bool rc;
	device_lock_t dvl = device_getlock(dev);

	mutex_enter(&dvl->dvl_mtx);
	rc = device_pmf_lock1(dev);
	mutex_exit(&dvl->dvl_mtx);

	return rc;
}

void
device_pmf_unlock(device_t dev)
{
	device_lock_t dvl = device_getlock(dev);

	KASSERT(dvl->dvl_nlock > 0);
	mutex_enter(&dvl->dvl_mtx);
	if (--dvl->dvl_nlock == 0)
		dvl->dvl_holder = NULL;
	cv_signal(&dvl->dvl_cv);
	pmflock_debug(dev, __func__, __LINE__);
	mutex_exit(&dvl->dvl_mtx);
}

device_lock_t
device_getlock(device_t dev)
{
	return &dev->dv_lock;
}

void *
device_pmf_bus_private(device_t dev)
{
	return dev->dv_bus_private;
}

bool
device_pmf_bus_suspend(device_t dev, const pmf_qual_t *qual)
{
	if ((dev->dv_flags & DVF_BUS_SUSPENDED) != 0)
		return true;
	if ((dev->dv_flags & DVF_CLASS_SUSPENDED) == 0 ||
	    (dev->dv_flags & DVF_DRIVER_SUSPENDED) == 0)
		return false;
	if (pmf_qual_depth(qual) <= DEVACT_LEVEL_BUS &&
	    dev->dv_bus_suspend != NULL &&
	    !(*dev->dv_bus_suspend)(dev, qual))
		return false;

	dev->dv_flags |= DVF_BUS_SUSPENDED;
	return true;
}

bool
device_pmf_bus_resume(device_t dev, const pmf_qual_t *qual)
{
	if ((dev->dv_flags & DVF_BUS_SUSPENDED) == 0)
		return true;
	if (pmf_qual_depth(qual) <= DEVACT_LEVEL_BUS &&
	    dev->dv_bus_resume != NULL &&
	    !(*dev->dv_bus_resume)(dev, qual))
		return false;

	dev->dv_flags &= ~DVF_BUS_SUSPENDED;
	return true;
}

bool
device_pmf_bus_shutdown(device_t dev, int how)
{

	if (*dev->dv_bus_shutdown != NULL &&
	    !(*dev->dv_bus_shutdown)(dev, how))
		return false;
	return true;
}

void
device_pmf_bus_register(device_t dev, void *priv,
    bool (*suspend)(device_t, const pmf_qual_t *),
    bool (*resume)(device_t, const pmf_qual_t *),
    bool (*shutdown)(device_t, int), void (*deregister)(device_t))
{
	dev->dv_bus_private = priv;
	dev->dv_bus_resume = resume;
	dev->dv_bus_suspend = suspend;
	dev->dv_bus_shutdown = shutdown;
	dev->dv_bus_deregister = deregister;
}

void
device_pmf_bus_deregister(device_t dev)
{
	if (dev->dv_bus_deregister == NULL)
		return;
	(*dev->dv_bus_deregister)(dev);
	dev->dv_bus_private = NULL;
	dev->dv_bus_suspend = NULL;
	dev->dv_bus_resume = NULL;
	dev->dv_bus_deregister = NULL;
}

void *
device_pmf_class_private(device_t dev)
{
	return dev->dv_class_private;
}

bool
device_pmf_class_suspend(device_t dev, const pmf_qual_t *qual)
{
	if ((dev->dv_flags & DVF_CLASS_SUSPENDED) != 0)
		return true;
	if (pmf_qual_depth(qual) <= DEVACT_LEVEL_CLASS &&
	    dev->dv_class_suspend != NULL &&
	    !(*dev->dv_class_suspend)(dev, qual))
		return false;

	dev->dv_flags |= DVF_CLASS_SUSPENDED;
	return true;
}

bool
device_pmf_class_resume(device_t dev, const pmf_qual_t *qual)
{
	if ((dev->dv_flags & DVF_CLASS_SUSPENDED) == 0)
		return true;
	if ((dev->dv_flags & DVF_BUS_SUSPENDED) != 0 ||
	    (dev->dv_flags & DVF_DRIVER_SUSPENDED) != 0)
		return false;
	if (pmf_qual_depth(qual) <= DEVACT_LEVEL_CLASS &&
	    dev->dv_class_resume != NULL &&
	    !(*dev->dv_class_resume)(dev, qual))
		return false;

	dev->dv_flags &= ~DVF_CLASS_SUSPENDED;
	return true;
}

void
device_pmf_class_register(device_t dev, void *priv,
    bool (*suspend)(device_t, const pmf_qual_t *),
    bool (*resume)(device_t, const pmf_qual_t *),
    void (*deregister)(device_t))
{
	dev->dv_class_private = priv;
	dev->dv_class_suspend = suspend;
	dev->dv_class_resume = resume;
	dev->dv_class_deregister = deregister;
}

void
device_pmf_class_deregister(device_t dev)
{
	if (dev->dv_class_deregister == NULL)
		return;
	(*dev->dv_class_deregister)(dev);
	dev->dv_class_private = NULL;
	dev->dv_class_suspend = NULL;
	dev->dv_class_resume = NULL;
	dev->dv_class_deregister = NULL;
}

bool
device_active(device_t dev, devactive_t type)
{
	size_t i;

	if (dev->dv_activity_count == 0)
		return false;

	for (i = 0; i < dev->dv_activity_count; ++i) {
		if (dev->dv_activity_handlers[i] == NULL)
			break;
		(*dev->dv_activity_handlers[i])(dev, type);
	}

	return true;
}

bool
device_active_register(device_t dev, void (*handler)(device_t, devactive_t))
{
	void (**new_handlers)(device_t, devactive_t);
	void (**old_handlers)(device_t, devactive_t);
	size_t i, old_size, new_size;
	int s;

	old_handlers = dev->dv_activity_handlers;
	old_size = dev->dv_activity_count;

	for (i = 0; i < old_size; ++i) {
		KASSERT(old_handlers[i] != handler);
		if (old_handlers[i] == NULL) {
			old_handlers[i] = handler;
			return true;
		}
	}

	new_size = old_size + 4;
	new_handlers = kmem_alloc(sizeof(void *[new_size]), KM_SLEEP);

	memcpy(new_handlers, old_handlers, sizeof(void *[old_size]));
	new_handlers[old_size] = handler;
	memset(new_handlers + old_size + 1, 0,
	    sizeof(int [new_size - (old_size+1)]));

	s = splhigh();
	dev->dv_activity_count = new_size;
	dev->dv_activity_handlers = new_handlers;
	splx(s);

	if (old_handlers != NULL)
		kmem_free(old_handlers, sizeof(void * [old_size]));

	return true;
}

void
device_active_deregister(device_t dev, void (*handler)(device_t, devactive_t))
{
	void (**old_handlers)(device_t, devactive_t);
	size_t i, old_size;
	int s;

	old_handlers = dev->dv_activity_handlers;
	old_size = dev->dv_activity_count;

	for (i = 0; i < old_size; ++i) {
		if (old_handlers[i] == handler)
			break;
		if (old_handlers[i] == NULL)
			return; /* XXX panic? */
	}

	if (i == old_size)
		return; /* XXX panic? */

	for (; i < old_size - 1; ++i) {
		if ((old_handlers[i] = old_handlers[i + 1]) != NULL)
			continue;

		if (i == 0) {
			s = splhigh();
			dev->dv_activity_count = 0;
			dev->dv_activity_handlers = NULL;
			splx(s);
			kmem_free(old_handlers, sizeof(void *[old_size]));
		}
		return;
	}
	old_handlers[i] = NULL;
}

/* Return true iff the device_t `dev' exists at generation `gen'. */
static bool
device_exists_at(device_t dv, devgen_t gen)
{
	return (dv->dv_del_gen == 0 || dv->dv_del_gen > gen) &&
	    dv->dv_add_gen <= gen;
}

static bool
deviter_visits(const deviter_t *di, device_t dv)
{
	return device_exists_at(dv, di->di_gen);
}

/*
 * Device Iteration
 *
 * deviter_t: a device iterator.  Holds state for a "walk" visiting
 *     each device_t's in the device tree.
 *
 * deviter_init(di, flags): initialize the device iterator `di'
 *     to "walk" the device tree.  deviter_next(di) will return
 *     the first device_t in the device tree, or NULL if there are
 *     no devices.
 *
 *     `flags' is one or more of DEVITER_F_RW, indicating that the
 *     caller intends to modify the device tree by calling
 *     config_detach(9) on devices in the order that the iterator
 *     returns them; DEVITER_F_ROOT_FIRST, asking for the devices
 *     nearest the "root" of the device tree to be returned, first;
 *     DEVITER_F_LEAVES_FIRST, asking for the devices furthest from
 *     the root of the device tree, first; and DEVITER_F_SHUTDOWN,
 *     indicating both that deviter_init() should not respect any
 *     locks on the device tree, and that deviter_next(di) may run
 *     in more than one LWP before the walk has finished.
 *
 *     Only one DEVITER_F_RW iterator may be in the device tree at
 *     once.
 *
 *     DEVITER_F_SHUTDOWN implies DEVITER_F_RW.
 *
 *     Results are undefined if the flags DEVITER_F_ROOT_FIRST and
 *     DEVITER_F_LEAVES_FIRST are used in combination.
 *
 * deviter_first(di, flags): initialize the device iterator `di'
 *     and return the first device_t in the device tree, or NULL
 *     if there are no devices.  The statement
 *
 *         dv = deviter_first(di);
 *
 *     is shorthand for
 *
 *         deviter_init(di);
 *         dv = deviter_next(di);
 *
 * deviter_next(di): return the next device_t in the device tree,
 *     or NULL if there are no more devices.  deviter_next(di)
 *     is undefined if `di' was not initialized with deviter_init() or
 *     deviter_first().
 *
 * deviter_release(di): stops iteration (subsequent calls to
 *     deviter_next() will return NULL), releases any locks and
 *     resources held by the device iterator.
 *
 * Device iteration does not return device_t's in any particular
 * order.  An iterator will never return the same device_t twice.
 * Device iteration is guaranteed to complete---i.e., if deviter_next(di)
 * is called repeatedly on the same `di', it will eventually return
 * NULL.  It is ok to attach/detach devices during device iteration.
 */
void
deviter_init(deviter_t *di, deviter_flags_t flags)
{
	device_t dv;
	int s;

	memset(di, 0, sizeof(*di));

	s = config_alldevs_lock();
	if ((flags & DEVITER_F_SHUTDOWN) != 0)
		flags |= DEVITER_F_RW;

	if ((flags & DEVITER_F_RW) != 0)
		alldevs_nwrite++;
	else
		alldevs_nread++;
	di->di_gen = alldevs_gen++;
	config_alldevs_unlock(s);

	di->di_flags = flags;

	switch (di->di_flags & (DEVITER_F_LEAVES_FIRST|DEVITER_F_ROOT_FIRST)) {
	case DEVITER_F_LEAVES_FIRST:
		TAILQ_FOREACH(dv, &alldevs, dv_list) {
			if (!deviter_visits(di, dv))
				continue;
			di->di_curdepth = MAX(di->di_curdepth, dv->dv_depth);
		}
		break;
	case DEVITER_F_ROOT_FIRST:
		TAILQ_FOREACH(dv, &alldevs, dv_list) {
			if (!deviter_visits(di, dv))
				continue;
			di->di_maxdepth = MAX(di->di_maxdepth, dv->dv_depth);
		}
		break;
	default:
		break;
	}

	deviter_reinit(di);
}

static void
deviter_reinit(deviter_t *di)
{
	if ((di->di_flags & DEVITER_F_RW) != 0)
		di->di_prev = TAILQ_LAST(&alldevs, devicelist);
	else
		di->di_prev = TAILQ_FIRST(&alldevs);
}

device_t
deviter_first(deviter_t *di, deviter_flags_t flags)
{
	deviter_init(di, flags);
	return deviter_next(di);
}

static device_t
deviter_next2(deviter_t *di)
{
	device_t dv;

	dv = di->di_prev;

	if (dv == NULL)
		return NULL;

	if ((di->di_flags & DEVITER_F_RW) != 0)
		di->di_prev = TAILQ_PREV(dv, devicelist, dv_list);
	else
		di->di_prev = TAILQ_NEXT(dv, dv_list);

	return dv;
}

static device_t
deviter_next1(deviter_t *di)
{
	device_t dv;

	do {
		dv = deviter_next2(di);
	} while (dv != NULL && !deviter_visits(di, dv));

	return dv;
}

device_t
deviter_next(deviter_t *di)
{
	device_t dv = NULL;

	switch (di->di_flags & (DEVITER_F_LEAVES_FIRST|DEVITER_F_ROOT_FIRST)) {
	case 0:
		return deviter_next1(di);
	case DEVITER_F_LEAVES_FIRST:
		while (di->di_curdepth >= 0) {
			if ((dv = deviter_next1(di)) == NULL) {
				di->di_curdepth--;
				deviter_reinit(di);
			} else if (dv->dv_depth == di->di_curdepth)
				break;
		}
		return dv;
	case DEVITER_F_ROOT_FIRST:
		while (di->di_curdepth <= di->di_maxdepth) {
			if ((dv = deviter_next1(di)) == NULL) {
				di->di_curdepth++;
				deviter_reinit(di);
			} else if (dv->dv_depth == di->di_curdepth)
				break;
		}
		return dv;
	default:
		return NULL;
	}
}

void
deviter_release(deviter_t *di)
{
	bool rw = (di->di_flags & DEVITER_F_RW) != 0;
	int s;

	s = config_alldevs_lock();
	if (rw)
		--alldevs_nwrite;
	else
		--alldevs_nread;
	/* XXX wake a garbage-collection thread */
	config_alldevs_unlock(s);
}

const char *
cfdata_ifattr(const struct cfdata *cf)
{
	return cf->cf_pspec->cfp_iattr;
}

bool
ifattr_match(const char *snull, const char *t)
{
	return (snull == NULL) || strcmp(snull, t) == 0;
}

void
null_childdetached(device_t self, device_t child)
{
	/* do nothing */
}

static void
sysctl_detach_setup(struct sysctllog **clog)
{

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_BOOL, "detachall",
		SYSCTL_DESCR("Detach all devices at shutdown"),
		NULL, 0, &detachall, 0,
		CTL_KERN, CTL_CREATE, CTL_EOL);
}
