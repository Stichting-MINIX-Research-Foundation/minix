/*	$NetBSD: symver.c,v 1.1 2011/06/25 05:45:12 nonaka Exp $	*/

/*-
 * Copyright 1996, 1997, 1998, 1999, 2000 John D. Polstra.
 * Copyright 2003 Alexander Kabaev <kan@FreeBSD.ORG>.
 * Copyright 2009, 2010, 2011 Konstantin Belousov <kib@FreeBSD.ORG>.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: head/libexec/rtld-elf/rtld.c 220004 2011-03-25 18:23:10Z avg $
 */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by NONAKA Kimihiro.
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

#include <sys/cdefs.h>
__RCSID("$NetBSD: symver.c,v 1.1 2011/06/25 05:45:12 nonaka Exp $");

#include <sys/param.h>
#include <sys/exec_elf.h>
#include <string.h>

#include "debug.h"
#include "rtld.h"


int
_rtld_object_match_name(const Obj_Entry *obj, const char *name)
{
	Name_Entry *entry;

	STAILQ_FOREACH(entry, &obj->names, link) {
		dbg(("name: %s, entry->name: %s", name, entry->name));
		if (strcmp(name, entry->name) == 0)
			return 1;
	}
	return 0;
}

static Obj_Entry *
locate_dependency(const Obj_Entry *obj, const char *name)
{
	const Objlist_Entry *entry;
	const Needed_Entry *needed;

	SIMPLEQ_FOREACH(entry, &_rtld_list_main, link) {
		if (_rtld_object_match_name(entry->obj, name))
			return entry->obj;
	}

	for (needed = obj->needed; needed != NULL; needed = needed->next) {
		dbg(("needed: name: %s, str: %s", name,
		    &obj->strtab[needed->name]));
		if (strcmp(name, &obj->strtab[needed->name]) == 0 ||
		    (needed->obj != NULL && _rtld_object_match_name(needed->obj, name))) {
			/*
			 * If there is DT_NEEDED for the name we are looking
			 * for, we are all set.  Note that object might not be
			 * found if dependency was not loaded yet, so the
			 * function can return NULL here.  This is expected
			 * and handled properly by the caller.
			 */
			return needed->obj;
		}
	}

	_rtld_error("%s: Unexpected inconsistency: dependency %s not found",
	    obj->path, name);
	return NULL;
}

static int
check_object_provided_version(Obj_Entry *refobj, const Obj_Entry *depobj,
    const Elf_Vernaux *vna)
{
	const char *vername = &refobj->strtab[vna->vna_name];
	const char *depstrtab = depobj->strtab;
	const Elf_Verdef *vd = depobj->verdef;
	const Elf_Word hash = vna->vna_hash;

	if (vd == NULL) {
		_rtld_error("%s: version %s required by %s not defined",
		    depobj->path, vername, refobj->path);
		return -1;
	}

	for (;; vd = (const Elf_Verdef *)((const char *)vd + vd->vd_next)) {
		if (vd->vd_version != VER_DEF_CURRENT) {
			_rtld_error(
			    "%s: Unsupported version %d of Elf_Verdef entry",
			    depobj->path, vd->vd_version);
			return -1;
		}
		dbg(("hash: 0x%x, vd_hash: 0x%x", hash, vd->vd_hash));
		if (hash == vd->vd_hash) {
			const Elf_Verdaux *vda = (const Elf_Verdaux *)
			    ((const char *)vd + vd->vd_aux);
			dbg(("vername: %s, str: %s", vername,
			    &depstrtab[vda->vda_name]));
			if (strcmp(vername, &depstrtab[vda->vda_name]) == 0)
				return 0;
		}
		if (vd->vd_next == 0)
			break;
	}
	if (vna->vna_flags & VER_FLG_WEAK)
		return 0;

	_rtld_error("%s: version %s required by %s not found", depobj->path,
	    vername, refobj->path);
	return -1;
}

