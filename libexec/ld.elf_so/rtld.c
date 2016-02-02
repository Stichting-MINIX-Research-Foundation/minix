/*	$NetBSD: rtld.c,v 1.177 2015/04/06 09:34:15 yamt Exp $	 */

/*
 * Copyright 1996 John D. Polstra.
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
 * Copyright 2002 Charles M. Hannum <root@ihack.net>
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

/*
 * Dynamic linker for ELF.
 *
 * John Polstra <jdp@polstra.com>.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: rtld.c,v 1.177 2015/04/06 09:34:15 yamt Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/mman.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <lwp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <ctype.h>

#include <dlfcn.h>
#include "debug.h"
#include "rtld.h"

#if !defined(lint)
#include "sysident.h"
#endif

/*
 * Function declarations.
 */
static void     _rtld_init(caddr_t, caddr_t, const char *);
static void     _rtld_exit(void);

Elf_Addr        _rtld(Elf_Addr *, Elf_Addr);


/*
 * Data declarations.
 */
static char    *error_message;	/* Message for dlopen(), or NULL */

struct r_debug  _rtld_debug;	/* for GDB; */
bool            _rtld_trust;	/* False for setuid and setgid programs */
Obj_Entry      *_rtld_objlist;	/* Head of linked list of shared objects */
Obj_Entry     **_rtld_objtail;	/* Link field of last object in list */
Obj_Entry      *_rtld_objmain;	/* The main program shared object */
Obj_Entry       _rtld_objself;	/* The dynamic linker shared object */
u_int		_rtld_objcount;	/* Number of objects in _rtld_objlist */
u_int		_rtld_objloads;	/* Number of objects loaded in _rtld_objlist */
u_int		_rtld_objgen;	/* Generation count for _rtld_objlist */
const char	_rtld_path[] = _PATH_RTLD;

/* Initialize a fake symbol for resolving undefined weak references. */
Elf_Sym		_rtld_sym_zero = {
    .st_info	= ELF_ST_INFO(STB_GLOBAL, STT_NOTYPE),
    .st_shndx	= SHN_ABS,
};
size_t	_rtld_pagesz;	/* Page size, as provided by kernel */

Search_Path    *_rtld_default_paths;
Search_Path    *_rtld_paths;

Library_Xform  *_rtld_xforms;
static void    *auxinfo;

/*
 * Global declarations normally provided by crt0.
 */
char           *__progname;
char          **environ;

#if !defined(__minix)
static volatile bool _rtld_mutex_may_recurse;
#endif /* !defined(__minix) */

#if defined(RTLD_DEBUG)
#ifndef __sh__
extern Elf_Addr _GLOBAL_OFFSET_TABLE_[];
#else  /* 32-bit SuperH */
register Elf_Addr *_GLOBAL_OFFSET_TABLE_ asm("r12");
#endif
#endif /* RTLD_DEBUG */
extern Elf_Dyn  _DYNAMIC;

static void _rtld_call_fini_functions(sigset_t *, int);
static void _rtld_call_init_functions(sigset_t *);
static void _rtld_initlist_visit(Objlist *, Obj_Entry *, int);
static void _rtld_initlist_tsort(Objlist *, int);
static Obj_Entry *_rtld_dlcheck(void *);
static void _rtld_init_dag(Obj_Entry *);
static void _rtld_init_dag1(Obj_Entry *, Obj_Entry *);
static void _rtld_objlist_remove(Objlist *, Obj_Entry *);
static void _rtld_objlist_clear(Objlist *);
static void _rtld_unload_object(sigset_t *, Obj_Entry *, bool);
static void _rtld_unref_dag(Obj_Entry *);
static Obj_Entry *_rtld_obj_from_addr(const void *);

static inline void
_rtld_call_initfini_function(const Obj_Entry *obj, Elf_Addr func, sigset_t *mask)
{
	_rtld_exclusive_exit(mask);
	_rtld_call_function_void(obj, func);
	_rtld_exclusive_enter(mask);
}

static void
_rtld_call_fini_function(Obj_Entry *obj, sigset_t *mask, u_int cur_objgen)
{
	if (obj->fini_arraysz == 0 && (obj->fini == 0 || obj->fini_called))
		return;

	if (obj->fini != 0 && !obj->fini_called) {
		dbg (("calling fini function %s at %p%s", obj->path,
		    (void *)obj->fini,
		    obj->z_initfirst ? " (DF_1_INITFIRST)" : ""));
		obj->fini_called = 1;
		_rtld_call_initfini_function(obj, obj->fini, mask);
	}
#ifdef HAVE_INITFINI_ARRAY
	/*
	 * Now process the fini_array if it exists.  Simply go from
	 * start to end.  We need to make restartable so just advance
	 * the array pointer and decrement the size each time through
	 * the loop.
	 */
	while (obj->fini_arraysz > 0 && _rtld_objgen == cur_objgen) {
		Elf_Addr fini = *obj->fini_array++;
		obj->fini_arraysz--;
		dbg (("calling fini array function %s at %p%s", obj->path,
		    (void *)fini,
		    obj->z_initfirst ? " (DF_1_INITFIRST)" : ""));
		_rtld_call_initfini_function(obj, fini, mask);
	}
#endif /* HAVE_INITFINI_ARRAY */
}

static void
_rtld_call_fini_functions(sigset_t *mask, int force)
{
	Objlist_Entry *elm;
	Objlist finilist;
	u_int cur_objgen;

	dbg(("_rtld_call_fini_functions(%d)", force));

restart:
	cur_objgen = ++_rtld_objgen;
	SIMPLEQ_INIT(&finilist);
	_rtld_initlist_tsort(&finilist, 1);

	/* First pass: objects _not_ marked with DF_1_INITFIRST. */
	SIMPLEQ_FOREACH(elm, &finilist, link) {
		Obj_Entry * const obj = elm->obj;
		if (!obj->z_initfirst) {
			if (obj->refcount > 0 && !force) {
				continue;
			}
			/*
			 * XXX This can race against a concurrent dlclose().
			 * XXX In that case, the object could be unmapped before
			 * XXX the fini() call or the fini_array has completed.
			 */
			_rtld_call_fini_function(obj, mask, cur_objgen);
			if (_rtld_objgen != cur_objgen) {
				dbg(("restarting fini iteration"));
				_rtld_objlist_clear(&finilist);
				goto restart;
		}
		}
	}

	/* Second pass: objects marked with DF_1_INITFIRST. */
	SIMPLEQ_FOREACH(elm, &finilist, link) {
		Obj_Entry * const obj = elm->obj;
		if (obj->refcount > 0 && !force) {
			continue;
		}
		/* XXX See above for the race condition here */
		_rtld_call_fini_function(obj, mask, cur_objgen);
		if (_rtld_objgen != cur_objgen) {
			dbg(("restarting fini iteration"));
			_rtld_objlist_clear(&finilist);
			goto restart;
		}
	}

        _rtld_objlist_clear(&finilist);
}

