/*	$NetBSD: kern_module.c,v 1.106 2015/06/22 16:35:13 matt Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software developed for The NetBSD Foundation
 * by Andrew Doran.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Kernel module support.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_module.c,v 1.106 2015/06/22 16:35:13 matt Exp $");

#define _MODULE_INTERNAL

#ifdef _KERNEL_OPT
#include "opt_ddb.h"
#include "opt_modular.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/kauth.h>
#include <sys/kobj.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/kthread.h>
#include <sys/sysctl.h>
#include <sys/lock.h>

#include <uvm/uvm_extern.h>

struct vm_map *module_map;
const char *module_machine;
char	module_base[MODULE_BASE_SIZE];

struct modlist        module_list = TAILQ_HEAD_INITIALIZER(module_list);
struct modlist        module_builtins = TAILQ_HEAD_INITIALIZER(module_builtins);
static struct modlist module_bootlist = TAILQ_HEAD_INITIALIZER(module_bootlist);

static module_t	*module_active;
static bool	module_verbose_on;
#ifdef MODULAR_DEFAULT_AUTOLOAD
static bool	module_autoload_on = true;
#else
static bool	module_autoload_on = false;
#endif
u_int		module_count;
u_int		module_builtinlist;
u_int		module_autotime = 10;
u_int		module_gen = 1;
static kcondvar_t module_thread_cv;
static kmutex_t module_thread_lock;
static int	module_thread_ticks;
int (*module_load_vfs_vec)(const char *, int, bool, module_t *,
			   prop_dictionary_t *) = (void *)eopnotsupp; 

static kauth_listener_t	module_listener;

/* Ensure that the kernel's link set isn't empty. */
static modinfo_t module_dummy;
__link_set_add_rodata(modules, module_dummy);

static module_t	*module_newmodule(modsrc_t);
static void	module_require_force(module_t *);
static int	module_do_load(const char *, bool, int, prop_dictionary_t,
		    module_t **, modclass_t modclass, bool);
static int	module_do_unload(const char *, bool);
static int	module_do_builtin(const char *, module_t **, prop_dictionary_t);
static int	module_fetch_info(module_t *);
static void	module_thread(void *);

static module_t	*module_lookup(const char *);
static void	module_enqueue(module_t *);

static bool	module_merge_dicts(prop_dictionary_t, const prop_dictionary_t);

static void	sysctl_module_setup(void);
static int	sysctl_module_autotime(SYSCTLFN_PROTO);

#define MODULE_CLASS_MATCH(mi, modclass) \
	((modclass) == MODULE_CLASS_ANY || (modclass) == (mi)->mi_class)

static void
module_incompat(const modinfo_t *mi, int modclass)
{
	module_error("incompatible module class for `%s' (%d != %d)",
	    mi->mi_name, modclass, mi->mi_class);
}

/*
 * module_error:
 *
 *	Utility function: log an error.
 */
void
module_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	printf("WARNING: module error: ");
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
}

/*
 * module_print:
 *
 *	Utility function: log verbose output.
 */
void
module_print(const char *fmt, ...)
{
	va_list ap;

	if (module_verbose_on) {
		va_start(ap, fmt);
		printf("DEBUG: module: ");
		vprintf(fmt, ap);
		printf("\n");
		va_end(ap);
	}
}

static int
module_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	int result;

	result = KAUTH_RESULT_DEFER;

	if (action != KAUTH_SYSTEM_MODULE)
		return result;

	if ((uintptr_t)arg2 != 0)	/* autoload */
		result = KAUTH_RESULT_ALLOW;

	return result;
}

/*
 * Allocate a new module_t
 */
static module_t *
module_newmodule(modsrc_t source)
{
	module_t *mod;

	mod = kmem_zalloc(sizeof(*mod), KM_SLEEP);
	if (mod != NULL) {
		mod->mod_source = source;
		mod->mod_info = NULL;
		mod->mod_flags = 0;
	}
	return mod;
}

/*
 * Require the -f (force) flag to load a module
 */
static void
module_require_force(struct module *mod)
{
	mod->mod_flags |= MODFLG_MUST_FORCE;
}

/*
 * Add modules to the builtin list.  This can done at boottime or
 * at runtime if the module is linked into the kernel with an
 * external linker.  All or none of the input will be handled.
 * Optionally, the modules can be initialized.  If they are not
 * initialized, module_init_class() or module_load() can be used
 * later, but these are not guaranteed to give atomic results.
 */
