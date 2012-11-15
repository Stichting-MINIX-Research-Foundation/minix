/*	$NetBSD: nsdispatch.c,v 1.37 2012/03/13 21:13:42 christos Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn; and by Jason R. Thorpe.
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

/*-
 * Copyright (c) 2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * Jacques A. Vidrine, Safeport Network Services, and Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: nsdispatch.c,v 1.37 2012/03/13 21:13:42 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/queue.h>

#include <assert.h>
#ifdef __ELF__
#include <dlfcn.h>
#endif /* __ELF__ */
#include <err.h>
#include <fcntl.h>
#define _NS_PRIVATE
#include <nsswitch.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "reentrant.h"

extern	FILE 	*_nsyyin;
extern	int	 _nsyyparse(void);


#ifdef __weak_alias
__weak_alias(nsdispatch,_nsdispatch)
#endif


/*
 * default sourcelist: `files'
 */
const ns_src __nsdefaultsrc[] = {
	{ NSSRC_FILES,	NS_SUCCESS },
	{ 0, 0 },
};

const ns_src __nsdefaultcompat[] = {
	{ NSSRC_COMPAT,	NS_SUCCESS },
	{ 0, 0 }
};

const ns_src __nsdefaultcompat_forceall[] = {
	{ NSSRC_COMPAT,	NS_SUCCESS | NS_FORCEALL },
	{ 0, 0 }
};

const ns_src __nsdefaultfiles[] = {
	{ NSSRC_FILES,	NS_SUCCESS },
	{ 0, 0 },
};

const ns_src __nsdefaultfiles_forceall[] = {
	{ NSSRC_FILES,	NS_SUCCESS | NS_FORCEALL },
	{ 0, 0 },
};

const ns_src __nsdefaultnis[] = {
	{ NSSRC_NIS,	NS_SUCCESS },
	{ 0, 0 }
};

const ns_src __nsdefaultnis_forceall[] = {
	{ NSSRC_NIS,	NS_SUCCESS | NS_FORCEALL },
	{ 0, 0 }
};


/* Database, source mappings. */
static	u_int			 _nsmapsize;
static	ns_dbt			*_nsmap;

/* Nsswitch modules. */
static	u_int			 _nsmodsize;
static	ns_mod			*_nsmod;

/* Placeholder for built-in modules' dlopen() handles. */
static	void			*_nsbuiltin = &_nsbuiltin;

#ifdef _REENTRANT
/*
 * Global nsswitch data structures are mostly read-only, but we update them
 * when we read or re-read nsswitch.conf.
 */
static 	rwlock_t		_nslock = RWLOCK_INITIALIZER;

/*
 * List of threads currently in nsdispatch().  We use this to detect
 * recursive calls and avoid reloading configuration in such cases,
 * which could cause deadlock.
 */
struct _ns_drec {
	LIST_ENTRY(_ns_drec)	list;
	thr_t			thr;
};
static LIST_HEAD(, _ns_drec) _ns_drec = LIST_HEAD_INITIALIZER(&_ns_drec);
static mutex_t _ns_drec_lock = MUTEX_INITIALIZER;
#endif /* _REENTRANT */


/*
 * Runtime determination of whether we are dynamically linked or not.
 */
#ifndef __ELF__
#define	is_dynamic()		(0)	/* don't bother - switch to ELF! */
#else
__weakref_visible int rtld_DYNAMIC __weak_reference(_DYNAMIC);
#define	is_dynamic()		(&rtld_DYNAMIC != NULL)
#endif


/*
 * size of dynamic array chunk for _nsmap and _nsmap[x].srclist (and other
 * growing arrays).
 */
#define NSELEMSPERCHUNK		8

/*
 * Dynamically growable arrays are used for lists of databases, sources,
 * and modules.  The following "vector" API is used to isolate the
 * common operations.
 */
typedef void	(*_nsvect_free_elem)(void *);