static void
_rtld_call_init_function(Obj_Entry *obj, sigset_t *mask, u_int cur_objgen)
{
	if (obj->init_arraysz == 0 && (obj->init_called || obj->init == 0))
		return;

	if (!obj->init_called && obj->init != 0) {
		dbg (("calling init function %s at %p%s",
		    obj->path, (void *)obj->init,
		    obj->z_initfirst ? " (DF_1_INITFIRST)" : ""));
		obj->init_called = 1;
		_rtld_call_initfini_function(obj, obj->init, mask);
	}

#ifdef HAVE_INITFINI_ARRAY
	/*
	 * Now process the init_array if it exists.  Simply go from
	 * start to end.  We need to make restartable so just advance
	 * the array pointer and decrement the size each time through
	 * the loop.
	 */
	while (obj->init_arraysz > 0 && _rtld_objgen == cur_objgen) {
		Elf_Addr init = *obj->init_array++;
		obj->init_arraysz--;
		dbg (("calling init_array function %s at %p%s",
		    obj->path, (void *)init,
		    obj->z_initfirst ? " (DF_1_INITFIRST)" : ""));
		_rtld_call_initfini_function(obj, init, mask);
	}
#endif /* HAVE_INITFINI_ARRAY */
}

static void
_rtld_call_init_functions(sigset_t *mask)
{
	Objlist_Entry *elm;
	Objlist initlist;
	u_int cur_objgen;

	dbg(("_rtld_call_init_functions()"));

restart:
	cur_objgen = ++_rtld_objgen;
	SIMPLEQ_INIT(&initlist);
	_rtld_initlist_tsort(&initlist, 0);

	/* First pass: objects marked with DF_1_INITFIRST. */
	SIMPLEQ_FOREACH(elm, &initlist, link) {
		Obj_Entry * const obj = elm->obj;
		if (obj->z_initfirst) {
			_rtld_call_init_function(obj, mask, cur_objgen);
			if (_rtld_objgen != cur_objgen) {
				dbg(("restarting init iteration"));
				_rtld_objlist_clear(&initlist);
				goto restart;
			}
		}
	}

	/* Second pass: all other objects. */
	SIMPLEQ_FOREACH(elm, &initlist, link) {
		_rtld_call_init_function(elm->obj, mask, cur_objgen);
		if (_rtld_objgen != cur_objgen) {
			dbg(("restarting init iteration"));
			_rtld_objlist_clear(&initlist);
			goto restart;
		}
	}

        _rtld_objlist_clear(&initlist);
}

/*
 * Initialize the dynamic linker.  The argument is the address at which
 * the dynamic linker has been mapped into memory.  The primary task of
 * this function is to create an Obj_Entry for the dynamic linker and
 * to resolve the PLT relocation for platforms that need it (those that
 * define __HAVE_FUNCTION_DESCRIPTORS
 */
static void
_rtld_init(caddr_t mapbase, caddr_t relocbase, const char *execname)
{

	/* Conjure up an Obj_Entry structure for the dynamic linker. */
	_rtld_objself.path = __UNCONST(_rtld_path);
	_rtld_objself.pathlen = sizeof(_rtld_path)-1;
	_rtld_objself.rtld = true;
	_rtld_objself.mapbase = mapbase;
	_rtld_objself.relocbase = relocbase;
	_rtld_objself.dynamic = (Elf_Dyn *) &_DYNAMIC;
	_rtld_objself.strtab = "_rtld_sym_zero";

	/*
	 * Set value to -relocbase so that
	 *
	 *     _rtld_objself.relocbase + _rtld_sym_zero.st_value == 0
	 *
	 * This allows unresolved references to weak symbols to be computed
	 * to a value of 0.
	 */
	_rtld_sym_zero.st_value = -(uintptr_t)relocbase;

	_rtld_digest_dynamic(_rtld_path, &_rtld_objself);
	assert(!_rtld_objself.needed);
#if !defined(__hppa__)
	assert(!_rtld_objself.pltrel && !_rtld_objself.pltrela);
#else
	_rtld_relocate_plt_objects(&_rtld_objself);
#endif
#if !defined(__mips__) && !defined(__hppa__)
	assert(!_rtld_objself.pltgot);
#endif
#if !defined(__arm__) && !defined(__mips__) && !defined(__sh__)
	/* ARM, MIPS and SH{3,5} have a bogus DT_TEXTREL. */
	assert(!_rtld_objself.textrel);
#endif

	_rtld_add_paths(execname, &_rtld_default_paths,
	    RTLD_DEFAULT_LIBRARY_PATH);

#ifdef RTLD_ARCH_SUBDIR
	_rtld_add_paths(execname, &_rtld_default_paths,
	    RTLD_DEFAULT_LIBRARY_PATH "/" RTLD_ARCH_SUBDIR);
#endif

	/* Make the object list empty. */
	_rtld_objlist = NULL;
	_rtld_objtail = &_rtld_objlist;
	_rtld_objcount = 0;

	_rtld_debug.r_brk = _rtld_debug_state;
	_rtld_debug.r_state = RT_CONSISTENT;
}

/*
 * Cleanup procedure.  It will be called (by the atexit() mechanism) just
 * before the process exits.
 */
static void
_rtld_exit(void)
{
	sigset_t mask;

	dbg(("rtld_exit()"));

	_rtld_exclusive_enter(&mask);

	_rtld_call_fini_functions(&mask, 1);

	_rtld_exclusive_exit(&mask);
}

__dso_public void *
_dlauxinfo(void)
{
	return auxinfo;
}

/*
 * Main entry point for dynamic linking.  The argument is the stack
 * pointer.  The stack is expected to be laid out as described in the
 * SVR4 ABI specification, Intel 386 Processor Supplement.  Specifically,
 * the stack pointer points to a word containing ARGC.  Following that
 * in the stack is a null-terminated sequence of pointers to argument
 * strings.  Then comes a null-terminated sequence of pointers to
 * environment strings.  Finally, there is a sequence of "auxiliary
 * vector" entries.
 *
 * This function returns the entry point for the main program, the dynamic
 * linker's exit procedure in sp[0], and a pointer to the main object in
 * sp[1].
 */