int
module_builtin_add(modinfo_t *const *mip, size_t nmodinfo, bool init)
{
	struct module **modp = NULL, *mod_iter;
	int rv = 0, i, mipskip;

	if (init) {
		rv = kauth_authorize_system(kauth_cred_get(),
		    KAUTH_SYSTEM_MODULE, 0, (void *)(uintptr_t)MODCTL_LOAD,
		    (void *)(uintptr_t)1, NULL);
		if (rv) {
			return rv;
		}
	}

	for (i = 0, mipskip = 0; i < nmodinfo; i++) {
		if (mip[i] == &module_dummy) {
			KASSERT(nmodinfo > 0);
			nmodinfo--;
		}
	}
	if (nmodinfo == 0)
		return 0;

	modp = kmem_zalloc(sizeof(*modp) * nmodinfo, KM_SLEEP);
	for (i = 0, mipskip = 0; i < nmodinfo; i++) {
		if (mip[i+mipskip] == &module_dummy) {
			mipskip++;
			continue;
		}
		modp[i] = module_newmodule(MODULE_SOURCE_KERNEL);
		modp[i]->mod_info = mip[i+mipskip];
	}
	kernconfig_lock();

	/* do this in three stages for error recovery and atomicity */

	/* first check for presence */
	for (i = 0; i < nmodinfo; i++) {
		TAILQ_FOREACH(mod_iter, &module_builtins, mod_chain) {
			if (strcmp(mod_iter->mod_info->mi_name,
			    modp[i]->mod_info->mi_name) == 0)
				break;
		}
		if (mod_iter) {
			rv = EEXIST;
			goto out;
		}

		if (module_lookup(modp[i]->mod_info->mi_name) != NULL) {
			rv = EEXIST;
			goto out;
		}
	}

	/* then add to list */
	for (i = 0; i < nmodinfo; i++) {
		TAILQ_INSERT_TAIL(&module_builtins, modp[i], mod_chain);
		module_builtinlist++;
	}

	/* finally, init (if required) */
	if (init) {
		for (i = 0; i < nmodinfo; i++) {
			rv = module_do_builtin(modp[i]->mod_info->mi_name,
			    NULL, NULL);
			/* throw in the towel, recovery hard & not worth it */
			if (rv)
				panic("builtin module \"%s\" init failed: %d",
				    modp[i]->mod_info->mi_name, rv);
		}
	}

 out:
	kernconfig_unlock();
	if (rv != 0) {
		for (i = 0; i < nmodinfo; i++) {
			if (modp[i])
				kmem_free(modp[i], sizeof(*modp[i]));
		}
	}
	kmem_free(modp, sizeof(*modp) * nmodinfo);
	return rv;
}

/*
 * Optionally fini and remove builtin module from the kernel.
 * Note: the module will now be unreachable except via mi && builtin_add.
 */
int
module_builtin_remove(modinfo_t *mi, bool fini)
{
	struct module *mod;
	int rv = 0;

	if (fini) {
		rv = kauth_authorize_system(kauth_cred_get(),
		    KAUTH_SYSTEM_MODULE, 0, (void *)(uintptr_t)MODCTL_UNLOAD,
		    NULL, NULL);
		if (rv)
			return rv;

		kernconfig_lock();
		rv = module_do_unload(mi->mi_name, true);
		if (rv) {
			goto out;
		}
	} else {
		kernconfig_lock();
	}
	TAILQ_FOREACH(mod, &module_builtins, mod_chain) {
		if (strcmp(mod->mod_info->mi_name, mi->mi_name) == 0)
			break;
	}
	if (mod) {
		TAILQ_REMOVE(&module_builtins, mod, mod_chain);
		module_builtinlist--;
	} else {
		KASSERT(fini == false);
		rv = ENOENT;
	}

 out:
	kernconfig_unlock();
	return rv;
}

/*
 * module_init:
 *
 *	Initialize the module subsystem.
 */
void
module_init(void)
{
	__link_set_decl(modules, modinfo_t);
	extern struct vm_map *module_map;
	modinfo_t *const *mip;
	int rv;

	if (module_map == NULL) {
		module_map = kernel_map;
	}
	cv_init(&module_thread_cv, "mod_unld");
	mutex_init(&module_thread_lock, MUTEX_DEFAULT, IPL_NONE);

#ifdef MODULAR	/* XXX */
	module_init_md();
#endif

	if (!module_machine)
		module_machine = machine;
#if __NetBSD_Version__ / 1000000 % 100 == 99	/* -current */
	snprintf(module_base, sizeof(module_base), "/stand/%s/%s/modules",
	    module_machine, osrelease);
#else						/* release */
	snprintf(module_base, sizeof(module_base), "/stand/%s/%d.%d/modules",
	    module_machine, __NetBSD_Version__ / 100000000,
	    __NetBSD_Version__ / 1000000 % 100);
#endif

	module_listener = kauth_listen_scope(KAUTH_SCOPE_SYSTEM,
	    module_listener_cb, NULL);

	__link_set_foreach(mip, modules) {
		if ((rv = module_builtin_add(mip, 1, false)) != 0)
			module_error("builtin %s failed: %d\n",
			    (*mip)->mi_name, rv);
	}

	sysctl_module_setup();
}

/*
 * module_start_unload_thread:
 *
 *	Start the auto unload kthread.
 */
void
module_start_unload_thread(void)
{
	int error;

	error = kthread_create(PRI_VM, KTHREAD_MPSAFE, NULL, module_thread,
	    NULL, NULL, "modunload");
	if (error != 0)
		panic("module_init: %d", error);
}