static void *
_nsvect_append(const void *elem, void *vec, u_int *count, size_t esize)
{
	void	*p;

	if ((*count % NSELEMSPERCHUNK) == 0) {
		p = realloc(vec, (*count + NSELEMSPERCHUNK) * esize);
		if (p == NULL)
			return (NULL);
		vec = p;
	}
	memmove((void *)(((uintptr_t)vec) + (*count * esize)), elem, esize);
	(*count)++;
	return (vec);
}

static void *
_nsvect_elem(u_int i, void *vec, u_int count, size_t esize)
{

	if (i < count)
		return ((void *)((uintptr_t)vec + (i * esize)));
	else
		return (NULL);
}

static void
_nsvect_free(void *vec, u_int *count, size_t esize, _nsvect_free_elem free_elem)
{
	void	*elem;
	u_int	 i;

	for (i = 0; i < *count; i++) {
		elem = _nsvect_elem(i, vec, *count, esize);
		if (elem != NULL)
			(*free_elem)(elem);
	}
	if (vec != NULL)
		free(vec);
	*count = 0;
}
#define	_NSVECT_FREE(v, c, s, f)					\
do {									\
	_nsvect_free((v), (c), (s), (f));				\
	(v) = NULL;							\
} while (/*CONSTCOND*/0)

static int
_nsdbtcmp(const void *a, const void *b)
{

	return (strcasecmp(((const ns_dbt *)a)->name,
	    ((const ns_dbt *)b)->name));
}

static int
_nsmodcmp(const void *a, const void *b)
{

	return (strcasecmp(((const ns_mod *)a)->name,
	    ((const ns_mod *)b)->name));
}

static int
_nsmtabcmp(const void *a, const void *b)
{
	int	cmp;

	cmp = strcmp(((const ns_mtab *)a)->name,
	    ((const ns_mtab *)b)->name);
	if (cmp)
		return (cmp);

	return (strcasecmp(((const ns_mtab *)a)->database,
	    ((const ns_mtab *)b)->database));
}

static void
_nsmodfree(ns_mod *mod)
{

	free(__UNCONST(mod->name));
	if (mod->handle == NULL)
		return;
	if (mod->unregister != NULL)
		(*mod->unregister)(mod->mtab, mod->mtabsize);
#ifdef __ELF__
	if (mod->handle != _nsbuiltin)
		(void) dlclose(mod->handle);
#endif /* __ELF__ */
}

/*
 * Load a built-in or dyanamically linked module.  If the `reg_fn'
 * argument is non-NULL, assume a built-in module and use `reg_fn'
 * to register it.  Otherwise, search for a dynamic nsswitch module.
 */
static int
_nsloadmod(const char *source, nss_module_register_fn reg_fn)
{
#ifdef __ELF__
	char	buf[PATH_MAX];
#endif
	ns_mod	mod, *new;

	memset(&mod, 0, sizeof(mod));
	mod.name = strdup(source);
	if (mod.name == NULL)
		return (-1);

	if (reg_fn != NULL) {
		/*
		 * The placeholder is required, as a NULL handle
		 * represents an invalid module.
		 */
		mod.handle = _nsbuiltin;
	} else if (!is_dynamic()) {
		goto out;
	} else {
#ifdef __ELF__
		if (snprintf(buf, sizeof(buf), "nss_%s.so.%d", mod.name,
		    NSS_MODULE_INTERFACE_VERSION) >= (int)sizeof(buf))
			goto out;
		mod.handle = dlopen(buf, RTLD_LOCAL | RTLD_LAZY);
		if (mod.handle == NULL) {
#ifdef _NSS_DEBUG
			/*
			 * This gets pretty annoying, since the built-in
			 * sources are not yet modules.
			 */
			/* XXX log some error? */
#endif
			goto out;
		}
		reg_fn = (nss_module_register_fn) dlsym(mod.handle,
		    "nss_module_register");
		if (reg_fn == NULL) {
			(void) dlclose(mod.handle);
			mod.handle = NULL;
			/* XXX log some error? */
			goto out;
		}
#else /* ! __ELF__ */
		mod.handle = NULL;
#endif /* __ELF__ */
	}
	mod.mtab = (*reg_fn)(mod.name, &mod.mtabsize, &mod.unregister);
	if (mod.mtab == NULL || mod.mtabsize == 0) {
#ifdef __ELF__
		if (mod.handle != _nsbuiltin)
			(void) dlclose(mod.handle);
#endif /* __ELF__ */
		mod.handle = NULL;
		/* XXX log some error? */
		goto out;
	}
	if (mod.mtabsize > 1)
		qsort(mod.mtab, mod.mtabsize, sizeof(mod.mtab[0]),
		    _nsmtabcmp);
 out:
	new = _nsvect_append(&mod, _nsmod, &_nsmodsize, sizeof(*_nsmod));
	if (new == NULL) {
		_nsmodfree(&mod);
		return (-1);
	}
	_nsmod = new;
	/* _nsmodsize already incremented */

	qsort(_nsmod, _nsmodsize, sizeof(*_nsmod), _nsmodcmp);
	return (0);
}