Elf_Addr
_rtld(Elf_Addr *sp, Elf_Addr relocbase)
{
	const AuxInfo  *pAUX_base, *pAUX_entry, *pAUX_execfd, *pAUX_phdr,
	               *pAUX_phent, *pAUX_phnum, *pAUX_euid, *pAUX_egid,
		       *pAUX_ruid, *pAUX_rgid;
	const AuxInfo  *pAUX_pagesz;
	char          **env, **oenvp;
	const AuxInfo  *auxp;
	Obj_Entry      *obj;
	Elf_Addr       *const osp = sp;
	bool            bind_now = 0;
	const char     *ld_bind_now, *ld_preload, *ld_library_path;
	const char    **argv;
	const char     *execname;
	long		argc;
	const char **real___progname;
	const Obj_Entry **real___mainprog_obj;
	char ***real_environ;
	sigset_t        mask;
#ifdef DEBUG
	const char     *ld_debug;
#endif
#ifdef RTLD_DEBUG
	int i = 0;
#endif

	/*
         * On entry, the dynamic linker itself has not been relocated yet.
         * Be very careful not to reference any global data until after
         * _rtld_init has returned.  It is OK to reference file-scope statics
         * and string constants, and to call static and global functions.
         */
	/* Find the auxiliary vector on the stack. */
	/* first Elf_Word reserved to address of exit routine */
#if defined(RTLD_DEBUG)
	debug = 1;
	dbg(("sp = %p, argc = %ld, argv = %p <%s> relocbase %p", sp,
	    (long)sp[2], &sp[3], (char *) sp[3], (void *)relocbase));
#ifndef __x86_64__
	dbg(("got is at %p, dynamic is at %p", _GLOBAL_OFFSET_TABLE_,
	    &_DYNAMIC));
#endif
#endif

	sp += 2;		/* skip over return argument space */
	argv = (const char **) &sp[1];
	argc = *(long *)sp;
	sp += 2 + argc;		/* Skip over argc, arguments, and NULL
				 * terminator */
	env = (char **) sp;
	while (*sp++ != 0) {	/* Skip over environment, and NULL terminator */
#if defined(RTLD_DEBUG)
		dbg(("env[%d] = %p %s", i++, (void *)sp[-1], (char *)sp[-1]));
#endif
	}
	auxinfo = (AuxInfo *) sp;

	pAUX_base = pAUX_entry = pAUX_execfd = NULL;
	pAUX_phdr = pAUX_phent = pAUX_phnum = NULL;
	pAUX_euid = pAUX_ruid = pAUX_egid = pAUX_rgid = NULL;
	pAUX_pagesz = NULL;

	execname = NULL;

	/* Digest the auxiliary vector. */
	for (auxp = auxinfo; auxp->a_type != AT_NULL; ++auxp) {
		switch (auxp->a_type) {
		case AT_BASE:
			pAUX_base = auxp;
			break;
		case AT_ENTRY:
			pAUX_entry = auxp;
			break;
		case AT_EXECFD:
			pAUX_execfd = auxp;
			break;
		case AT_PHDR:
			pAUX_phdr = auxp;
			break;
		case AT_PHENT:
			pAUX_phent = auxp;
			break;
		case AT_PHNUM:
			pAUX_phnum = auxp;
			break;
#ifdef AT_EUID
		case AT_EUID:
			pAUX_euid = auxp;
			break;
		case AT_RUID:
			pAUX_ruid = auxp;
			break;
		case AT_EGID:
			pAUX_egid = auxp;
			break;
		case AT_RGID:
			pAUX_rgid = auxp;
			break;
#endif
#ifdef AT_SUN_EXECNAME
		case AT_SUN_EXECNAME:
			execname = (const char *)(const void *)auxp->a_v;
			break;
#endif
		case AT_PAGESZ:
			pAUX_pagesz = auxp;
			break;
		}
	}

	/* Initialize and relocate ourselves. */
	if (pAUX_base == NULL) {
		_rtld_error("Bad pAUX_base");
		_rtld_die();
	}
	assert(pAUX_pagesz != NULL);
	_rtld_pagesz = (int)pAUX_pagesz->a_v;
	_rtld_init((caddr_t)pAUX_base->a_v, (caddr_t)relocbase, execname);

	__progname = _rtld_objself.path;
	environ = env;

	_rtld_trust = ((pAUX_euid ? (uid_t)pAUX_euid->a_v : geteuid()) ==
	    (pAUX_ruid ? (uid_t)pAUX_ruid->a_v : getuid())) &&
	    ((pAUX_egid ? (gid_t)pAUX_egid->a_v : getegid()) ==
	    (pAUX_rgid ? (gid_t)pAUX_rgid->a_v : getgid()));

#ifdef DEBUG
	ld_debug = NULL;
#endif
	ld_bind_now = NULL;
	ld_library_path = NULL;
	ld_preload = NULL;
	/*
	 * Inline avoid using normal getenv/unsetenv here as the libc
	 * code is quite a bit more complicated.
	 */
	for (oenvp = env; *env != NULL; ++env) {
		static const char bind_var[] = "LD_BIND_NOW=";
		static const char debug_var[] =  "LD_DEBUG=";
		static const char path_var[] = "LD_LIBRARY_PATH=";
		static const char preload_var[] = "LD_PRELOAD=";
#define LEN(x)	(sizeof(x) - 1)

		if ((*env)[0] != 'L' || (*env)[1] != 'D') {
			/*
			 * Special case to skip most entries without
			 * the more expensive calls to strncmp.
			 */
			*oenvp++ = *env;
		} else if (strncmp(*env, debug_var, LEN(debug_var)) == 0) {
			if (_rtld_trust) {
#ifdef DEBUG
				ld_debug = *env + LEN(debug_var);
#endif
				*oenvp++ = *env;
			}
		} else if (strncmp(*env, bind_var, LEN(bind_var)) == 0) {
			if (_rtld_trust) {
				ld_bind_now = *env + LEN(bind_var);
				*oenvp++ = *env;
			}
		} else if (strncmp(*env, path_var, LEN(path_var)) == 0) {
			if (_rtld_trust) {
				ld_library_path = *env + LEN(path_var);
				*oenvp++ = *env;
			}
		} else if (strncmp(*env, preload_var, LEN(preload_var)) == 0) {
			if (_rtld_trust) {
				ld_preload = *env + LEN(preload_var);
				*oenvp++ = *env;
			}
		} else {
			*oenvp++ = *env;
		}
#undef LEN
	}
	*oenvp++ = NULL;

	if (ld_bind_now != NULL && *ld_bind_now != '\0')
		bind_now = true;
	if (_rtld_trust) {
#ifdef DEBUG
#ifdef RTLD_DEBUG
		debug = 0;
#endif
		if (ld_debug != NULL && *ld_debug != '\0')
			debug = 1;
#endif
		_rtld_add_paths(execname, &_rtld_paths, ld_library_path);
	} else {
		execname = NULL;
	}
	_rtld_process_hints(execname, &_rtld_paths, &_rtld_xforms,
	    _PATH_LD_HINTS);
	dbg(("dynamic linker is initialized, mapbase=%p, relocbase=%p",
	     _rtld_objself.mapbase, _rtld_objself.relocbase));

	/*
         * Load the main program, or process its program header if it is
         * already loaded.
         */
	if (pAUX_execfd != NULL) {	/* Load the main program. */
		int             fd = pAUX_execfd->a_v;
		const char *obj_name = argv[0] ? argv[0] : "main program";
		dbg(("loading main program"));
		_rtld_objmain = _rtld_map_object(obj_name, fd, NULL);
		close(fd);
		if (_rtld_objmain == NULL)
			_rtld_die();
	} else {		/* Main program already loaded. */
		const Elf_Phdr *phdr;
		int             phnum;
		caddr_t         entry;

		dbg(("processing main program's program header"));
		assert(pAUX_phdr != NULL);
		phdr = (const Elf_Phdr *) pAUX_phdr->a_v;
		assert(pAUX_phnum != NULL);
		phnum = pAUX_phnum->a_v;
		assert(pAUX_phent != NULL);
		assert(pAUX_phent->a_v == sizeof(Elf_Phdr));
		assert(pAUX_entry != NULL);
		entry = (caddr_t) pAUX_entry->a_v;
		_rtld_objmain = _rtld_digest_phdr(phdr, phnum, entry);
		_rtld_objmain->path = xstrdup(argv[0] ? argv[0] :
		    "main program");
		_rtld_objmain->pathlen = strlen(_rtld_objmain->path);
	}

	_rtld_objmain->mainprog = true;

	/*
	 * Get the actual dynamic linker pathname from the executable if
	 * possible.  (It should always be possible.)  That ensures that
	 * gdb will find the right dynamic linker even if a non-standard
	 * one is being used.
	 */
	if (_rtld_objmain->interp != NULL &&
	    strcmp(_rtld_objmain->interp, _rtld_objself.path) != 0) {
		_rtld_objself.path = xstrdup(_rtld_objmain->interp);
		_rtld_objself.pathlen = strlen(_rtld_objself.path);
	}
	dbg(("actual dynamic linker is %s", _rtld_objself.path));

	_rtld_digest_dynamic(execname, _rtld_objmain);

	/* Link the main program into the list of objects. */
	*_rtld_objtail = _rtld_objmain;
	_rtld_objtail = &_rtld_objmain->next;
	_rtld_objcount++;
	_rtld_objloads++;

	_rtld_linkmap_add(_rtld_objmain);
	_rtld_linkmap_add(&_rtld_objself);

	++_rtld_objmain->refcount;
	_rtld_objmain->mainref = 1;
	_rtld_objlist_push_tail(&_rtld_list_main, _rtld_objmain);

	if (ld_preload) {
		/*
		 * Pre-load user-specified objects after the main program
		 * but before any shared object dependencies.
		 */
		dbg(("preloading objects"));
		if (_rtld_preload(ld_preload) == -1)
			_rtld_die();
	}

	dbg(("loading needed objects"));
	if (_rtld_load_needed_objects(_rtld_objmain, _RTLD_MAIN) == -1)
		_rtld_die();

	dbg(("checking for required versions"));
	for (obj = _rtld_objlist; obj != NULL; obj = obj->next) {
		if (_rtld_verify_object_versions(obj) == -1)
			_rtld_die();
	}

#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
	dbg(("initializing initial Thread Local Storage offsets"));
	/*
	 * All initial objects get the TLS space from the static block.
	 */
	for (obj = _rtld_objlist; obj != NULL; obj = obj->next)
		_rtld_tls_offset_allocate(obj);
#endif

	dbg(("relocating objects"));
	if (_rtld_relocate_objects(_rtld_objmain, bind_now) == -1)
		_rtld_die();

	dbg(("doing copy relocations"));
	if (_rtld_do_copy_relocations(_rtld_objmain) == -1)
		_rtld_die();

#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
	dbg(("initializing Thread Local Storage for main thread"));
	/*
	 * Set up TLS area for the main thread.
	 * This has to be done after all relocations are processed,
	 * since .tdata may contain relocations.
	 */
	_rtld_tls_initial_allocation();
#endif

	/*
	 * Set the __progname,  environ and, __mainprog_obj before
	 * calling anything that might use them.
	 */
	real___progname = _rtld_objmain_sym("__progname");
	if (real___progname) {
		if (argv[0] != NULL) {
			if ((*real___progname = strrchr(argv[0], '/')) == NULL)
				(*real___progname) = argv[0];
			else
				(*real___progname)++;
		} else {
			(*real___progname) = NULL;
		}
	}
	real_environ = _rtld_objmain_sym("environ");
	if (real_environ)
		*real_environ = environ;
	/*
	 * Set __mainprog_obj for old binaries.
	 */
	real___mainprog_obj = _rtld_objmain_sym("__mainprog_obj");
	if (real___mainprog_obj)
		*real___mainprog_obj = _rtld_objmain;

	_rtld_exclusive_enter(&mask);

	dbg(("calling _init functions"));
	_rtld_call_init_functions(&mask);

	dbg(("control at program entry point = %p, obj = %p, exit = %p",
	     _rtld_objmain->entry, _rtld_objmain, _rtld_exit));

	_rtld_exclusive_exit(&mask);

	/*
	 * Return with the entry point and the exit procedure in at the top
	 * of stack.
	 */

	_rtld_debug_state();	/* say hello to gdb! */

	((void **) osp)[0] = _rtld_exit;
	((void **) osp)[1] = _rtld_objmain;
	return (Elf_Addr) _rtld_objmain->entry;
}