/*
 * module_builtin_require_force
 *
 * Require MODCTL_MUST_FORCE to load any built-in modules that have 
 * not yet been initialized
 */
void
module_builtin_require_force(void)
{
	module_t *mod;

	kernconfig_lock();
	TAILQ_FOREACH(mod, &module_builtins, mod_chain) {
		module_require_force(mod);
	}
	kernconfig_unlock();
}

static struct sysctllog *module_sysctllog;

static int
sysctl_module_autotime(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int t, error;

	t = *(int *)rnode->sysctl_data;

	node = *rnode;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (t < 0)
		return (EINVAL);

	*(int *)rnode->sysctl_data = t;
	return (0);
}

static void
sysctl_module_setup(void)
{
	const struct sysctlnode *node = NULL;

	sysctl_createv(&module_sysctllog, 0, NULL, &node,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "module",
		SYSCTL_DESCR("Module options"),
		NULL, 0, NULL, 0,
		CTL_KERN, CTL_CREATE, CTL_EOL);

	if (node == NULL)
		return;

	sysctl_createv(&module_sysctllog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_BOOL, "autoload",
		SYSCTL_DESCR("Enable automatic load of modules"),
		NULL, 0, &module_autoload_on, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(&module_sysctllog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_BOOL, "verbose",
		SYSCTL_DESCR("Enable verbose output"),
		NULL, 0, &module_verbose_on, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(&module_sysctllog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READONLY,
		CTLTYPE_STRING, "path",
		SYSCTL_DESCR("Default module load path"),
		NULL, 0, module_base, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(&module_sysctllog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "autotime",
		SYSCTL_DESCR("Auto-unload delay"),
		sysctl_module_autotime, 0, &module_autotime, 0,
		CTL_CREATE, CTL_EOL);
}

/*
 * module_init_class:
 *
 *	Initialize all built-in and pre-loaded modules of the
 *	specified class.
 */
void
module_init_class(modclass_t modclass)
{
	TAILQ_HEAD(, module) bi_fail = TAILQ_HEAD_INITIALIZER(bi_fail);
	module_t *mod;
	modinfo_t *mi;

	kernconfig_lock();
	/*
	 * Builtins first.  These will not depend on pre-loaded modules
	 * (because the kernel would not link).
	 */
	do {
		TAILQ_FOREACH(mod, &module_builtins, mod_chain) {
			mi = mod->mod_info;
			if (!MODULE_CLASS_MATCH(mi, modclass))
				continue;
			/*
			 * If initializing a builtin module fails, don't try
			 * to load it again.  But keep it around and queue it
			 * on the builtins list after we're done with module
			 * init.  Don't set it to MODFLG_MUST_FORCE in case a
			 * future attempt to initialize can be successful.
			 * (If the module has previously been set to
			 * MODFLG_MUST_FORCE, don't try to override that!)
			 */
			if ((mod->mod_flags & MODFLG_MUST_FORCE) ||
			    module_do_builtin(mi->mi_name, NULL, NULL) != 0) {
				TAILQ_REMOVE(&module_builtins, mod, mod_chain);
				TAILQ_INSERT_TAIL(&bi_fail, mod, mod_chain);
			}
			break;
		}
	} while (mod != NULL);

	/*
	 * Now preloaded modules.  These will be pulled off the
	 * list as we call module_do_load();
	 */
	do {
		TAILQ_FOREACH(mod, &module_bootlist, mod_chain) {
			mi = mod->mod_info;
			if (!MODULE_CLASS_MATCH(mi, modclass))
				continue;
			module_do_load(mi->mi_name, false, 0, NULL, NULL,
			    modclass, false);
			break;
		}
	} while (mod != NULL);

	/* return failed builtin modules to builtin list */
	while ((mod = TAILQ_FIRST(&bi_fail)) != NULL) {
		TAILQ_REMOVE(&bi_fail, mod, mod_chain);
		TAILQ_INSERT_TAIL(&module_builtins, mod, mod_chain);
	}

	kernconfig_unlock();
}

/*
 * module_compatible:
 *
 *	Return true if the two supplied kernel versions are said to
 *	have the same binary interface for kernel code.  The entire
 *	version is signficant for the development tree (-current),
 *	major and minor versions are significant for official
 *	releases of the system.
 */
bool
module_compatible(int v1, int v2)
{

#if __NetBSD_Version__ / 1000000 % 100 == 99	/* -current */
	return v1 == v2;
#else						/* release */
	return abs(v1 - v2) < 10000;
#endif
}

/*
 * module_load:
 *
 *	Load a single module from the file system.
 */
int
module_load(const char *filename, int flags, prop_dictionary_t props,
	    modclass_t modclass)
{
	int error;

	/* Authorize. */
	error = kauth_authorize_system(kauth_cred_get(), KAUTH_SYSTEM_MODULE,
	    0, (void *)(uintptr_t)MODCTL_LOAD, NULL, NULL);
	if (error != 0) {
		return error;
	}

	kernconfig_lock();
	error = module_do_load(filename, false, flags, props, NULL, modclass,
	    false);
	kernconfig_unlock();

	return error;
}

/*
 * module_autoload:
 *
 *	Load a single module from the file system, system initiated.
 */
int
module_autoload(const char *filename, modclass_t modclass)
{
	int error;

	kernconfig_lock();

	/* Nothing if the user has disabled it. */
	if (!module_autoload_on) {
		kernconfig_unlock();
		return EPERM;
	}

        /* Disallow path separators and magic symlinks. */
        if (strchr(filename, '/') != NULL || strchr(filename, '@') != NULL ||
            strchr(filename, '.') != NULL) {
		kernconfig_unlock();
        	return EPERM;
	}

	/* Authorize. */
	error = kauth_authorize_system(kauth_cred_get(), KAUTH_SYSTEM_MODULE,
	    0, (void *)(uintptr_t)MODCTL_LOAD, (void *)(uintptr_t)1, NULL);

	if (error == 0)
		error = module_do_load(filename, false, 0, NULL, NULL, modclass,
		    true);

	kernconfig_unlock();
	return error;
}

/*
 * module_unload:
 *
 *	Find and unload a module by name.
 */
int
module_unload(const char *name)
{
	int error;

	/* Authorize. */
	error = kauth_authorize_system(kauth_cred_get(), KAUTH_SYSTEM_MODULE,
	    0, (void *)(uintptr_t)MODCTL_UNLOAD, NULL, NULL);
	if (error != 0) {
		return error;
	}

	kernconfig_lock();
	error = module_do_unload(name, true);
	kernconfig_unlock();

	return error;
}

/*
 * module_lookup:
 *
 *	Look up a module by name.
 */
module_t *
module_lookup(const char *name)
{
	module_t *mod;

	KASSERT(kernconfig_is_held());

	TAILQ_FOREACH(mod, &module_list, mod_chain) {
		if (strcmp(mod->mod_info->mi_name, name) == 0) {
			break;
		}
	}

	return mod;
}

/*
 * module_hold:
 *
 *	Add a single reference to a module.  It's the caller's
 *	responsibility to ensure that the reference is dropped
 *	later.
 */
int
module_hold(const char *name)
{
	module_t *mod;

	kernconfig_lock();
	mod = module_lookup(name);
	if (mod == NULL) {
		kernconfig_unlock();
		return ENOENT;
	}
	mod->mod_refcnt++;
	kernconfig_unlock();

	return 0;
}

/*
 * module_rele:
 *
 *	Release a reference acquired with module_hold().
 */
void
module_rele(const char *name)
{
	module_t *mod;

	kernconfig_lock();
	mod = module_lookup(name);
	if (mod == NULL) {
		kernconfig_unlock();
		panic("module_rele: gone");
	}
	mod->mod_refcnt--;
	kernconfig_unlock();
}

/*
 * module_enqueue:
 *
 *	Put a module onto the global list and update counters.
 */
void
module_enqueue(module_t *mod)
{
	int i;

	KASSERT(kernconfig_is_held());

	/*
	 * If there are requisite modules, put at the head of the queue.
	 * This is so that autounload can unload requisite modules with
	 * only one pass through the queue.
	 */
	if (mod->mod_nrequired) {
		TAILQ_INSERT_HEAD(&module_list, mod, mod_chain);

		/* Add references to the requisite modules. */
		for (i = 0; i < mod->mod_nrequired; i++) {
			KASSERT(mod->mod_required[i] != NULL);
			mod->mod_required[i]->mod_refcnt++;
		}
	} else {
		TAILQ_INSERT_TAIL(&module_list, mod, mod_chain);
	}
	module_count++;
	module_gen++;
}

/*
 * module_do_builtin:
 *
 *	Initialize a module from the list of modules that are
 *	already linked into the kernel.
 */
static int
module_do_builtin(const char *name, module_t **modp, prop_dictionary_t props)
{
	const char *p, *s;
	char buf[MAXMODNAME];
	modinfo_t *mi = NULL;
	module_t *mod, *mod2, *mod_loaded, *prev_active;
	size_t len;
	int error;

	KASSERT(kernconfig_is_held());

	/*
	 * Search the list to see if we have a module by this name.
	 */
	TAILQ_FOREACH(mod, &module_builtins, mod_chain) {
		if (strcmp(mod->mod_info->mi_name, name) == 0) {
			mi = mod->mod_info;
			break;
		}
	}

	/*
	 * Check to see if already loaded.  This might happen if we
	 * were already loaded as a dependency.
	 */
	if ((mod_loaded = module_lookup(name)) != NULL) {
		KASSERT(mod == NULL);
		if (modp)
			*modp = mod_loaded;
		return 0;
	}

	/* Note! This is from TAILQ, not immediate above */
	if (mi == NULL) {
		/*
		 * XXX: We'd like to panic here, but currently in some
		 * cases (such as nfsserver + nfs), the dependee can be
		 * succesfully linked without the dependencies.
		 */
		module_error("can't find builtin dependency `%s'", name);
		return ENOENT;
	}

	/*
	 * Initialize pre-requisites.
	 */
	if (mi->mi_required != NULL) {
		for (s = mi->mi_required; *s != '\0'; s = p) {
			if (*s == ',')
				s++;
			p = s;
			while (*p != '\0' && *p != ',')
				p++;
			len = min(p - s + 1, sizeof(buf));
			strlcpy(buf, s, len);
			if (buf[0] == '\0')
				break;
			if (mod->mod_nrequired == MAXMODDEPS - 1) {
				module_error("too many required modules "
				    "%d >= %d", mod->mod_nrequired,
				    MAXMODDEPS - 1);
				return EINVAL;
			}
			error = module_do_builtin(buf, &mod2, NULL);
			if (error != 0) {
				return error;
			}
			mod->mod_required[mod->mod_nrequired++] = mod2;
		}
	}

	/*
	 * Try to initialize the module.
	 */
	prev_active = module_active;
	module_active = mod;
	error = (*mi->mi_modcmd)(MODULE_CMD_INIT, props);
	module_active = prev_active;
	if (error != 0) {
		module_error("builtin module `%s' "
		    "failed to init, error %d", mi->mi_name, error);
		return error;
	}

	/* load always succeeds after this point */

	TAILQ_REMOVE(&module_builtins, mod, mod_chain);
	module_builtinlist--;
	if (modp != NULL) {
		*modp = mod;
	}
	module_enqueue(mod);
	return 0;
}

/*
 * module_do_load:
 *
 *	Helper routine: load a module from the file system, or one
 *	pushed by the boot loader.
 */
static int
module_do_load(const char *name, bool isdep, int flags,
	       prop_dictionary_t props, module_t **modp, modclass_t modclass,
	       bool autoload)
{
#define MODULE_MAX_DEPTH 6

	TAILQ_HEAD(pending_t, module);
	static int depth = 0;
	static struct pending_t *pending_lists[MODULE_MAX_DEPTH];
	struct pending_t *pending;
	struct pending_t new_pending = TAILQ_HEAD_INITIALIZER(new_pending);
	modinfo_t *mi;
	module_t *mod, *mod2, *prev_active;
	prop_dictionary_t filedict;
	char buf[MAXMODNAME];
	const char *s, *p;
	int error;
	size_t len;

	KASSERT(kernconfig_is_held());

	filedict = NULL;
	error = 0;

	/*
	 * Avoid recursing too far.
	 */
	if (++depth > MODULE_MAX_DEPTH) {
		module_error("recursion too deep for `%s' %d > %d", name,
		    depth, MODULE_MAX_DEPTH);
		depth--;
		return EMLINK;
	}

	/*
	 * Set up the pending list for this depth.  If this is a
	 * recursive entry, then use same list as for outer call,
	 * else use the locally allocated list.  In either case,
	 * remember which one we're using.
	 */
	if (isdep) {
		KASSERT(depth > 1);
		pending = pending_lists[depth - 2];
	} else
		pending = &new_pending;
	pending_lists[depth - 1] = pending;

	/*
	 * Search the list of disabled builtins first.
	 */
	TAILQ_FOREACH(mod, &module_builtins, mod_chain) {
		if (strcmp(mod->mod_info->mi_name, name) == 0) {
			break;
		}
	}
	if (mod) {
		if ((mod->mod_flags & MODFLG_MUST_FORCE) &&
		    (flags & MODCTL_LOAD_FORCE) == 0) {
			if (!autoload) {
				module_error("use -f to reinstate "
				    "builtin module `%s'", name);
			}
			depth--;
			return EPERM;
		} else {
			error = module_do_builtin(name, modp, props);
			depth--;
			return error;
		}
	}

	/*
	 * Load the module and link.  Before going to the file system,
	 * scan the list of modules loaded by the boot loader.
	 */
	TAILQ_FOREACH(mod, &module_bootlist, mod_chain) {
		if (strcmp(mod->mod_info->mi_name, name) == 0) {
			TAILQ_REMOVE(&module_bootlist, mod, mod_chain);
			break;
		}
	}
	if (mod != NULL) {
		TAILQ_INSERT_TAIL(pending, mod, mod_chain);
	} else {
		/*
		 * If a requisite module, check to see if it is
		 * already present.
		 */
		if (isdep) {
			mod = module_lookup(name);
			if (mod != NULL) {
				if (modp != NULL) {
					*modp = mod;
				}
				depth--;
				return 0;
			}
		}				
		mod = module_newmodule(MODULE_SOURCE_FILESYS);
		if (mod == NULL) {
			module_error("out of memory for `%s'", name);
			depth--;
			return ENOMEM;
		}

		error = module_load_vfs_vec(name, flags, autoload, mod,
					    &filedict);
		if (error != 0) {
#ifdef DEBUG
			/*
			 * The exec class of modules contains a list of
			 * modules that is the union of all the modules
			 * available for each architecture, so we don't
			 * print an error if they are missing.
			 */
			if ((modclass != MODULE_CLASS_EXEC || error != ENOENT)
			    && root_device != NULL)
				module_error("vfs load failed for `%s', "
				    "error %d", name, error);
#endif
			kmem_free(mod, sizeof(*mod));
			depth--;
			return error;
		}
		TAILQ_INSERT_TAIL(pending, mod, mod_chain);

		error = module_fetch_info(mod);
		if (error != 0) {
			module_error("cannot fetch info for `%s', error %d",
			    name, error);
			goto fail;
		}
	}

	/*
	 * Check compatibility.
	 */
	mi = mod->mod_info;
	if (strlen(mi->mi_name) >= MAXMODNAME) {
		error = EINVAL;
		module_error("module name `%s' longer than %d", mi->mi_name,
		    MAXMODNAME);
		goto fail;
	}
	if (!module_compatible(mi->mi_version, __NetBSD_Version__)) {
		module_error("module `%s' built for `%d', system `%d'",
		    mi->mi_name, mi->mi_version, __NetBSD_Version__);
		if ((flags & MODCTL_LOAD_FORCE) != 0) {
			module_error("forced load, system may be unstable");
		} else {
			error = EPROGMISMATCH;
			goto fail;
		}
	}

	/*
	 * If a specific kind of module was requested, ensure that we have
	 * a match.
	 */
	if (!MODULE_CLASS_MATCH(mi, modclass)) {
		module_incompat(mi, modclass);
		error = ENOENT;
		goto fail;
	}

	/*
	 * If loading a dependency, `name' is a plain module name.
	 * The name must match.
	 */
	if (isdep && strcmp(mi->mi_name, name) != 0) {
		module_error("dependency name mismatch (`%s' != `%s')",
		    name, mi->mi_name);
		error = ENOENT;
		goto fail;
	}

	/*
	 * Check to see if the module is already loaded.  If so, we may
	 * have been recursively called to handle a dependency, so be sure
	 * to set modp.
	 */
	if ((mod2 = module_lookup(mi->mi_name)) != NULL) {
		if (modp != NULL)
			*modp = mod2;
		module_print("module `%s' already loaded", mi->mi_name);
		error = EEXIST;
		goto fail;
	}

	/*
	 * Block circular dependencies.
	 */
	TAILQ_FOREACH(mod2, pending, mod_chain) {
		if (mod == mod2) {
			continue;
		}
		if (strcmp(mod2->mod_info->mi_name, mi->mi_name) == 0) {
		    	error = EDEADLK;
			module_error("circular dependency detected for `%s'",
			    mi->mi_name);
		    	goto fail;
		}
	}

	/*
	 * Now try to load any requisite modules.
	 */
	if (mi->mi_required != NULL) {
		for (s = mi->mi_required; *s != '\0'; s = p) {
			if (*s == ',')
				s++;
			p = s;
			while (*p != '\0' && *p != ',')
				p++;
			len = p - s + 1;
			if (len >= MAXMODNAME) {
				error = EINVAL;
				module_error("required module name `%s' "
				    "longer than %d", mi->mi_required,
				    MAXMODNAME);
				goto fail;
			}
			strlcpy(buf, s, len);
			if (buf[0] == '\0')
				break;
			if (mod->mod_nrequired == MAXMODDEPS - 1) {
				error = EINVAL;
				module_error("too many required modules "
				    "%d >= %d", mod->mod_nrequired,
				    MAXMODDEPS - 1);
				goto fail;
			}
			if (strcmp(buf, mi->mi_name) == 0) {
				error = EDEADLK;
				module_error("self-dependency detected for "
				   "`%s'", mi->mi_name);
				goto fail;
			}
			error = module_do_load(buf, true, flags, NULL,
			    &mod2, MODULE_CLASS_ANY, true);
			if (error != 0) {
				module_error("recursive load failed for `%s' "
				    "(`%s' required), error %d", mi->mi_name,
				    buf, error);
				goto fail;
			}
			mod->mod_required[mod->mod_nrequired++] = mod2;
		}
	}

	/*
	 * We loaded all needed modules successfully: perform global
	 * relocations and initialize.
	 */
	error = kobj_affix(mod->mod_kobj, mi->mi_name);
	if (error != 0) {
		/* Cannot touch 'mi' as the module is now gone. */
		module_error("unable to affix module `%s', error %d", name,
		    error);
		goto fail2;
	}

	if (filedict) {
		if (!module_merge_dicts(filedict, props)) {
			module_error("module properties failed for %s", name);
			error = EINVAL;
			goto fail;
		}
	}
	prev_active = module_active;
	module_active = mod;
	error = (*mi->mi_modcmd)(MODULE_CMD_INIT, filedict ? filedict : props);
	module_active = prev_active;
	if (filedict) {
		prop_object_release(filedict);
		filedict = NULL;
	}
	if (error != 0) {
		module_error("modcmd function failed for `%s', error %d",
		    mi->mi_name, error);
		goto fail;
	}

	/*
	 * Good, the module loaded successfully.  Put it onto the
	 * list and add references to its requisite modules.
	 */
	TAILQ_REMOVE(pending, mod, mod_chain);
	module_enqueue(mod);
	if (modp != NULL) {
		*modp = mod;
	}
	if (autoload && module_autotime > 0) {
		/*
		 * Arrange to try unloading the module after
		 * a short delay unless auto-unload is disabled.
		 */
		mod->mod_autotime = time_second + module_autotime;
		mod->mod_flags |= MODFLG_AUTO_LOADED;
		module_thread_kick();
	}
	depth--;
	return 0;

 fail:
	kobj_unload(mod->mod_kobj);
 fail2:
	if (filedict != NULL) {
		prop_object_release(filedict);
		filedict = NULL;
	}
	TAILQ_REMOVE(pending, mod, mod_chain);
	kmem_free(mod, sizeof(*mod));
	depth--;
	return error;
}

/*
 * module_do_unload:
 *
 *	Helper routine: do the dirty work of unloading a module.
 */
static int
module_do_unload(const char *name, bool load_requires_force)
{
	module_t *mod, *prev_active;
	int error;
	u_int i;

	KASSERT(kernconfig_is_held());
	KASSERT(name != NULL);

	mod = module_lookup(name);
	if (mod == NULL) {
		module_error("module `%s' not found", name);
		return ENOENT;
	}
	if (mod->mod_refcnt != 0) {
		module_print("module `%s' busy", name);
		return EBUSY;
	}

	/*
	 * Builtin secmodels are there to stay.
	 */
	if (mod->mod_source == MODULE_SOURCE_KERNEL &&
	    mod->mod_info->mi_class == MODULE_CLASS_SECMODEL) {
		return EPERM;
	}

	prev_active = module_active;
	module_active = mod;
	error = (*mod->mod_info->mi_modcmd)(MODULE_CMD_FINI, NULL);
	module_active = prev_active;
	if (error != 0) {
		module_print("cannot unload module `%s' error=%d", name,
		    error);
		return error;
	}
	module_count--;
	TAILQ_REMOVE(&module_list, mod, mod_chain);
	for (i = 0; i < mod->mod_nrequired; i++) {
		mod->mod_required[i]->mod_refcnt--;
	}
	module_print("unloaded module `%s'", name);
	if (mod->mod_kobj != NULL) {
		kobj_unload(mod->mod_kobj);
	}
	if (mod->mod_source == MODULE_SOURCE_KERNEL) {
		mod->mod_nrequired = 0; /* will be re-parsed */
		if (load_requires_force)
			module_require_force(mod);
		TAILQ_INSERT_TAIL(&module_builtins, mod, mod_chain);
		module_builtinlist++;
	} else {
		kmem_free(mod, sizeof(*mod));
	}
	module_gen++;

	return 0;
}

/*
 * module_prime:
 *
 *	Push a module loaded by the bootloader onto our internal
 *	list.
 */
int
module_prime(const char *name, void *base, size_t size)
{
	module_t *mod;
	int error;

	mod = module_newmodule(MODULE_SOURCE_BOOT);
	if (mod == NULL) {
		return ENOMEM;
	}

	error = kobj_load_mem(&mod->mod_kobj, name, base, size);
	if (error != 0) {
		kmem_free(mod, sizeof(*mod));
		module_error("unable to load `%s' pushed by boot loader, "
		    "error %d", name, error);
		return error;
	}
	error = module_fetch_info(mod);
	if (error != 0) {
		kobj_unload(mod->mod_kobj);
		kmem_free(mod, sizeof(*mod));
		module_error("unable to load `%s' pushed by boot loader, "
		    "error %d", name, error);
		return error;
	}

	TAILQ_INSERT_TAIL(&module_bootlist, mod, mod_chain);

	return 0;
}

/*
 * module_fetch_into:
 *
 *	Fetch modinfo record from a loaded module.
 */
static int
module_fetch_info(module_t *mod)
{
	int error;
	void *addr;
	size_t size;

	/*
	 * Find module info record and check compatibility.
	 */
	error = kobj_find_section(mod->mod_kobj, "link_set_modules",
	    &addr, &size);
	if (error != 0) {
		module_error("`link_set_modules' section not present, "
		    "error %d", error);
		return error;
	}
	if (size != sizeof(modinfo_t **)) {
		module_error("`link_set_modules' section wrong size %zu != %zu",
		    size, sizeof(modinfo_t **));
		return ENOEXEC;
	}
	mod->mod_info = *(modinfo_t **)addr;

	return 0;
}

/*
 * module_find_section:
 *
 *	Allows a module that is being initialized to look up a section
 *	within its ELF object.
 */
int
module_find_section(const char *name, void **addr, size_t *size)
{

	KASSERT(kernconfig_is_held());
	KASSERT(module_active != NULL);

	return kobj_find_section(module_active->mod_kobj, name, addr, size);
}

/*
 * module_thread:
 *
 *	Automatically unload modules.  We try once to unload autoloaded
 *	modules after module_autotime seconds.  If the system is under
 *	severe memory pressure, we'll try unloading all modules, else if
 *	module_autotime is zero, we don't try to unload, even if the
 *	module was previously scheduled for unload.
 */
static void
module_thread(void *cookie)
{
	module_t *mod, *next;
	modinfo_t *mi;
	int error;

	for (;;) {
		kernconfig_lock();
		for (mod = TAILQ_FIRST(&module_list); mod != NULL; mod = next) {
			next = TAILQ_NEXT(mod, mod_chain);

			/* skip built-in modules */
			if (mod->mod_source == MODULE_SOURCE_KERNEL)
				continue;
			/* skip modules that weren't auto-loaded */
			if ((mod->mod_flags & MODFLG_AUTO_LOADED) == 0)
				continue;

			if (uvmexp.free < uvmexp.freemin) {
				module_thread_ticks = hz;
			} else if (module_autotime == 0 ||
				   mod->mod_autotime == 0) {
				continue;
			} else if (time_second < mod->mod_autotime) {
				module_thread_ticks = hz;
			    	continue;
			} else {
				mod->mod_autotime = 0;
			}

			/*
			 * If this module wants to avoid autounload then
			 * skip it.  Some modules can ping-pong in and out
			 * because their use is transient but often. 
			 * Example: exec_script.
			 */
			mi = mod->mod_info;
			error = (*mi->mi_modcmd)(MODULE_CMD_AUTOUNLOAD, NULL);
			if (error == 0 || error == ENOTTY) {
				(void)module_do_unload(mi->mi_name, false);
			}
		}
		kernconfig_unlock();

		mutex_enter(&module_thread_lock);
		(void)cv_timedwait(&module_thread_cv, &module_thread_lock,
		    module_thread_ticks);
		module_thread_ticks = 0;
		mutex_exit(&module_thread_lock);
	}
}

/*
 * module_thread:
 *
 *	Kick the module thread into action, perhaps because the
 *	system is low on memory.
 */
void
module_thread_kick(void)
{

	mutex_enter(&module_thread_lock);
	module_thread_ticks = hz;
	cv_broadcast(&module_thread_cv);
	mutex_exit(&module_thread_lock);
}

#ifdef DDB
/*
 * module_whatis:
 *
 *	Helper routine for DDB.
 */
void
module_whatis(uintptr_t addr, void (*pr)(const char *, ...))
{
	module_t *mod;
	size_t msize;
	vaddr_t maddr;

	TAILQ_FOREACH(mod, &module_list, mod_chain) {
		if (mod->mod_kobj == NULL) {
			continue;
		}
		if (kobj_stat(mod->mod_kobj, &maddr, &msize) != 0)
			continue;
		if (addr < maddr || addr >= maddr + msize) {
			continue;
		}
		(*pr)("%p is %p+%zu, in kernel module `%s'\n",
		    (void *)addr, (void *)maddr,
		    (size_t)(addr - maddr), mod->mod_info->mi_name);
	}
}

/*
 * module_print_list:
 *
 *	Helper routine for DDB.
 */
void
module_print_list(void (*pr)(const char *, ...))
{
	const char *src;
	module_t *mod;
	size_t msize;
	vaddr_t maddr;

	(*pr)("%16s %16s %8s %8s\n", "NAME", "TEXT/DATA", "SIZE", "SOURCE");

	TAILQ_FOREACH(mod, &module_list, mod_chain) {
		switch (mod->mod_source) {
		case MODULE_SOURCE_KERNEL:
			src = "builtin";
			break;
		case MODULE_SOURCE_FILESYS:
			src = "filesys";
			break;
		case MODULE_SOURCE_BOOT:
			src = "boot";
			break;
		default:
			src = "unknown";
			break;
		}
		if (mod->mod_kobj == NULL) {
			maddr = 0;
			msize = 0;
		} else if (kobj_stat(mod->mod_kobj, &maddr, &msize) != 0)
			continue;
		(*pr)("%16s %16lx %8ld %8s\n", mod->mod_info->mi_name,
		    (long)maddr, (long)msize, src);
	}
}
#endif	/* DDB */

static bool
module_merge_dicts(prop_dictionary_t existing_dict,
		   const prop_dictionary_t new_dict)
{
	prop_dictionary_keysym_t props_keysym;
	prop_object_iterator_t props_iter;
	prop_object_t props_obj;
	const char *props_key;
	bool error;

	if (new_dict == NULL) {			/* nothing to merge */
		return true;
	}

	error = false;
	props_iter = prop_dictionary_iterator(new_dict);
	if (props_iter == NULL) {
		return false;
	}

	while ((props_obj = prop_object_iterator_next(props_iter)) != NULL) {
		props_keysym = (prop_dictionary_keysym_t)props_obj;
		props_key = prop_dictionary_keysym_cstring_nocopy(props_keysym);
		props_obj = prop_dictionary_get_keysym(new_dict, props_keysym);
		if ((props_obj == NULL) || !prop_dictionary_set(existing_dict,
		    props_key, props_obj)) {
			error = true;
			goto out;
		}
	}
	error = false;

out:
	prop_object_iterator_release(props_iter);

	return !error;
}