static void
_nsloadbuiltin(void)
{

	/* Do nothing, for now. */
}

int
_nsdbtaddsrc(ns_dbt *dbt, const ns_src *src)
{
	void		*new;
	const ns_mod	*mod;
	ns_mod		 modkey;

	_DIAGASSERT(dbt != NULL);
	_DIAGASSERT(src != NULL);

	new = _nsvect_append(src, dbt->srclist, &dbt->srclistsize,
	    sizeof(*src));
	if (new == NULL)
		return (-1);
	dbt->srclist = new;
	/* dbt->srclistsize already incremented */

	modkey.name = src->name;
	mod = bsearch(&modkey, _nsmod, _nsmodsize, sizeof(*_nsmod), _nsmodcmp);
	if (mod == NULL)
		return (_nsloadmod(src->name, NULL));

	return (0);
}

void
_nsdbtdump(const ns_dbt *dbt)
{
	unsigned int	i;

	_DIAGASSERT(dbt != NULL);

	printf("%s (%d source%s):", dbt->name, dbt->srclistsize,
	    dbt->srclistsize == 1 ? "" : "s");
	for (i = 0; i < dbt->srclistsize; i++) {
		printf(" %s", dbt->srclist[i].name);
		if (!(dbt->srclist[i].flags &
		    (NS_UNAVAIL|NS_NOTFOUND|NS_TRYAGAIN)) &&
		    (dbt->srclist[i].flags & NS_SUCCESS))
			continue;
		printf(" [");
		if (!(dbt->srclist[i].flags & NS_SUCCESS))
			printf(" SUCCESS=continue");
		if (dbt->srclist[i].flags & NS_UNAVAIL)
			printf(" UNAVAIL=return");
		if (dbt->srclist[i].flags & NS_NOTFOUND)
			printf(" NOTFOUND=return");
		if (dbt->srclist[i].flags & NS_TRYAGAIN)
			printf(" TRYAGAIN=return");
		printf(" ]");
	}
	printf("\n");
}

static void
_nssrclist_free(ns_src **src, u_int srclistsize)
{
	u_int	i;

	for (i = 0; i < srclistsize; i++) {
		if ((*src)[i].name != NULL)
			free(__UNCONST((*src)[i].name));
	}
	free(*src);
	*src = NULL;
}

static void
_nsdbtfree(ns_dbt *dbt)
{

	_nssrclist_free(&dbt->srclist, dbt->srclistsize);
	if (dbt->name != NULL)
		free(__UNCONST(dbt->name));
}