void
_rtld_die(void)
{
	const char *msg = dlerror();

	if (msg == NULL)
		msg = "Fatal error";
	xerrx(1, "%s", msg);
}

static Obj_Entry *
_rtld_dlcheck(void *handle)
{
	Obj_Entry *obj;

	for (obj = _rtld_objlist; obj != NULL; obj = obj->next)
		if (obj == (Obj_Entry *) handle)
			break;

	if (obj == NULL || obj->dl_refcount == 0) {
		_rtld_error("Invalid shared object handle %p", handle);
		return NULL;
	}
	return obj;
}

static void
_rtld_initlist_visit(Objlist* list, Obj_Entry *obj, int rev)
{
	Needed_Entry* elm;

	/* dbg(("_rtld_initlist_visit(%s)", obj->path)); */

	if (obj->init_done)
		return;
	obj->init_done = 1;

	for (elm = obj->needed; elm != NULL; elm = elm->next) {
		if (elm->obj != NULL) {
			_rtld_initlist_visit(list, elm->obj, rev);
		}
	}

	if (rev) {
		_rtld_objlist_push_head(list, obj);
	} else {
		_rtld_objlist_push_tail(list, obj);
	}
}

static void
_rtld_initlist_tsort(Objlist* list, int rev)
{
	dbg(("_rtld_initlist_tsort"));

	Obj_Entry* obj;

	for (obj = _rtld_objlist->next; obj; obj = obj->next) {
		obj->init_done = 0;
	}

	for (obj = _rtld_objlist->next; obj; obj = obj->next) {
		_rtld_initlist_visit(list, obj, rev);
	}
}

