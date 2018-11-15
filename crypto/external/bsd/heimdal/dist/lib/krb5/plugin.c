/*	$NetBSD: plugin.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "krb5_locl.h"

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#include <dirent.h>

struct krb5_plugin {
    void *symbol;
    struct krb5_plugin *next;
};

struct plugin {
    enum { DSO, SYMBOL } type;
    union {
	struct {
	    char *path;
	    void *dsohandle;
	} dso;
	struct {
	    enum krb5_plugin_type type;
	    char *name;
	    char *symbol;
	} symbol;
    } u;
    struct plugin *next;
};

static HEIMDAL_MUTEX plugin_mutex = HEIMDAL_MUTEX_INITIALIZER;
static struct plugin *registered = NULL;

/**
 * Register a plugin symbol name of specific type.
 * @param context a Keberos context
 * @param type type of plugin symbol
 * @param name name of plugin symbol
 * @param symbol a pointer to the named symbol
 * @return In case of error a non zero error com_err error is returned
 * and the Kerberos error string is set.
 *
 * @ingroup krb5_support
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_plugin_register(krb5_context context,
		     enum krb5_plugin_type type,
		     const char *name,
		     void *symbol)
{
    struct plugin *e;

    HEIMDAL_MUTEX_lock(&plugin_mutex);

    /* check for duplicates */
    for (e = registered; e != NULL; e = e->next) {
	if (e->type == SYMBOL &&
	    strcmp(e->u.symbol.name, name) == 0 &&
	    e->u.symbol.type == type && e->u.symbol.symbol == symbol) {
	    HEIMDAL_MUTEX_unlock(&plugin_mutex);
	    return 0;
	}
    }

    e = calloc(1, sizeof(*e));
    if (e == NULL) {
	HEIMDAL_MUTEX_unlock(&plugin_mutex);
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    e->type = SYMBOL;
    e->u.symbol.type = type;
    e->u.symbol.name = strdup(name);
    if (e->u.symbol.name == NULL) {
	HEIMDAL_MUTEX_unlock(&plugin_mutex);
	free(e);
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    e->u.symbol.symbol = symbol;

    e->next = registered;
    registered = e;
    HEIMDAL_MUTEX_unlock(&plugin_mutex);

    return 0;
}

static krb5_error_code
add_symbol(krb5_context context, struct krb5_plugin **list, void *symbol)
{
    struct krb5_plugin *e;

    e = calloc(1, sizeof(*e));
    if (e == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    e->symbol = symbol;
    e->next = *list;
    *list = e;
    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_plugin_find(krb5_context context,
		  enum krb5_plugin_type type,
		  const char *name,
		  struct krb5_plugin **list)
{
    struct plugin *e;
    krb5_error_code ret;

    *list = NULL;

    HEIMDAL_MUTEX_lock(&plugin_mutex);

    for (ret = 0, e = registered; e != NULL; e = e->next) {
	switch(e->type) {
	case DSO: {
	    void *sym;
	    if (e->u.dso.dsohandle == NULL)
		continue;
	    sym = dlsym(e->u.dso.dsohandle, name);
	    if (sym)
		ret = add_symbol(context, list, sym);
	    break;
	}
	case SYMBOL:
	    if (strcmp(e->u.symbol.name, name) == 0 && e->u.symbol.type == type)
		ret = add_symbol(context, list, e->u.symbol.symbol);
	    break;
	}
	if (ret) {
	    _krb5_plugin_free(*list);
	    *list = NULL;
	}
    }

    HEIMDAL_MUTEX_unlock(&plugin_mutex);
    if (ret)
	return ret;

    if (*list == NULL) {
	krb5_set_error_message(context, ENOENT, "Did not find a plugin for %s", name);
	return ENOENT;
    }

    return 0;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_plugin_free(struct krb5_plugin *list)
{
    struct krb5_plugin *next;
    while (list) {
	next = list->next;
	free(list);
	list = next;
    }
}
/*
 * module - dict of {
 *      ModuleName = [
 *          plugin = object{
 *              array = { ptr, ctx }
 *          }
 *      ]
 * }
 */

static heim_dict_t modules;

struct plugin2 {
    heim_string_t path;
    void *dsohandle;
    heim_dict_t names;
};

static void
plug_dealloc(void *ptr)
{
    struct plugin2 *p = ptr;
    heim_release(p->path);
    heim_release(p->names);
    if (p->dsohandle)
	dlclose(p->dsohandle);
}

static char *
resolve_origin(const char *di)
{
#ifdef HAVE_DLADDR
    Dl_info dl_info;
    const char *dname;
    char *path, *p;
#endif

    if (strncmp(di, "$ORIGIN/", sizeof("$ORIGIN/") - 1) &&
        strcmp(di, "$ORIGIN"))
        return strdup(di);

#ifndef HAVE_DLADDR
    return strdup(LIBDIR "/plugin/krb5");
#else /* !HAVE_DLADDR */
    di += sizeof("$ORIGIN") - 1;

    if (dladdr(_krb5_load_plugins, &dl_info) == 0)
        return strdup(LIBDIR "/plugin/krb5");

    dname = dl_info.dli_fname;
#ifdef _WIN32
    p = strrchr(dname, '\\');
    if (p == NULL)
#endif
	p = strrchr(dname, '/');
    if (p) {
        if (asprintf(&path, "%.*s%s", (int) (p - dname), dname, di) == -1)
            return NULL;
    } else {
        if (asprintf(&path, "%s%s", dname, di) == -1)
            return NULL;
    }

    return path;
#endif /* !HAVE_DLADDR */
}


/**
 * Load plugins (new system) for the given module @name (typically
 * "krb5") from the given directory @paths.
 *
 * Inputs:
 *
 * @context A krb5_context
 * @name    Name of plugin module (typically "krb5")
 * @paths   Array of directory paths where to look
 */
KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_load_plugins(krb5_context context, const char *name, const char **paths)
{
#ifdef HAVE_DLOPEN
    heim_string_t s = heim_string_create(name);
    heim_dict_t module;
    struct dirent *entry;
    krb5_error_code ret;
    const char **di;
    char *dirname = NULL;
    DIR *d;
#ifdef _WIN32
    const char * plugin_prefix;
    size_t plugin_prefix_len;

    if (asprintf(&plugin_prefix, "plugin_%s_", name) == -1)
	return;
    plugin_prefix_len = (plugin_prefix ? strlen(plugin_prefix) : 0);
#endif

    HEIMDAL_MUTEX_lock(&plugin_mutex);

    if (modules == NULL) {
	modules = heim_dict_create(11);
	if (modules == NULL) {
	    HEIMDAL_MUTEX_unlock(&plugin_mutex);
	    return;
	}
    }

    module = heim_dict_copy_value(modules, s);
    if (module == NULL) {
	module = heim_dict_create(11);
	if (module == NULL) {
	    HEIMDAL_MUTEX_unlock(&plugin_mutex);
	    heim_release(s);
	    return;
	}
	heim_dict_set_value(modules, s, module);
    }
    heim_release(s);

    for (di = paths; *di != NULL; di++) {
        free(dirname);
        dirname = resolve_origin(*di);
        if (dirname == NULL)
            continue;
	d = opendir(dirname);
	if (d == NULL)
	    continue;
	rk_cloexec_dir(d);

	while ((entry = readdir(d)) != NULL) {
	    char *n = entry->d_name;
	    char *path = NULL;
	    heim_string_t spath;
	    struct plugin2 *p;

	    /* skip . and .. */
	    if (n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0')))
		continue;

	    ret = 0;
#ifdef _WIN32
	    /*
	     * On Windows, plugins must be loaded from the same directory as
	     * heimdal.dll (typically the assembly directory) and must have
	     * the name form "plugin_<module>_<name>.dll".
	     */
	    {
		char *ext;

		if (strnicmp(n, plugin_prefix, plugin_prefix_len))
		    continue;
		ext = strrchr(n, '.');
		if (ext == NULL || stricmp(ext, ".dll"))
		     continue;

		ret = asprintf(&path, "%s\\%s", dirname, n);
		if (ret < 0 || path == NULL)
		    continue;
	    }
#endif
#ifdef __APPLE__
	    { /* support loading bundles on MacOS */
		size_t len = strlen(n);
		if (len > 7 && strcmp(&n[len - 7],  ".bundle") == 0)
		    ret = asprintf(&path, "%s/%s/Contents/MacOS/%.*s", dirname, n, (int)(len - 7), n);
	    }
#endif
	    if (ret < 0 || path == NULL)
		ret = asprintf(&path, "%s/%s", dirname, n);

	    if (ret < 0 || path == NULL)
		continue;

	    spath = heim_string_create(n);
	    if (spath == NULL) {
		free(path);
		continue;
	    }

	    /* check if already cached */
	    p = heim_dict_copy_value(module, spath);
	    if (p == NULL) {
		p = heim_alloc(sizeof(*p), "krb5-plugin", plug_dealloc);
		if (p)
		    p->dsohandle = dlopen(path, RTLD_LOCAL|RTLD_LAZY);

		if (p && p->dsohandle) {
		    p->path = heim_retain(spath);
		    p->names = heim_dict_create(11);
		    heim_dict_set_value(module, spath, p);
		}
	    }
            heim_release(p);
	    heim_release(spath);
	    free(path);
	}
	closedir(d);
    }
    free(dirname);
    HEIMDAL_MUTEX_unlock(&plugin_mutex);
    heim_release(module);
#ifdef _WIN32
    if (plugin_prefix)
	free(plugin_prefix);
#endif
#endif /* HAVE_DLOPEN */
}

/**
 * Unload plugins (new system)
 */
KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_unload_plugins(krb5_context context, const char *name)
{
    HEIMDAL_MUTEX_lock(&plugin_mutex);
    heim_release(modules);
    modules = NULL;
    HEIMDAL_MUTEX_unlock(&plugin_mutex);
}

/*
 *
 */

struct common_plugin_method {
    int			version;
    krb5_error_code	(*init)(krb5_context, void **);
    void		(*fini)(void *);
};

struct plug {
    void *dataptr;
    void *ctx;
};

static void
plug_free(void *ptr)
{
    struct plug *pl = ptr;
    if (pl->dataptr) {
	struct common_plugin_method *cpm = pl->dataptr;
	cpm->fini(pl->ctx);
    }
}

struct iter_ctx {
    krb5_context context;
    heim_string_t n;
    const char *name;
    int min_version;
    int flags;
    heim_array_t result;
    krb5_error_code (KRB5_LIB_CALL *func)(krb5_context, const void *, void *, void *);
    void *userctx;
    krb5_error_code ret;
};

static void
search_modules(heim_object_t key, heim_object_t value, void *ctx)
{
    struct iter_ctx *s = ctx;
    struct plugin2 *p = value;
    struct plug *pl = heim_dict_copy_value(p->names, s->n);
    struct common_plugin_method *cpm;

    if (pl == NULL) {
	if (p->dsohandle == NULL)
	    return;

	pl = heim_alloc(sizeof(*pl), "struct-plug", plug_free);

	cpm = pl->dataptr = dlsym(p->dsohandle, s->name);
	if (cpm) {
	    int ret;

	    ret = cpm->init(s->context, &pl->ctx);
	    if (ret)
		cpm = pl->dataptr = NULL;
	}
	heim_dict_set_value(p->names, s->n, pl);
    } else {
	cpm = pl->dataptr;
    }

    if (cpm && cpm->version >= s->min_version)
	heim_array_append_value(s->result, pl);
    heim_release(pl);
}

static void
eval_results(heim_object_t value, void *ctx, int *stop)
{
    struct plug *pl = value;
    struct iter_ctx *s = ctx;

    if (s->ret != KRB5_PLUGIN_NO_HANDLE)
	return;

    s->ret = s->func(s->context, pl->dataptr, pl->ctx, s->userctx);
    if (s->ret != KRB5_PLUGIN_NO_HANDLE
        && !(s->flags & KRB5_PLUGIN_INVOKE_ALL))
        *stop = 1;
}

/**
 * Run plugins for the given @module (e.g., "krb5") and @name (e.g.,
 * "kuserok").  Specifically, the @func is invoked once per-plugin with
 * four arguments: the @context, the plugin symbol value (a pointer to a
 * struct whose first three fields are the same as struct common_plugin_method),
 * a context value produced by the plugin's init method, and @userctx.
 *
 * @func should unpack arguments for a plugin function and invoke it
 * with arguments taken from @userctx.  @func should save plugin
 * outputs, if any, in @userctx.
 *
 * All loaded and registered plugins are invoked via @func until @func
 * returns something other than KRB5_PLUGIN_NO_HANDLE.  Plugins that
 * have nothing to do for the given arguments should return
 * KRB5_PLUGIN_NO_HANDLE.
 *
 * Inputs:
 *
 * @context     A krb5_context
 * @module      Name of module (typically "krb5")
 * @name        Name of pluggable interface (e.g., "kuserok")
 * @min_version Lowest acceptable plugin minor version number
 * @flags       Flags (none defined at this time)
 * @userctx     Callback data for the callback function @func
 * @func        A callback function, invoked once per-plugin
 *
 * Outputs: None, other than the return value and such outputs as are
 *          gathered by @func.
 */
KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_plugin_run_f(krb5_context context,
		   const char *module,
		   const char *name,
		   int min_version,
		   int flags,
		   void *userctx,
		   krb5_error_code (KRB5_LIB_CALL *func)(krb5_context, const void *, void *, void *))
{
    heim_string_t m = heim_string_create(module);
    heim_dict_t dict;
    void *plug_ctx;
    struct common_plugin_method *cpm;
    struct iter_ctx s;
    struct krb5_plugin *registered_plugins = NULL;
    struct krb5_plugin *p;

    /* Get registered plugins */
    (void) _krb5_plugin_find(context, SYMBOL, name, &registered_plugins);

    HEIMDAL_MUTEX_lock(&plugin_mutex);

    s.context = context;
    s.name = name;
    s.n = heim_string_create(name);
    s.flags = flags;
    s.min_version = min_version;
    s.result = heim_array_create();
    s.func = func;
    s.userctx = userctx;
    s.ret = KRB5_PLUGIN_NO_HANDLE;

    /* Get loaded plugins */
    dict = heim_dict_copy_value(modules, m);
    heim_release(m);

    /* Add loaded plugins to s.result array */
    if (dict)
	heim_dict_iterate_f(dict, &s, search_modules);

    /* We don't need to hold plugin_mutex during plugin invocation */
    HEIMDAL_MUTEX_unlock(&plugin_mutex);

    /* Invoke registered plugins (old system) */
    for (p = registered_plugins; p; p = p->next) {
	/*
	 * XXX This is the wrong way to handle registered plugins, as we
	 * call init/fini on each invocation!  We do this because we
	 * have nowhere in the struct plugin registered list to store
	 * the context allocated by the plugin's init function.  (But at
	 * least we do call init/fini!)
	 *
	 * What we should do is adapt the old plugin system to the new
	 * one and change how we register plugins so that we use the new
	 * struct plug to keep track of their context structures, that
	 * way we can init once, invoke many times, then fini.
	 */
	cpm = (struct common_plugin_method *)p->symbol;
	s.ret = cpm->init(context, &plug_ctx);
	if (s.ret)
	    continue;
	s.ret = s.func(s.context, p->symbol, plug_ctx, s.userctx);
	cpm->fini(plug_ctx);
	if (s.ret != KRB5_PLUGIN_NO_HANDLE &&
            !(flags & KRB5_PLUGIN_INVOKE_ALL))
	    break;
    }
    _krb5_plugin_free(registered_plugins);

    /* Invoke loaded plugins (new system) */
    if (s.ret == KRB5_PLUGIN_NO_HANDLE)
	heim_array_iterate_f(s.result, &s, eval_results);

    heim_release(s.result);
    heim_release(s.n);
    heim_release(dict);

    return s.ret;
}