int
_nsdbtput(const ns_dbt *dbt)
{
	ns_dbt	*p;
	void	*new;
	u_int	i;

	_DIAGASSERT(dbt != NULL);

	for (i = 0; i < _nsmapsize; i++) {
		p = _nsvect_elem(i, _nsmap, _nsmapsize, sizeof(*_nsmap));
		if (strcasecmp(dbt->name, p->name) == 0) {
					/* overwrite existing entry */
			if (p->srclist != NULL)
				_nssrclist_free(&p->srclist, p->srclistsize);
			memmove(p, dbt, sizeof(*dbt));
			return (0);
		}
	}
	new = _nsvect_append(dbt, _nsmap, &_nsmapsize, sizeof(*_nsmap));
	if (new == NULL)
		return (-1);
	_nsmap = new;
	/* _nsmapsize already incremented */

	return (0);
}

/*
 * This function is called each time nsdispatch() is called.  If this
 * is the first call, or if the configuration has changed, (re-)prepare
 * the global data used by NSS.
 */
static int
_nsconfigure(void)
{
#ifdef _REENTRANT
	static mutex_t	_nsconflock = MUTEX_INITIALIZER;
#endif
	static time_t	_nsconfmod;
	struct stat	statbuf;

	mutex_lock(&_nsconflock);

	if (stat(_PATH_NS_CONF, &statbuf) == -1) {
		/*
		 * No nsswitch.conf; just use whatever configuration we
		 * currently have, or fall back on the defaults specified
		 * by the caller.
		 */
		mutex_unlock(&_nsconflock);
		return (0);
	}

	if (statbuf.st_mtime <= _nsconfmod) {
		/* Internal state is up-to-date with nsswitch.conf. */
		mutex_unlock(&_nsconflock);
		return (0);
	}

	/*
	 * Ok, we've decided we need to update the nsswitch configuration
	 * structures.  Acquire a write-lock on _nslock while continuing
	 * to hold _nsconflock.  Acquiring a write-lock blocks while
	 * waiting for other threads already holding a read-lock to clear.
	 * We hold _nsconflock for the duration, and update the time stamp
	 * at the end of the update operation, at which time we release
	 * both locks.
	 */
	rwlock_wrlock(&_nslock);

	_nsyyin = fopen(_PATH_NS_CONF, "r");
	if (_nsyyin == NULL) {
		/*
		 * Unable to open nsswitch.conf; behave as though the
		 * stat() above failed.  Even though we have already
		 * updated _nsconfmod, if the file reappears, the
		 * mtime will change.
		 */
		goto out;
	}

	_NSVECT_FREE(_nsmap, &_nsmapsize, sizeof(*_nsmap),
	    (_nsvect_free_elem) _nsdbtfree);
	_NSVECT_FREE(_nsmod, &_nsmodsize, sizeof(*_nsmod),
	    (_nsvect_free_elem) _nsmodfree);

	_nsloadbuiltin();

	_nsyyparse();
	(void) fclose(_nsyyin);
	if (_nsmapsize != 0)
		qsort(_nsmap, _nsmapsize, sizeof(*_nsmap), _nsdbtcmp);

	_nsconfmod = statbuf.st_mtime;

 out:
	rwlock_unlock(&_nslock);
	mutex_unlock(&_nsconflock);
	return (0);
}

static nss_method
_nsmethod(const char *source, const char *database, const char *method,
    const ns_dtab disp_tab[], void **cb_data)
{
	int	curdisp;
	ns_mod	*mod, modkey;
	ns_mtab	*mtab, mtabkey;

	if (disp_tab != NULL) {
		for (curdisp = 0; disp_tab[curdisp].src != NULL; curdisp++) {
			if (strcasecmp(source, disp_tab[curdisp].src) == 0) {
				*cb_data = disp_tab[curdisp].cb_data;
				return (disp_tab[curdisp].callback);
			}
		}
	}

	modkey.name = source;
	mod = bsearch(&modkey, _nsmod, _nsmodsize, sizeof(*_nsmod),
	    _nsmodcmp);
	if (mod != NULL && mod->handle != NULL) {
		mtabkey.database = database;
		mtabkey.name = method;
		mtab = bsearch(&mtabkey, mod->mtab, mod->mtabsize,
		    sizeof(mod->mtab[0]), _nsmtabcmp);
		if (mtab != NULL) {
			*cb_data = mtab->mdata;
			return (mtab->method);
		}
	}

	*cb_data = NULL;
	return (NULL);
}