static void
_rtld_init_dag(Obj_Entry *root)
{

	_rtld_init_dag1(root, root);
}

static void
_rtld_init_dag1(Obj_Entry *root, Obj_Entry *obj)
{
	const Needed_Entry *needed;

	if (!obj->mainref) {
		if (_rtld_objlist_find(&obj->dldags, root))
			return;
		dbg(("add %p (%s) to %p (%s) DAG", obj, obj->path, root,
		    root->path));
		_rtld_objlist_push_tail(&obj->dldags, root);
		_rtld_objlist_push_tail(&root->dagmembers, obj);
	}
	for (needed = obj->needed; needed != NULL; needed = needed->next)
		if (needed->obj != NULL)
			_rtld_init_dag1(root, needed->obj);
}

/*
 * Note, this is called only for objects loaded by dlopen().
 */
static void
_rtld_unload_object(sigset_t *mask, Obj_Entry *root, bool do_fini_funcs)
{

	_rtld_unref_dag(root);
	if (root->refcount == 0) { /* We are finished with some objects. */
		Obj_Entry *obj;
		Obj_Entry **linkp;
		Objlist_Entry *elm;

		/* Finalize objects that are about to be unmapped. */
		if (do_fini_funcs)
			_rtld_call_fini_functions(mask, 0);

		/* Remove the DAG from all objects' DAG lists. */
		SIMPLEQ_FOREACH(elm, &root->dagmembers, link)
			_rtld_objlist_remove(&elm->obj->dldags, root);

		/* Remove the DAG from the RTLD_GLOBAL list. */
		if (root->globalref) {
			root->globalref = 0;
			_rtld_objlist_remove(&_rtld_list_global, root);
		}

		/* Unmap all objects that are no longer referenced. */
		linkp = &_rtld_objlist->next;
		while ((obj = *linkp) != NULL) {
			if (obj->refcount == 0) {
				dbg(("unloading \"%s\"", obj->path));
				if (obj->ehdr != MAP_FAILED)
					munmap(obj->ehdr, _rtld_pagesz);
				munmap(obj->mapbase, obj->mapsize);
				_rtld_objlist_remove(&_rtld_list_global, obj);
				_rtld_linkmap_delete(obj);
				*linkp = obj->next;
				_rtld_objcount--;
				_rtld_obj_free(obj);
			} else
				linkp = &obj->next;
		}
		_rtld_objtail = linkp;
	}
}

void
_rtld_ref_dag(Obj_Entry *root)
{
	const Needed_Entry *needed;

	assert(root);

	++root->refcount;

	dbg(("incremented reference on \"%s\" (%d)", root->path,
	    root->refcount));
	for (needed = root->needed; needed != NULL;
	     needed = needed->next) {
		if (needed->obj != NULL)
			_rtld_ref_dag(needed->obj);
	}
}

static void
_rtld_unref_dag(Obj_Entry *root)
{

	assert(root);
	assert(root->refcount != 0);

	--root->refcount;
	dbg(("decremented reference on \"%s\" (%d)", root->path,
	    root->refcount));

	if (root->refcount == 0) {
		const Needed_Entry *needed;

		for (needed = root->needed; needed != NULL;
		     needed = needed->next) {
			if (needed->obj != NULL)
				_rtld_unref_dag(needed->obj);
		}
	}
}

__strong_alias(__dlclose,dlclose)
int
dlclose(void *handle)
{
	Obj_Entry *root;
	sigset_t mask;

	dbg(("dlclose of %p", handle));

	_rtld_exclusive_enter(&mask);

	root = _rtld_dlcheck(handle);

	if (root == NULL) {
		_rtld_exclusive_exit(&mask);
		return -1;
	}

	_rtld_debug.r_state = RT_DELETE;
	_rtld_debug_state();

	--root->dl_refcount;
	_rtld_unload_object(&mask, root, true);

	_rtld_debug.r_state = RT_CONSISTENT;
	_rtld_debug_state();

	_rtld_exclusive_exit(&mask);

	return 0;
}

__strong_alias(__dlerror,dlerror)
char *
dlerror(void)
{
	char *msg = error_message;

	error_message = NULL;
	return msg;
}

