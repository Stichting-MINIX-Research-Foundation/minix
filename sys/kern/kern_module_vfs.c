/*	$NetBSD: kern_module_vfs.c,v 1.13 2014/12/02 13:00:38 pooka Exp $	*/

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
 * Kernel module file system interaction.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_module_vfs.c,v 1.13 2014/12/02 13:00:38 pooka Exp $");

#define _MODULE_INTERNAL
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/kmem.h>
#include <sys/kobj.h>
#include <sys/module.h>
#include <sys/namei.h>
#include <sys/pool.h>
#include <sys/stat.h>
#include <sys/vnode.h>

#include <prop/proplib.h>

static int	module_load_plist_vfs(const char *, const bool,
				      prop_dictionary_t *);

void
module_load_vfs_init(void)
{
	module_load_vfs_vec = module_load_vfs;
	aprint_normal("kern.module.path=%s\n", module_base);
}

int
module_load_vfs(const char *name, int flags, bool autoload,
	module_t *mod, prop_dictionary_t *filedictp)
{
	char *path;
	bool nochroot;
	int error;
	prop_bool_t noload;
	prop_dictionary_t moduledict;

	nochroot = false;
	error = 0;
	path = NULL;
	moduledict = NULL;
	if (filedictp)
		*filedictp = NULL;
	path = PNBUF_GET();

	if (!autoload) {
		if (strchr(name,  '/') != NULL) {
			nochroot = false;
			snprintf(path, MAXPATHLEN, "%s", name);
			error = kobj_load_vfs(&mod->mod_kobj, path, nochroot);
		} else
			error = ENOENT;
	}
	if (autoload || (error == ENOENT)) {
		if (strchr(name, '/') == NULL) {
			nochroot = true;
			snprintf(path, MAXPATHLEN, "%s/%s/%s.kmod",
			    module_base, name, name);
			error = kobj_load_vfs(&mod->mod_kobj, path, nochroot);
		} else
			error = ENOENT;
	}
	if (error != 0) {
		PNBUF_PUT(path);
		module_print("Cannot %sload kernel object `%s'"
		    " error=%d", autoload ? "auto" : "", name, error);
		return error;
	}

	/*
	 * Load and process <module>.plist if it exists.
	 */
	if (((flags & MODCTL_NO_PROP) == 0 && filedictp) || autoload) {
		error = module_load_plist_vfs(path, nochroot, &moduledict);
		if (error != 0) {
			module_print("plist load returned error %d for `%s'",
			    error, path);
			if (error != ENOENT)
				goto fail;
		} else if (autoload) {
			noload = prop_dictionary_get(moduledict, "noautoload");
			if (noload != NULL && prop_bool_true(noload)) {
				module_error("autoloading is disallowed for %s",
				    path);
				prop_object_release(moduledict);
				error = EPERM;
				goto fail;
			}
		}
		if (error == 0) {	/* can get here if error == ENOENT */
			if ((flags & MODCTL_NO_PROP) == 0 && filedictp)
				*filedictp = moduledict;
			else 
				prop_object_release(moduledict);
		}
	}

	PNBUF_PUT(path);
	return 0;

 fail:
	kobj_unload(mod->mod_kobj);
	PNBUF_PUT(path);
	return error;
}

/*
 * module_load_plist_vfs:
 *
 *	Load a plist located in the file system into memory.
 */
static int
module_load_plist_vfs(const char *modpath, const bool nochroot,
    prop_dictionary_t *filedictp)
{
	struct pathbuf *pb;
	struct nameidata nd;
	struct stat sb;
	void *base;
	char *proppath;
	const size_t plistsize = 8192;
	size_t resid;
	int error, pathlen;

	KASSERT(filedictp != NULL);
	base = NULL;

	proppath = PNBUF_GET();
	strcpy(proppath, modpath);
	pathlen = strlen(proppath);
	if ((pathlen >= 6) && (strcmp(&proppath[pathlen - 5], ".kmod") == 0)) {
		strcpy(&proppath[pathlen - 5], ".plist");
	} else if (pathlen < MAXPATHLEN - 6) {
			strcat(proppath, ".plist");
	} else {
		error = ENOENT;
		goto out1;
	}

	/* XXX this makes an unnecessary extra copy of the path */
	pb = pathbuf_create(proppath);
	if (pb == NULL) {
		error = ENOMEM;
		goto out1;
	}
	
	NDINIT(&nd, LOOKUP, FOLLOW | (nochroot ? NOCHROOT : 0), pb);

	error = vn_open(&nd, FREAD, 0);
 	if (error != 0) {
	 	goto out2;
	}

	error = vn_stat(nd.ni_vp, &sb);
	if (error != 0) {
		goto out3;
	}
	if (sb.st_size >= (plistsize - 1)) {	/* leave space for term \0 */
		error = EFBIG;
		goto out3;
	}

	base = kmem_alloc(plistsize, KM_SLEEP);
	if (base == NULL) {
		error = ENOMEM;
		goto out3;
	}

	error = vn_rdwr(UIO_READ, nd.ni_vp, base, sb.st_size, 0,
	    UIO_SYSSPACE, IO_NODELOCKED, curlwp->l_cred, &resid, curlwp);
	*((uint8_t *)base + sb.st_size) = '\0';
	if (error == 0 && resid != 0) {
		error = EFBIG;
	}
	if (error != 0) {
		kmem_free(base, plistsize);
		base = NULL;
		goto out3;
	}

	*filedictp = prop_dictionary_internalize(base);
	if (*filedictp == NULL) {
		error = EINVAL;
	}
	kmem_free(base, plistsize);
	base = NULL;
	KASSERT(error == 0);

out3:
	VOP_UNLOCK(nd.ni_vp);
	vn_close(nd.ni_vp, FREAD, kauth_cred_get());

out2:
	pathbuf_destroy(pb);

out1:
	PNBUF_PUT(proppath);
	return error;
}
