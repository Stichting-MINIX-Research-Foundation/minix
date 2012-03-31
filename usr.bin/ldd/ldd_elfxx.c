/*	$NetBSD: ldd_elfxx.c,v 1.4 2009/09/07 04:49:03 dholland Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * Copyright 1996 John D. Polstra.
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
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
 *      This product includes software developed by John Polstra.
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
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: ldd_elfxx.c,v 1.4 2009/09/07 04:49:03 dholland Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "debug.h"
#include "rtld.h"
#include "ldd.h"

#define munmap minix_munmap

/*
 * elfxx_ldd() - bit-size independant ELF ldd implementation.
 * returns 0 on success and -1 on failure.
 */
int
ELFNAME(ldd)(int fd, char *path, const char *fmt1, const char *fmt2)
{
	struct stat st;

	if (lseek(fd, 0, SEEK_SET) < 0 ||
	    fstat(fd, &st) < 0) {
		_rtld_error("%s: %s", path, strerror(errno));
		return -1;
	}

	_rtld_pagesz = sysconf(_SC_PAGESIZE);

#ifdef RTLD_ARCH_SUBDIR
	_rtld_add_paths(path, &_rtld_default_paths,
	    RTLD_DEFAULT_LIBRARY_PATH "/" RTLD_ARCH_SUBDIR);
#endif
	_rtld_add_paths(path, &_rtld_default_paths, RTLD_DEFAULT_LIBRARY_PATH);

	_rtld_paths = NULL;
	_rtld_trust = (st.st_mode & (S_ISUID | S_ISGID)) == 0;
	if (_rtld_trust)
		_rtld_add_paths(path, &_rtld_paths, getenv("LD_LIBRARY_PATH"));

	_rtld_process_hints(path, &_rtld_paths, &_rtld_xforms, _PATH_LD_HINTS);
	_rtld_objmain = _rtld_map_object(xstrdup(path), fd, &st);
	if (_rtld_objmain == NULL)
		return -1;

	_rtld_objmain->path = xstrdup(path);
	_rtld_digest_dynamic(path, _rtld_objmain);

	/* Link the main program into the list of objects. */
	*_rtld_objtail = _rtld_objmain;
	_rtld_objtail = &_rtld_objmain->next;
	++_rtld_objmain->refcount;

	(void) _rtld_load_needed_objects(_rtld_objmain, 0);

	if (fmt1 == NULL)
		printf("%s:\n", _rtld_objmain->path);
	main_local = path;
	main_progname = _rtld_objmain->path;
	print_needed(_rtld_objmain, fmt1, fmt2);

	while (_rtld_objlist != NULL) {
		Obj_Entry *obj = _rtld_objlist;
		_rtld_objlist = obj->next;
		while (obj->rpaths != NULL) {
			const Search_Path *rpath = obj->rpaths;
			obj->rpaths = rpath->sp_next;
			xfree(__UNCONST(rpath->sp_path));
			xfree(__UNCONST(rpath));
		}
		while (obj->needed != NULL) {
			const Needed_Entry *needed = obj->needed;
			obj->needed = needed->next;
			xfree(__UNCONST(needed));
		}
		(void) munmap(obj->mapbase, obj->mapsize);
		xfree(obj->path);
		xfree(obj);
	}

	_rtld_objmain = NULL;
	_rtld_objtail = &_rtld_objlist;
	/* Need to free _rtld_paths? */

	return 0;
}