__strong_alias(__dlopen,dlopen)
void *
dlopen(const char *name, int mode)
{
	Obj_Entry **old_obj_tail = _rtld_objtail;
	Obj_Entry *obj = NULL;
	int flags = _RTLD_DLOPEN;
	bool nodelete;
	bool now;
	sigset_t mask;
	int result;

	dbg(("dlopen of %s %d", name, mode));

	_rtld_exclusive_enter(&mask);

	flags |= (mode & RTLD_GLOBAL) ? _RTLD_GLOBAL : 0;
	flags |= (mode & RTLD_NOLOAD) ? _RTLD_NOLOAD : 0;

	nodelete = (mode & RTLD_NODELETE) ? true : false;
	now = ((mode & RTLD_MODEMASK) == RTLD_NOW) ? true : false;

	_rtld_debug.r_state = RT_ADD;
	_rtld_debug_state();

	if (name == NULL) {
		obj = _rtld_objmain;
		obj->refcount++;
	} else
		obj = _rtld_load_library(name, _rtld_objmain, flags);


	if (obj != NULL) {
		++obj->dl_refcount;
		if (*old_obj_tail != NULL) {	/* We loaded something new. */
			assert(*old_obj_tail == obj);

			result = _rtld_load_needed_objects(obj, flags);
			if (result != -1) {
				Objlist_Entry *entry;
				_rtld_init_dag(obj);
				SIMPLEQ_FOREACH(entry, &obj->dagmembers, link) {
					result = _rtld_verify_object_versions(entry->obj);
					if (result == -1)
						break;
				}
			}
			if (result == -1 || _rtld_relocate_objects(obj,
			    (now || obj->z_now)) == -1) {
				_rtld_unload_object(&mask, obj, false);
				obj->dl_refcount--;
				obj = NULL;
			} else {
				_rtld_call_init_functions(&mask);
			}
		}
		if (obj != NULL) {
			if ((nodelete || obj->z_nodelete) && !obj->ref_nodel) {
				dbg(("dlopen obj %s nodelete", obj->path));
				_rtld_ref_dag(obj);
				obj->z_nodelete = obj->ref_nodel = true;
			}
		}
	}
	_rtld_debug.r_state = RT_CONSISTENT;
	_rtld_debug_state();

	_rtld_exclusive_exit(&mask);

	return obj;
}

/*
 * Find a symbol in the main program.
 */
void *
_rtld_objmain_sym(const char *name)
{
	unsigned long hash;
	const Elf_Sym *def;
	const Obj_Entry *obj;
	DoneList donelist;

	hash = _rtld_elf_hash(name);
	obj = _rtld_objmain;
	_rtld_donelist_init(&donelist);

	def = _rtld_symlook_list(name, hash, &_rtld_list_main, &obj, 0,
	    NULL, &donelist);

	if (def != NULL)
		return obj->relocbase + def->st_value;
	return NULL;
}

#ifdef __powerpc__
static void *
hackish_return_address(void)
{
	return __builtin_return_address(1);
}
#endif

#ifdef __HAVE_FUNCTION_DESCRIPTORS
#define	lookup_mutex_enter()	_rtld_exclusive_enter(&mask)
#define	lookup_mutex_exit()	_rtld_exclusive_exit(&mask)
#else
#define	lookup_mutex_enter()	_rtld_shared_enter()
#define	lookup_mutex_exit()	_rtld_shared_exit()
#endif

static void *
do_dlsym(void *handle, const char *name, const Ver_Entry *ventry, void *retaddr)
{
	const Obj_Entry *obj;
	unsigned long hash;
	const Elf_Sym *def;
	const Obj_Entry *defobj;
	DoneList donelist;
	const u_int flags = SYMLOOK_DLSYM | SYMLOOK_IN_PLT;
#ifdef __HAVE_FUNCTION_DESCRIPTORS
	sigset_t mask;
#endif

	lookup_mutex_enter();

	hash = _rtld_elf_hash(name);
	def = NULL;
	defobj = NULL;

	switch ((intptr_t)handle) {
	case (intptr_t)NULL:
	case (intptr_t)RTLD_NEXT:
	case (intptr_t)RTLD_DEFAULT:
	case (intptr_t)RTLD_SELF:
		if ((obj = _rtld_obj_from_addr(retaddr)) == NULL) {
			_rtld_error("Cannot determine caller's shared object");
			lookup_mutex_exit();
			return NULL;
		}

		switch ((intptr_t)handle) {
		case (intptr_t)NULL:	 /* Just the caller's shared object. */
			def = _rtld_symlook_obj(name, hash, obj, flags, ventry);
			defobj = obj;
			break;

		case (intptr_t)RTLD_NEXT:	/* Objects after callers */
			obj = obj->next;
			/*FALLTHROUGH*/

		case (intptr_t)RTLD_SELF:	/* Caller included */
			for (; obj; obj = obj->next) {
				if ((def = _rtld_symlook_obj(name, hash, obj,
				    flags, ventry)) != NULL) {
					defobj = obj;
					break;
				}
			}
			break;

		case (intptr_t)RTLD_DEFAULT:
			def = _rtld_symlook_default(name, hash, obj, &defobj,
			    flags, ventry);
			break;

		default:
			abort();
		}
		break;

	default:
		if ((obj = _rtld_dlcheck(handle)) == NULL) {
			lookup_mutex_exit();
			return NULL;
		}

		_rtld_donelist_init(&donelist);

		if (obj->mainprog) {
			/* Search main program and all libraries loaded by it */
			def = _rtld_symlook_list(name, hash, &_rtld_list_main,
			    &defobj, flags, ventry, &donelist);
		} else {
			Needed_Entry fake;
			DoneList depth;

			/* Search the object and all the libraries loaded by it. */
			fake.next = NULL;
			fake.obj = __UNCONST(obj);
			fake.name = 0;

			_rtld_donelist_init(&depth);
			def = _rtld_symlook_needed(name, hash, &fake, &defobj,
			    flags, ventry, &donelist, &depth);
		}

		break;
	}

	if (def != NULL) {
		void *p;

		if (ELF_ST_TYPE(def->st_info) == STT_GNU_IFUNC) {
#ifdef __HAVE_FUNCTION_DESCRIPTORS
			lookup_mutex_exit();
			_rtld_shared_enter();
#endif
			p = (void *)_rtld_resolve_ifunc(defobj, def);
			_rtld_shared_exit();
			return p;
		}

#ifdef __HAVE_FUNCTION_DESCRIPTORS
		if (ELF_ST_TYPE(def->st_info) == STT_FUNC) {
			p = (void *)_rtld_function_descriptor_alloc(defobj,
			    def, 0);
			lookup_mutex_exit();
			return p;
		}
#endif /* __HAVE_FUNCTION_DESCRIPTORS */
		p = defobj->relocbase + def->st_value;
		lookup_mutex_exit();
		return p;
	}

	_rtld_error("Undefined symbol \"%s\"", name);
	lookup_mutex_exit();
	return NULL;
}

__strong_alias(__dlsym,dlsym)
void *
dlsym(void *handle, const char *name)
{
	void *retaddr;

	dbg(("dlsym of %s in %p", name, handle));

#ifdef __powerpc__
	retaddr = hackish_return_address();
#else
	retaddr = __builtin_return_address(0);
#endif
	return do_dlsym(handle, name, NULL, retaddr);
}