int
/*ARGSUSED*/
nsdispatch(void *retval, const ns_dtab disp_tab[], const char *database,
	    const char *method, const ns_src defaults[], ...)
{
	static int	 _nsdispatching;
#ifdef _REENTRANT
	struct _ns_drec	 drec, *ldrec;
#endif
	va_list		 ap;
	int		 i, result;
	ns_dbt		 key;
	const ns_dbt	*dbt;
	const ns_src	*srclist;
	int		 srclistsize;
	nss_method	 cb;
	void		*cb_data;

	/* retval may be NULL */
	/* disp_tab may be NULL */
	_DIAGASSERT(database != NULL);
	_DIAGASSERT(method != NULL);
	_DIAGASSERT(defaults != NULL);
	if (database == NULL || method == NULL || defaults == NULL)
		return (NS_UNAVAIL);

	/*
	 * In both the threaded and non-threaded cases, avoid reloading
	 * the configuration if the current thread is already running
	 * nsdispatch() (i.e. recursive call).
	 *
	 * In the non-threaded case, this avoids changing the data structures
	 * while we're using them.
	 *
	 * In the threaded case, this avoids trying to take a write lock
	 * while the current thread holds a read lock (which would result
	 * in deadlock).
	 */
#ifdef _REENTRANT
	if (__isthreaded) {
		drec.thr = thr_self();
		mutex_lock(&_ns_drec_lock);
		LIST_FOREACH(ldrec, &_ns_drec, list) {
			if (ldrec->thr == drec.thr)
				break;
		}
		LIST_INSERT_HEAD(&_ns_drec, &drec, list);
		mutex_unlock(&_ns_drec_lock);
		if (ldrec == NULL && _nsconfigure()) {
			mutex_lock(&_ns_drec_lock);
			LIST_REMOVE(&drec, list);
			mutex_unlock(&_ns_drec_lock);
			return (NS_UNAVAIL);
		}
	} else
#endif /* _REENTRANT */
	if (_nsdispatching++ == 0 && _nsconfigure()) {
		_nsdispatching--;
		return (NS_UNAVAIL);
	}

	rwlock_rdlock(&_nslock);

	key.name = database;
	dbt = bsearch(&key, _nsmap, _nsmapsize, sizeof(*_nsmap), _nsdbtcmp);
	if (dbt != NULL) {
		srclist = dbt->srclist;
		srclistsize = dbt->srclistsize;
	} else {
		srclist = defaults;
		srclistsize = 0;
		while (srclist[srclistsize].name != NULL)
			srclistsize++;
	}
	result = 0;

	for (i = 0; i < srclistsize; i++) {
		cb = _nsmethod(srclist[i].name, database, method,
		    disp_tab, &cb_data);
		result = 0;
		if (cb != NULL) {
			va_start(ap, defaults);
			result = (*cb)(retval, cb_data, ap);
			va_end(ap);
			if (defaults[0].flags & NS_FORCEALL)
				continue;
			if (result & srclist[i].flags)
				break;
		}
	}
	result &= NS_STATUSMASK;	/* clear private flags in result */

	rwlock_unlock(&_nslock);

#ifdef _REENTRANT
	if (__isthreaded) {
		mutex_lock(&_ns_drec_lock);
		LIST_REMOVE(&drec, list);
		mutex_unlock(&_ns_drec_lock);
	} else
#endif /* _REENTRANT */
		_nsdispatching--;

	return (result ? result : NS_NOTFOUND);
}