int
_rtld_verify_object_versions(Obj_Entry *obj)
{
	const char *strtab = obj->strtab;
	const Elf_Verneed *vn;
	const Elf_Vernaux *vna;
	const Elf_Verdef *vd;
	const Elf_Verdaux *vda;
	const Obj_Entry *depobj;
	int maxvertab, vernum;

	dbg(("obj->path: %s", obj->path));

	/*
	 * If we don't have string table or objects that have their version
	 * requirements already checked, we must be ok.
	 */
	if (strtab == NULL || obj->vertab != NULL)
		return 0;

	maxvertab = 0;

	/*
	 * Walk over defined and required version records and figure out
	 * max index used by any of them. Do very basic sanity checking
	 * while there.
	 */
	for (vn = obj->verneed;
	     vn != NULL;
	     vn = (const Elf_Verneed *)((const char *)vn + vn->vn_next)) {

		if (vn->vn_version != VER_NEED_CURRENT) {
			_rtld_error(
			    "%s: Unsupported version %d of Elf_Verneed entry",
			    obj->path, vn->vn_version);
			return -1;
		}

		dbg(("verneed: vn_file: %d, str: %s",
		    vn->vn_file, &strtab[vn->vn_file]));
		depobj = locate_dependency(obj, &strtab[vn->vn_file]);
		assert(depobj != NULL);

		for (vna = (const Elf_Vernaux *)((const char *)vn + vn->vn_aux);
		     /*CONSTCOND*/1;
		     vna = (const Elf_Vernaux *)((const char *)vna + vna->vna_next)) {

			if (check_object_provided_version(obj, depobj, vna) == -1)
				return -1;

			vernum = VER_NEED_IDX(vna->vna_other);
			if (vernum > maxvertab)
				maxvertab = vernum;

			if (vna->vna_next == 0) {
				/* No more symbols. */
				break;
			}
		}

		if (vn->vn_next == 0) {
			/* No more dependencies. */
			break;
		}
	}

	for (vd = obj->verdef;
	     vd != NULL;
	     vd = (const Elf_Verdef *)((const char *)vd + vd->vd_next)) {

		if (vd->vd_version != VER_DEF_CURRENT) {
			_rtld_error(
			    "%s: Unsupported version %d of Elf_Verdef entry",
			    obj->path, vd->vd_version);
			return -1;
		}

		dbg(("verdef: vn_ndx: 0x%x", vd->vd_ndx));
		vernum = VER_DEF_IDX(vd->vd_ndx);
		if (vernum > maxvertab)
			maxvertab = vernum;

		if (vd->vd_next == 0) {
			/* No more definitions. */
			break;
		}
	}

	dbg(("maxvertab: %d", maxvertab));
	if (maxvertab == 0)
		return 0;

	/*
	 * Store version information in array indexable by version index.
	 * Verify that object version requirements are satisfied along the
	 * way.
	 */
	obj->vertabnum = maxvertab + 1;
	obj->vertab = (Ver_Entry *)xcalloc(obj->vertabnum * sizeof(Ver_Entry));

	for (vn = obj->verneed;
	     vn != NULL;
	     vn = (const Elf_Verneed *)((const char *)vn + vn->vn_next)) {

		for (vna = (const Elf_Vernaux *)((const char *)vn + vn->vn_aux);
		     /*CONSTCOND*/1;
		     vna = (const Elf_Vernaux *)((const char *)vna + vna->vna_next)) {

			vernum = VER_NEED_IDX(vna->vna_other);
			assert(vernum <= maxvertab);
			obj->vertab[vernum].hash = vna->vna_hash;
			obj->vertab[vernum].name = &strtab[vna->vna_name];
			obj->vertab[vernum].file = &strtab[vn->vn_file];
			obj->vertab[vernum].flags =
			    (vna->vna_other & VER_NEED_HIDDEN)
			      ? VER_INFO_HIDDEN : 0;
			dbg(("verneed: vernum: %d, hash: 0x%x, name: %s, "
			    "file: %s, flags: 0x%x", vernum,
			    obj->vertab[vernum].hash, obj->vertab[vernum].name,
			    obj->vertab[vernum].file,
			    obj->vertab[vernum].flags));

			if (vna->vna_next == 0) {
				/* No more symbols. */
				break;
			}
		}
		if (vn->vn_next == 0) {
			/* No more dependencies. */
			break;
		}
	}

	for (vd = obj->verdef;
	     vd != NULL;
	     vd = (const Elf_Verdef *)((const char *)vd + vd->vd_next)) {

		if ((vd->vd_flags & VER_FLG_BASE) == 0) {
			vernum = VER_DEF_IDX(vd->vd_ndx);
			assert(vernum <= maxvertab);
			vda = (const Elf_Verdaux *)
			    ((const char *)vd + vd->vd_aux);
			obj->vertab[vernum].hash = vd->vd_hash;
			obj->vertab[vernum].name = &strtab[vda->vda_name];
			obj->vertab[vernum].file = NULL;
			obj->vertab[vernum].flags = 0;
			dbg(("verdef: vernum: %d, hash: 0x%x, name: %s",
			    vernum, obj->vertab[vernum].hash,
			    obj->vertab[vernum].name));
		}

		if (vd->vd_next == 0) {
			/* No more definitions. */
			break;
		}
	}

	return 0;
}