__strong_alias(__dlvsym,dlvsym)
void *
dlvsym(void *handle, const char *name, const char *version)
{
	Ver_Entry *ventry = NULL;
	Ver_Entry ver_entry;
	void *retaddr;

	dbg(("dlvsym of %s@%s in %p", name, version ? version : NULL, handle));

	if (version != NULL) {
		ver_entry.name = version;
		ver_entry.file = NULL;
		ver_entry.hash = _rtld_elf_hash(version);
		ver_entry.flags = 0;
		ventry = &ver_entry;
	}
#ifdef __powerpc__
	retaddr = hackish_return_address();
#else
	retaddr = __builtin_return_address(0);
#endif
	return do_dlsym(handle, name, ventry, retaddr);
}

__strong_alias(__dladdr,dladdr)
int
dladdr(const void *addr, Dl_info *info)
{
	const Obj_Entry *obj;
	const Elf_Sym *def, *best_def;
	void *symbol_addr;
	unsigned long symoffset;
#ifdef __HAVE_FUNCTION_DESCRIPTORS
	sigset_t mask;
#endif

	dbg(("dladdr of %p", addr));

	lookup_mutex_enter();

#ifdef __HAVE_FUNCTION_DESCRIPTORS
	addr = _rtld_function_descriptor_function(addr);
#endif /* __HAVE_FUNCTION_DESCRIPTORS */

	obj = _rtld_obj_from_addr(addr);
	if (obj == NULL) {
		_rtld_error("No shared object contains address");
		lookup_mutex_exit();
		return 0;
	}
	info->dli_fname = obj->path;
	info->dli_fbase = obj->mapbase;
	info->dli_saddr = (void *)0;
	info->dli_sname = NULL;

	/*
	 * Walk the symbol list looking for the symbol whose address is
	 * closest to the address sent in.
	 */
	best_def = NULL;
	for (symoffset = 0; symoffset < obj->nchains; symoffset++) {
		def = obj->symtab + symoffset;

		/*
		 * For skip the symbol if st_shndx is either SHN_UNDEF or
		 * SHN_COMMON.
		 */
		if (def->st_shndx == SHN_UNDEF || def->st_shndx == SHN_COMMON)
			continue;

		/*
		 * If the symbol is greater than the specified address, or if it
		 * is further away from addr than the current nearest symbol,
		 * then reject it.
		 */
		symbol_addr = obj->relocbase + def->st_value;
		if (symbol_addr > addr || symbol_addr < info->dli_saddr)
			continue;

		/* Update our idea of the nearest symbol. */
		info->dli_sname = obj->strtab + def->st_name;
		info->dli_saddr = symbol_addr;
		best_def = def;


		/* Exact match? */
		if (info->dli_saddr == addr)
			break;
	}

#ifdef __HAVE_FUNCTION_DESCRIPTORS
	if (best_def != NULL && ELF_ST_TYPE(best_def->st_info) == STT_FUNC)
		info->dli_saddr = (void *)_rtld_function_descriptor_alloc(obj,
		    best_def, 0);
#else
	__USE(best_def);
#endif /* __HAVE_FUNCTION_DESCRIPTORS */

	lookup_mutex_exit();
	return 1;
}

__strong_alias(__dlinfo,dlinfo)
int
dlinfo(void *handle, int req, void *v)
{
	const Obj_Entry *obj;
	void *retaddr;

	dbg(("dlinfo for %p %d", handle, req));

	_rtld_shared_enter();

	if (handle == RTLD_SELF) {
#ifdef __powerpc__
		retaddr = hackish_return_address();
#else
		retaddr = __builtin_return_address(0);
#endif
		if ((obj = _rtld_obj_from_addr(retaddr)) == NULL) {
			_rtld_error("Cannot determine caller's shared object");
			_rtld_shared_exit();
			return -1;
		}
	} else {
		if ((obj = _rtld_dlcheck(handle)) == NULL) {
			_rtld_shared_exit();
			return -1;
		}
	}

	switch (req) {
	case RTLD_DI_LINKMAP:
		{
		const struct link_map **map = v;

		*map = &obj->linkmap;
		break;
		}

	default:
		_rtld_error("Invalid request");
		_rtld_shared_exit();
		return -1;
	}

	_rtld_shared_exit();
	return 0;
}

__strong_alias(__dl_iterate_phdr,dl_iterate_phdr);
int
dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *), void *param)
{
	struct dl_phdr_info phdr_info;
	const Obj_Entry *obj;
	int error = 0;

	dbg(("dl_iterate_phdr"));

	_rtld_shared_enter();

	for (obj = _rtld_objlist;  obj != NULL;  obj = obj->next) {
		phdr_info.dlpi_addr = (Elf_Addr)obj->relocbase;
		/* XXX: wrong but not fixing it yet */
		phdr_info.dlpi_name = SIMPLEQ_FIRST(&obj->names) ?
		    SIMPLEQ_FIRST(&obj->names)->name : obj->path;
		phdr_info.dlpi_phdr = obj->phdr;
		phdr_info.dlpi_phnum = obj->phsize / sizeof(obj->phdr[0]);
#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
		phdr_info.dlpi_tls_modid = obj->tlsindex;
		phdr_info.dlpi_tls_data = obj->tlsinit;
#else
		phdr_info.dlpi_tls_modid = 0;
		phdr_info.dlpi_tls_data = 0;
#endif
		phdr_info.dlpi_adds = _rtld_objloads;
		phdr_info.dlpi_subs = _rtld_objloads - _rtld_objcount;

		/* XXXlocking: exit point */
		error = callback(&phdr_info, sizeof(phdr_info), param);
		if (error)
			break;
	}

	_rtld_shared_exit();
	return error;
}

/*
 * Error reporting function.  Use it like printf.  If formats the message
 * into a buffer, and sets things up so that the next call to dlerror()
 * will return the message.
 */
void
_rtld_error(const char *fmt,...)
{
	static char     buf[512];
	va_list         ap;

	va_start(ap, fmt);
	xvsnprintf(buf, sizeof buf, fmt, ap);
	error_message = buf;
	va_end(ap);
}

void
_rtld_debug_state(void)
{
#if defined(__hppa__)
	__asm volatile("nop" ::: "memory");
#endif

	/* Prevent optimizer from removing calls to this function */
	__insn_barrier();
}

void
_rtld_linkmap_add(Obj_Entry *obj)
{
	struct link_map *l = &obj->linkmap;
	struct link_map *prev;

	obj->linkmap.l_name = obj->path;
	obj->linkmap.l_addr = obj->relocbase;
	obj->linkmap.l_ld = obj->dynamic;
#ifdef __mips__
	/* XXX This field is not standard and will be removed eventually. */
	obj->linkmap.l_offs = obj->relocbase;
#endif

	if (_rtld_debug.r_map == NULL) {
		_rtld_debug.r_map = l;
		return;
	}

	/*
	 * Scan to the end of the list, but not past the entry for the
	 * dynamic linker, which we want to keep at the very end.
	 */
	for (prev = _rtld_debug.r_map;
	    prev->l_next != NULL && prev->l_next != &_rtld_objself.linkmap;
	    prev = prev->l_next);

	l->l_prev = prev;
	l->l_next = prev->l_next;
	if (l->l_next != NULL)
		l->l_next->l_prev = l;
	prev->l_next = l;
}

void
_rtld_linkmap_delete(Obj_Entry *obj)
{
	struct link_map *l = &obj->linkmap;

	if (l->l_prev == NULL) {
		if ((_rtld_debug.r_map = l->l_next) != NULL)
			l->l_next->l_prev = NULL;
		return;
	}
	if ((l->l_prev->l_next = l->l_next) != NULL)
		l->l_next->l_prev = l->l_prev;
}

static Obj_Entry *
_rtld_obj_from_addr(const void *addr)
{
	Obj_Entry *obj;

	for (obj = _rtld_objlist;  obj != NULL;  obj = obj->next) {
		if (addr < (void *) obj->mapbase)
			continue;
		if (addr < (void *) (obj->mapbase + obj->mapsize))
			return obj;
	}
	return NULL;
}

static void
_rtld_objlist_clear(Objlist *list)
{
	while (!SIMPLEQ_EMPTY(list)) {
		Objlist_Entry* elm = SIMPLEQ_FIRST(list);
		SIMPLEQ_REMOVE_HEAD(list, link);
		xfree(elm);
	}
}

static void
_rtld_objlist_remove(Objlist *list, Obj_Entry *obj)
{
	Objlist_Entry *elm;

	if ((elm = _rtld_objlist_find(list, obj)) != NULL) {
		SIMPLEQ_REMOVE(list, elm, Struct_Objlist_Entry, link);
		xfree(elm);
	}
}

#if defined(__minix)
void _rtld_shared_enter(void) {}
void _rtld_shared_exit(void) {}
void _rtld_exclusive_enter(sigset_t *mask) {}
void _rtld_exclusive_exit(sigset_t *mask) {}
#else
#define	RTLD_EXCLUSIVE_MASK	0x80000000U
static volatile unsigned int _rtld_mutex;
static volatile unsigned int _rtld_waiter_exclusive;
static volatile unsigned int _rtld_waiter_shared;

void
_rtld_shared_enter(void)
{
	unsigned int cur;
	lwpid_t waiter, self = 0;

	membar_enter();

	for (;;) {
		cur = _rtld_mutex;
		/*
		 * First check if we are currently not exclusively locked.
		 */
		if ((cur & RTLD_EXCLUSIVE_MASK) == 0) {
			/* Yes, so increment use counter */
			if (atomic_cas_uint(&_rtld_mutex, cur, cur + 1) != cur)
				continue;
			membar_enter();
			return;
		}
		/*
		 * Someone has an exclusive lock.  Puts us on the waiter list.
		 */
		if (!self)
			self = _lwp_self();
		if (cur == (self | RTLD_EXCLUSIVE_MASK)) {
			if (_rtld_mutex_may_recurse)
				return;
			_rtld_error("dead lock detected");
			_rtld_die();
		}
		waiter = atomic_swap_uint(&_rtld_waiter_shared, self);
		/*
		 * Check for race against _rtld_exclusive_exit before sleeping.
		 */
		membar_sync();
		if ((_rtld_mutex & RTLD_EXCLUSIVE_MASK) ||
		    _rtld_waiter_exclusive)
			_lwp_park(CLOCK_REALTIME, 0, NULL, 0,
			    __UNVOLATILE(&_rtld_mutex), NULL);
		/* Try to remove us from the waiter list. */
		atomic_cas_uint(&_rtld_waiter_shared, self, 0);
		if (waiter)
			_lwp_unpark(waiter, __UNVOLATILE(&_rtld_mutex));
	}
}

void
_rtld_shared_exit(void)
{
	lwpid_t waiter;

	/*
	 * Shared lock taken after an exclusive lock.
	 * Just assume this is a partial recursion.
	 */
	if (_rtld_mutex & RTLD_EXCLUSIVE_MASK)
		return;

	/*
	 * Wakeup LWPs waiting for an exclusive lock if this is the last
	 * LWP on the shared lock.
	 */
	membar_exit();
	if (atomic_dec_uint_nv(&_rtld_mutex))
		return;
	membar_sync();
	if ((waiter = _rtld_waiter_exclusive) != 0)
		_lwp_unpark(waiter, __UNVOLATILE(&_rtld_mutex));
}

void
_rtld_exclusive_enter(sigset_t *mask)
{
	lwpid_t waiter, self = _lwp_self();
	unsigned int locked_value = (unsigned int)self | RTLD_EXCLUSIVE_MASK;
	unsigned int cur;
	sigset_t blockmask;

	sigfillset(&blockmask);
	sigdelset(&blockmask, SIGTRAP);	/* Allow the debugger */
	sigprocmask(SIG_BLOCK, &blockmask, mask);

	for (;;) {
		if (atomic_cas_uint(&_rtld_mutex, 0, locked_value) == 0) {
			membar_enter();
			break;
		}
		waiter = atomic_swap_uint(&_rtld_waiter_exclusive, self);
		membar_sync();
		cur = _rtld_mutex;
		if (cur == locked_value) {
			_rtld_error("dead lock detected");
			_rtld_die();
		}
		if (cur)
			_lwp_park(CLOCK_REALTIME, 0, NULL, 0,
			    __UNVOLATILE(&_rtld_mutex), NULL);
		atomic_cas_uint(&_rtld_waiter_exclusive, self, 0);
		if (waiter)
			_lwp_unpark(waiter, __UNVOLATILE(&_rtld_mutex));
	}
}

void
_rtld_exclusive_exit(sigset_t *mask)
{
	lwpid_t waiter;

	membar_exit();
	_rtld_mutex = 0;
	membar_sync();
	if ((waiter = _rtld_waiter_exclusive) != 0)
		_lwp_unpark(waiter, __UNVOLATILE(&_rtld_mutex));

	if ((waiter = _rtld_waiter_shared) != 0)
		_lwp_unpark(waiter, __UNVOLATILE(&_rtld_mutex));

	sigprocmask(SIG_SETMASK, mask, NULL);
}
#endif /* !defined(__minix) */
