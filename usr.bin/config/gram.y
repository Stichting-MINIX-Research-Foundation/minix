%{
/*	$NetBSD: gram.y,v 1.52 2015/09/01 13:42:48 uebayasi Exp $	*/

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
 *	from: @(#)gram.y	8.1 (Berkeley) 6/6/93
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: gram.y,v 1.52 2015/09/01 13:42:48 uebayasi Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "defs.h"
#include "sem.h"

#define	FORMAT(n) (((n).fmt == 8 && (n).val != 0) ? "0%llo" : \
    ((n).fmt == 16) ? "0x%llx" : "%lld")

#define	stop(s)	cfgerror(s), exit(1)

static	struct	config conf;	/* at most one active at a time */


/*
 * Allocation wrapper functions
 */
static void wrap_alloc(void *ptr, unsigned code);
static void wrap_continue(void);
static void wrap_cleanup(void);

/*
 * Allocation wrapper type codes
 */
#define WRAP_CODE_nvlist	1
#define WRAP_CODE_defoptlist	2
#define WRAP_CODE_loclist	3
#define WRAP_CODE_attrlist	4
#define WRAP_CODE_condexpr	5

/*
 * The allocation wrappers themselves
 */
#define DECL_ALLOCWRAP(t)	static struct t *wrap_mk_##t(struct t *arg)

DECL_ALLOCWRAP(nvlist);
DECL_ALLOCWRAP(defoptlist);
DECL_ALLOCWRAP(loclist);
DECL_ALLOCWRAP(attrlist);
DECL_ALLOCWRAP(condexpr);

/* allow shorter names */
#define wrap_mk_loc(p) wrap_mk_loclist(p)
#define wrap_mk_cx(p) wrap_mk_condexpr(p)

/*
 * Macros for allocating new objects
 */

/* old-style for struct nvlist */
#define	new0(n,s,p,i,x)	wrap_mk_nvlist(newnv(n, s, p, i, x))
#define	new_n(n)	new0(n, NULL, NULL, 0, NULL)
#define	new_nx(n, x)	new0(n, NULL, NULL, 0, x)
#define	new_ns(n, s)	new0(n, s, NULL, 0, NULL)
#define	new_si(s, i)	new0(NULL, s, NULL, i, NULL)
#define	new_nsi(n,s,i)	new0(n, s, NULL, i, NULL)
#define	new_np(n, p)	new0(n, NULL, p, 0, NULL)
#define	new_s(s)	new0(NULL, s, NULL, 0, NULL)
#define	new_p(p)	new0(NULL, NULL, p, 0, NULL)
#define	new_px(p, x)	new0(NULL, NULL, p, 0, x)
#define	new_sx(s, x)	new0(NULL, s, NULL, 0, x)
#define	new_nsx(n,s,x)	new0(n, s, NULL, 0, x)
#define	new_i(i)	new0(NULL, NULL, NULL, i, NULL)

/* new style, type-polymorphic; ordinary and for types with multiple flavors */
#define MK0(t)		wrap_mk_##t(mk_##t())
#define MK1(t, a0)	wrap_mk_##t(mk_##t(a0))
#define MK2(t, a0, a1)	wrap_mk_##t(mk_##t(a0, a1))
#define MK3(t, a0, a1, a2)	wrap_mk_##t(mk_##t(a0, a1, a2))

#define MKF0(t, f)		wrap_mk_##t(mk_##t##_##f())
#define MKF1(t, f, a0)		wrap_mk_##t(mk_##t##_##f(a0))
#define MKF2(t, f, a0, a1)	wrap_mk_##t(mk_##t##_##f(a0, a1))

/*
 * Data constructors
 */

static struct defoptlist *mk_defoptlist(const char *, const char *,
					const char *);
static struct loclist *mk_loc(const char *, const char *, long long);
static struct loclist *mk_loc_val(const char *, struct loclist *);
static struct attrlist *mk_attrlist(struct attrlist *, struct attr *);
static struct condexpr *mk_cx_atom(const char *);
static struct condexpr *mk_cx_not(struct condexpr *);
static struct condexpr *mk_cx_and(struct condexpr *, struct condexpr *);
static struct condexpr *mk_cx_or(struct condexpr *, struct condexpr *);

/*
 * Other private functions
 */

static	void	setmachine(const char *, const char *, struct nvlist *, int);
static	void	check_maxpart(void);

static struct loclist *present_loclist(struct loclist *ll);
static void app(struct loclist *, struct loclist *);
static struct loclist *locarray(const char *, int, struct loclist *, int);
static struct loclist *namelocvals(const char *, struct loclist *);

%}

%union {
	struct	attr *attr;
	struct	devbase *devb;
	struct	deva *deva;
	struct	nvlist *list;
	struct defoptlist *defoptlist;
	struct loclist *loclist;
	struct attrlist *attrlist;
	struct condexpr *condexpr;
	const char *str;
	struct	numconst num;
	int64_t	val;
	u_char	flag;
	devmajor_t devmajor;
	int32_t i32;
}

%token	AND AT ATTACH
%token	BLOCK BUILD
%token	CHAR COLONEQ COMPILE_WITH CONFIG
%token	DEFFS DEFINE DEFOPT DEFPARAM DEFFLAG DEFPSEUDO DEFPSEUDODEV
%token	DEVICE DEVCLASS DUMPS DEVICE_MAJOR
%token	ENDFILE
%token	XFILE FILE_SYSTEM FLAGS
%token	IDENT IOCONF
%token	LINKZERO
%token	XMACHINE MAJOR MAKEOPTIONS MAXUSERS MAXPARTITIONS MINOR
%token	NEEDS_COUNT NEEDS_FLAG NO
%token	XOBJECT OBSOLETE ON OPTIONS
%token	PACKAGE PLUSEQ PREFIX BUILDPREFIX PSEUDO_DEVICE PSEUDO_ROOT
%token	ROOT
%token	SELECT SINGLE SOURCE
%token	TYPE
%token	VECTOR VERSION
%token	WITH
%token	<num> NUMBER
%token	<str> PATHNAME QSTRING WORD EMPTYSTRING
%token	ENDDEFS

%type	<condexpr>	fopts condexpr condatom
%type	<condexpr>	cond_or_expr cond_and_expr cond_prefix_expr
%type	<condexpr>	 cond_base_expr
%type	<str>	fs_spec
%type	<flag>	fflags fflag oflags oflag
%type	<str>	rule
%type	<attr>	depend
%type	<devb>	devbase
%type	<deva>	devattach_opt
%type	<list>	atlist
%type	<loclist> interface_opt
%type	<str>	atname
%type	<loclist>	loclist locdef
%type	<str>	locdefault
%type	<loclist>	values locdefaults
%type	<attrlist>	depend_list depends
%type	<loclist>	locators locator
%type	<list>	dev_spec
%type	<str>	device_instance
%type	<str>	attachment
%type	<str>	value
%type	<val>	major_minor
%type	<num>	signed_number
%type	<i32>	int32 npseudo device_flags
%type	<str>	deffs
%type	<list>	deffses
%type	<defoptlist>	defopt
%type	<defoptlist>	defopts
%type	<str>	optdepend
%type	<list>	optdepends
%type	<list>	optdepend_list
%type	<str>	optfile_opt
%type	<list>	subarches
%type	<str>	filename stringvalue locname mkvarname
%type	<devmajor>	device_major_block device_major_char
%type	<list>	devnodes devnodetype devnodeflags devnode_dims

%%

/*
 * A complete configuration consists of both the selection part (a
 * kernel config such as GENERIC or SKYNET, plus also the various
 * std.* files), which selects the material to be in the kernel, and
 * also the definition part (files, files.*, etc.) that declares what
 * material is available to be placed in kernels.
 *
 * The two parts have almost entirely separate syntaxes. This grammar
 * covers both of them. When config is run on a kernel configuration
 * file, the std.* file for the port is included explicitly. The
 * files.* files are included implicitly when the std.* file declares
 * the machine type.
 *
 * The machine spec, which brings in the definition part, must appear
 * before all configuration material except for the "topthings"; these
 * are the "source" and "build" declarations that tell config where
 * things are. These are not used by default.
 *
 * A previous version of this comment contained the following text:
 *
 *       Note that we do not have sufficient keywords to enforce any
 *       order between elements of "topthings" without introducing
 *       shift/reduce conflicts.  Instead, check order requirements in
 *       the C code.
 *
 * As of March 2012 this comment makes no sense, as there are only two
 * topthings and no reason for them to be forcibly ordered.
 * Furthermore, the statement about conflicts is false.
 */

/* Complete configuration. */
configuration:
	topthings machine_spec definition_part selection_part
;

/* Sequence of zero or more topthings. */
topthings:
	  /* empty */
	| topthings topthing
;

/* Directory specification. */
topthing:
	                  '\n'
	| SOURCE filename '\n'		{ if (!srcdir) srcdir = $2; }
	| BUILD  filename '\n'		{ if (!builddir) builddir = $2; }
;

/* "machine foo" from std.whatever */
machine_spec:
	  XMACHINE WORD '\n'			{ setmachine($2,NULL,NULL,0); }
	| XMACHINE WORD WORD '\n'		{ setmachine($2,$3,NULL,0); }
	| XMACHINE WORD WORD subarches '\n'	{ setmachine($2,$3,$4,0); }
	| IOCONF WORD '\n'			{ setmachine($2,NULL,NULL,1); }
	| error { stop("cannot proceed without machine or ioconf specifier"); }
;

/* One or more sub-arches. */
subarches:
	  WORD				{ $$ = new_n($1); }
	| subarches WORD		{ $$ = new_nx($2, $1); }
;

/************************************************************/

/*
 * The machine definitions grammar.
 */

/* Complete definition part: the contents of all files.* files. */
definition_part:
	definitions ENDDEFS		{ check_maxpart(); check_version(); }
;

/* Zero or more definitions. Trap errors. */
definitions:
	  /* empty */
	| definitions '\n'
	| definitions definition '\n'	{ wrap_continue(); }
	| definitions error '\n'	{ wrap_cleanup(); }
	| definitions ENDFILE		{ enddefs(); checkfiles(); }
;

/* A single definition. */
definition:
	  define_file
	| define_object
	| define_device_major
	| define_prefix
	| define_buildprefix
	| define_devclass
	| define_filesystems
	| define_attribute
	| define_option
	| define_flag
	| define_obsolete_flag
	| define_param
	| define_obsolete_param
	| define_device
	| define_device_attachment
	| define_maxpartitions
	| define_maxusers
	| define_makeoptions
	| define_pseudo
	| define_pseudodev
	| define_major
	| define_version
;

/* source file: file foo/bar.c bar|baz needs-flag compile-with blah */
define_file:
	XFILE filename fopts fflags rule	{ addfile($2, $3, $4, $5); }
;

/* object file: object zot.o foo|zot needs-flag */
define_object:
	XOBJECT filename fopts oflags	{ addfile($2, $3, $4, NULL); }
;

/* device major declaration */
define_device_major:
	DEVICE_MAJOR WORD device_major_char device_major_block fopts devnodes
					{
		adddevm($2, $3, $4, $5, $6);
		do_devsw = 1;
	}
;

/* prefix delimiter */
define_prefix:
	  PREFIX filename		{ prefix_push($2); }
	| PREFIX			{ prefix_pop(); }
;

define_buildprefix:
	  BUILDPREFIX filename		{ buildprefix_push($2); }
	| BUILDPREFIX WORD		{ buildprefix_push($2); }
	| BUILDPREFIX			{ buildprefix_pop(); }
;

define_devclass:
	DEVCLASS WORD			{ (void)defdevclass($2, NULL, NULL, 1); }
;

define_filesystems:
	DEFFS deffses optdepend_list	{ deffilesystem($2, $3); }
;

define_attribute:
	DEFINE WORD interface_opt depend_list
					{ (void)defattr0($2, $3, $4, 0); }
;

define_option:
	DEFOPT optfile_opt defopts optdepend_list
					{ defoption($2, $3, $4); }
;

define_flag:
	DEFFLAG optfile_opt defopts optdepend_list
					{ defflag($2, $3, $4, 0); }
;

define_obsolete_flag:
	OBSOLETE DEFFLAG optfile_opt defopts
					{ defflag($3, $4, NULL, 1); }
;

define_param:
	DEFPARAM optfile_opt defopts optdepend_list
					{ defparam($2, $3, $4, 0); }
;

define_obsolete_param:
	OBSOLETE DEFPARAM optfile_opt defopts
					{ defparam($3, $4, NULL, 1); }
;

define_device:
	DEVICE devbase interface_opt depend_list
					{ defdev($2, $3, $4, 0); }
;

define_device_attachment:
	ATTACH devbase AT atlist devattach_opt depend_list
					{ defdevattach($5, $2, $4, $6); }
;

define_maxpartitions:
	MAXPARTITIONS int32		{ maxpartitions = $2; }
;

define_maxusers:
	MAXUSERS int32 int32 int32
					{ setdefmaxusers($2, $3, $4); }
;

define_makeoptions:
	MAKEOPTIONS condmkopt_list
;

define_pseudo:
	/* interface_opt in DEFPSEUDO is for backwards compatibility */
	DEFPSEUDO devbase interface_opt depend_list
					{ defdev($2, $3, $4, 1); }
;

define_pseudodev:
	DEFPSEUDODEV devbase interface_opt depend_list
					{ defdev($2, $3, $4, 2); }
;

define_major:
	MAJOR '{' majorlist '}'
;

define_version:
	VERSION int32		{ setversion($2); }
;

/* file options: optional expression of conditions */
fopts:
	  /* empty */			{ $$ = NULL; }
	| condexpr			{ $$ = $1; }
;

/* zero or more flags for a file */
fflags:
	  /* empty */			{ $$ = 0; }
	| fflags fflag			{ $$ = $1 | $2; }
;

/* one flag for a file */
fflag:
	  NEEDS_COUNT			{ $$ = FI_NEEDSCOUNT; }
	| NEEDS_FLAG			{ $$ = FI_NEEDSFLAG; }
;

/* extra compile directive for a source file */
rule:
	  /* empty */			{ $$ = NULL; }
	| COMPILE_WITH stringvalue	{ $$ = $2; }
;

/* zero or more flags for an object file */
oflags:
	  /* empty */			{ $$ = 0; }
	| oflags oflag			{ $$ = $1 | $2; }
;

/* a single flag for an object file */
oflag:
	NEEDS_FLAG			{ $$ = FI_NEEDSFLAG; }
;

/* char 55 */
device_major_char:
	  /* empty */			{ $$ = -1; }
	| CHAR int32			{ $$ = $2; }
;

/* block 33 */
device_major_block:
	  /* empty */			{ $$ = -1; }
	| BLOCK int32			{ $$ = $2; }
;

/* device node specification */
devnodes:
	  /* empty */			{ $$ = new_s("DEVNODE_DONTBOTHER"); }
	| devnodetype ',' devnodeflags	{ $$ = nvcat($1, $3); }
	| devnodetype			{ $$ = $1; }
;

/* device nodes without flags */
devnodetype:
	  SINGLE			{ $$ = new_s("DEVNODE_SINGLE"); }
	| VECTOR '=' devnode_dims  { $$ = nvcat(new_s("DEVNODE_VECTOR"), $3); }
;

/* dimensions (?) */
devnode_dims:
	  NUMBER			{ $$ = new_i($1.val); }
	| NUMBER ':' NUMBER		{
		struct nvlist *__nv1, *__nv2;

		__nv1 = new_i($1.val);
		__nv2 = new_i($3.val);
		$$ = nvcat(__nv1, __nv2);
	  }
;

/* flags for device nodes */
devnodeflags:
	LINKZERO			{ $$ = new_s("DEVNODE_FLAG_LINKZERO");}
;

/* one or more file system names */
deffses:
	  deffs				{ $$ = new_n($1); }
	| deffses deffs			{ $$ = new_nx($2, $1); }
;

/* a single file system name */
deffs:
	WORD				{ $$ = $1; }
;

/* optional locator specification */
interface_opt:
	  /* empty */			{ $$ = NULL; }
	| '{' '}'			{ $$ = present_loclist(NULL); }
	| '{' loclist '}'		{ $$ = present_loclist($2); }
;

/*
 * loclist order matters, must use right recursion
 * XXX wot?
 */

/* list of locator definitions */
loclist:
	  locdef			{ $$ = $1; }
	| locdef ',' loclist		{ $$ = $1; app($1, $3); }
;

/*
 * "[ WORD locdefault ]" syntax may be unnecessary...
 */

/* one locator definition */
locdef:
	  locname locdefault 		{ $$ = MK3(loc, $1, $2, 0); }
	| locname			{ $$ = MK3(loc, $1, NULL, 0); }
	| '[' locname locdefault ']'	{ $$ = MK3(loc, $2, $3, 1); }
	| locname '[' int32 ']'	{ $$ = locarray($1, $3, NULL, 0); }
	| locname '[' int32 ']' locdefaults
					{ $$ = locarray($1, $3, $5, 0); }
	| '[' locname '[' int32 ']' locdefaults ']'
					{ $$ = locarray($2, $4, $6, 1); }
;

/* locator name */
locname:
	  WORD				{ $$ = $1; }
	| QSTRING			{ $$ = $1; }
;

/* locator default value */
locdefault:
	'=' value			{ $$ = $2; }
;

/* multiple locator default values */
locdefaults:
	'=' '{' values '}'		{ $$ = $3; }
;

/* list of depends, may be empty */
depend_list:
	  /* empty */			{ $$ = NULL; }
	| ':' depends			{ $$ = $2; }
;

/* one or more depend items */
depends:
	  depend			{ $$ = MK2(attrlist, NULL, $1); }
	| depends ',' depend		{ $$ = MK2(attrlist, $1, $3); }
;

/* one depend item (which is an attribute) */
depend:
	WORD				{ $$ = refattr($1); }
;

/* list of option depends, may be empty */
optdepend_list:
	  /* empty */			{ $$ = NULL; }
	| ':' optdepends		{ $$ = $2; }
;

/* a list of option dependencies */
optdepends:
	  optdepend			{ $$ = new_n($1); }
	| optdepends ',' optdepend	{ $$ = new_nx($3, $1); }
;

/* one option depend, which is an option name */
optdepend:
	WORD				{ $$ = $1; }
;


/* list of places to attach: attach blah at ... */
atlist:
	  atname			{ $$ = new_n($1); }
	| atlist ',' atname		{ $$ = new_nx($3, $1); }
;

/* a place to attach a device */
atname:
	  WORD				{ $$ = $1; }
	| ROOT				{ $$ = NULL; }
;

/* one or more defined options */
defopts:
	  defopt			{ $$ = $1; }
	| defopts defopt		{ $$ = defoptlist_append($2, $1); }
;

/* one defined option */
defopt:
	  WORD				{ $$ = MK3(defoptlist, $1, NULL, NULL); }
	| WORD '=' value		{ $$ = MK3(defoptlist, $1, $3, NULL); }
	| WORD COLONEQ value		{ $$ = MK3(defoptlist, $1, NULL, $3); }
	| WORD '=' value COLONEQ value	{ $$ = MK3(defoptlist, $1, $3, $5); }
;

/* list of conditional makeoptions */
condmkopt_list:
	  condmkoption
	| condmkopt_list ',' condmkoption
;

/* one conditional make option */
condmkoption:
	condexpr mkvarname PLUSEQ value	{ appendcondmkoption($1, $2, $4); }
;

/* device name */
devbase:
	WORD				{ $$ = getdevbase($1); }
;

/* optional attachment: with foo */
devattach_opt:
	  /* empty */			{ $$ = NULL; }
	| WITH WORD			{ $$ = getdevattach($2); }
;

/* list of major numbers */
/* XXX why is this right-recursive? */
majorlist:
	  majordef
	| majorlist ',' majordef
;

/* one major number */
majordef:
	devbase '=' int32		{ setmajor($1, $3); }
;

int32:
	NUMBER	{
		if ($1.val > INT_MAX || $1.val < INT_MIN)
			cfgerror("overflow %" PRId64, $1.val);
		else
			$$ = (int32_t)$1.val;
	}
;

/************************************************************/

/*
 * The selection grammar.
 */

/* Complete selection part: all std.* files plus selected config. */
selection_part:
	selections
;

/* Zero or more config items. Trap errors. */
selections:
	  /* empty */
	| selections '\n'
	| selections selection '\n'	{ wrap_continue(); }
	| selections error '\n'		{ wrap_cleanup(); }
;

/* One config item. */
selection:
	  definition
	| select_attr
	| select_no_attr
	| select_no_filesystems
	| select_filesystems
	| select_no_makeoptions
	| select_makeoptions
	| select_no_options
	| select_options
	| select_maxusers
	| select_ident
	| select_no_ident
	| select_config
	| select_no_config
	| select_no_pseudodev
	| select_pseudodev
	| select_pseudoroot
	| select_no_device_instance_attachment
	| select_no_device_attachment
	| select_no_device_instance
	| select_device_instance
;

select_attr:
	SELECT WORD			{ addattr($2); }
;

select_no_attr:
	NO SELECT WORD			{ delattr($3); }
;

select_no_filesystems:
	NO FILE_SYSTEM no_fs_list
;

select_filesystems:
	FILE_SYSTEM fs_list
;

select_no_makeoptions:
	NO MAKEOPTIONS no_mkopt_list
;

select_makeoptions:
	MAKEOPTIONS mkopt_list
;

select_no_options:
	NO OPTIONS no_opt_list
;

select_options:
	OPTIONS opt_list
;

select_maxusers:
	MAXUSERS int32			{ setmaxusers($2); }
;

select_ident:
	IDENT stringvalue		{ setident($2); }
;

select_no_ident:
	NO IDENT			{ setident(NULL); }
;

select_config:
	CONFIG conf root_spec sysparam_list
					{ addconf(&conf); }
;

select_no_config:
	NO CONFIG WORD			{ delconf($3); }
;

select_no_pseudodev:
	NO PSEUDO_DEVICE WORD		{ delpseudo($3); }
;

select_pseudodev:
	PSEUDO_DEVICE WORD npseudo	{ addpseudo($2, $3); }
;

select_pseudoroot:
	PSEUDO_ROOT device_instance	{ addpseudoroot($2); }
;

select_no_device_instance_attachment:
	NO device_instance AT attachment
					{ deldevi($2, $4); }
;

select_no_device_attachment:
	NO DEVICE AT attachment		{ deldeva($4); }
;

select_no_device_instance:
	NO device_instance		{ deldev($2); }
;

select_device_instance:
	device_instance AT attachment locators device_flags
					{ adddev($1, $3, $4, $5); }
;

/* list of filesystems */
fs_list:
	  fsoption
	| fs_list ',' fsoption
;

/* one filesystem */
fsoption:
	WORD				{ addfsoption($1); }
;

/* list of filesystems that had NO in front */
no_fs_list:
	  no_fsoption
	| no_fs_list ',' no_fsoption
;

/* one filesystem that had NO in front */
no_fsoption:
	WORD				{ delfsoption($1); }
;

/* list of make options */
/* XXX why is this right-recursive? */
mkopt_list:
	  mkoption
	| mkopt_list ',' mkoption
;

/* one make option */
mkoption:
	  mkvarname '=' value		{ addmkoption($1, $3); }
	| mkvarname PLUSEQ value	{ appendmkoption($1, $3); }
;

/* list of make options that had NO in front */
no_mkopt_list:
	  no_mkoption
	| no_mkopt_list ',' no_mkoption
;

/* one make option that had NO in front */
/* XXX shouldn't this be mkvarname rather than WORD? */
no_mkoption:
	WORD				{ delmkoption($1); }
;

/* list of options */
opt_list:
	  option
	| opt_list ',' option
;

/* one option */
option:
	  WORD				{ addoption($1, NULL); }
	| WORD '=' value		{ addoption($1, $3); }
;

/* list of options that had NO in front */
no_opt_list:
	  no_option
	| no_opt_list ',' no_option
;

/* one option that had NO in front */
no_option:
	WORD				{ deloption($1); }
;

/* the name in "config name root on ..." */
conf:
	WORD				{
		conf.cf_name = $1;
		conf.cf_lineno = currentline();
		conf.cf_fstype = NULL;
		conf.cf_root = NULL;
		conf.cf_dump = NULL;
	}
;

/* root fs specification */
root_spec:
	  ROOT on_opt dev_spec		{ setconf(&conf.cf_root, "root", $3); }
	| ROOT on_opt dev_spec fs_spec	{ setconf(&conf.cf_root, "root", $3); }
;

/* device for root fs or dump */
dev_spec:
	  '?'				{ $$ = new_si(intern("?"),
					    (long long)NODEV); }
	| WORD				{ $$ = new_si($1,
					    (long long)NODEV); }
	| major_minor			{ $$ = new_si(NULL, $1); }
;

/* major and minor device number */
major_minor:
	MAJOR NUMBER MINOR NUMBER	{ $$ = (int64_t)makedev($2.val, $4.val); }
;

/* filesystem type for root fs specification */
fs_spec:
	  TYPE '?'		   { setfstype(&conf.cf_fstype, intern("?")); }
	| TYPE WORD			{ setfstype(&conf.cf_fstype, $2); }
;

/* zero or more additional system parameters */
sysparam_list:
	  /* empty */
	| sysparam_list sysparam
;

/* one additional system parameter (there's only one: dumps) */
sysparam:
	DUMPS on_opt dev_spec	       { setconf(&conf.cf_dump, "dumps", $3); }
;

/* number of pseudo devices to configure (which is optional) */
npseudo:
	  /* empty */			{ $$ = 1; }
	| int32				{ $$ = $1; }
;

/* name of a device to configure */
device_instance:
	  WORD				{ $$ = $1; }
	| WORD '*'			{ $$ = starref($1); }
;

/* name of a device to configure an attachment to */
attachment:
	  ROOT				{ $$ = NULL; }
	| WORD				{ $$ = $1; }
	| WORD '?'			{ $$ = wildref($1); }
;

/* zero or more locators */
locators:
	  /* empty */			{ $$ = NULL; }
	| locators locator		{ $$ = $2; app($2, $1); }
;

/* one locator */
locator:
	  WORD '?'			{ $$ = MK3(loc, $1, NULL, 0); }
	| WORD values			{ $$ = namelocvals($1, $2); }
;

/* optional device flags */
device_flags:
	  /* empty */			{ $$ = 0; }
	| FLAGS int32			{ $$ = $2; }
;

/************************************************************/

/*
 * conditions
 */


/*
 * order of options is important, must use right recursion
 *
 * dholland 20120310: wut?
 */

/* expression of conditions */
condexpr:
	cond_or_expr
;

cond_or_expr:
	  cond_and_expr
	| cond_or_expr '|' cond_and_expr	{ $$ = MKF2(cx, or, $1, $3); }
;

cond_and_expr:
	  cond_prefix_expr
	| cond_and_expr '&' cond_prefix_expr	{ $$ = MKF2(cx, and, $1, $3); }
;

cond_prefix_expr:
	  cond_base_expr
/* XXX notyet - need to strengthen downstream first */
/*	| '!' cond_prefix_expr			{ $$ = MKF1(cx, not, $2); } */
;

cond_base_expr:
	  condatom			{ $$ = $1; }
	| '!' condatom			{ $$ = MKF1(cx, not, $2); }
	| '(' condexpr ')'		{ $$ = $2; }
;

/* basic element of config element expression: a config element */
condatom:
	WORD				{ $$ = MKF1(cx, atom, $1); }
;

/************************************************************/

/*
 * Various nonterminals shared between the grammars.
 */

/* variable name for make option */
mkvarname:
	  QSTRING			{ $$ = $1; }
	| WORD				{ $$ = $1; }
;

/* optional file for an option */
optfile_opt:
	  /* empty */			{ $$ = NULL; }
	| filename			{ $$ = $1; }
;

/* filename. */
filename:
	  QSTRING			{ $$ = $1; }
	| PATHNAME			{ $$ = $1; }
;

/* constant value */
value:
	  QSTRING			{ $$ = $1; }
	| WORD				{ $$ = $1; }
	| EMPTYSTRING			{ $$ = $1; }
	| signed_number			{
		char bf[40];

		(void)snprintf(bf, sizeof(bf), FORMAT($1), (long long)$1.val);
		$$ = intern(bf);
	  }
;

/* constant value that is a string */
stringvalue:
	  QSTRING			{ $$ = $1; }
	| WORD				{ $$ = $1; }
;

/* comma-separated list of values */
/* XXX why right-recursive? */
values:
	  value				{ $$ = MKF2(loc, val, $1, NULL); }
	| value ',' values		{ $$ = MKF2(loc, val, $1, $3); }
;

/* possibly negative number */
signed_number:
	  NUMBER			{ $$ = $1; }
	| '-' NUMBER			{ $$.fmt = $2.fmt; $$.val = -$2.val; }
;

/* optional ON keyword */
on_opt:
	  /* empty */
	| ON
;

%%

void
yyerror(const char *s)
{

	cfgerror("%s", s);
}

/************************************************************/

/*
 * Wrap allocations that live on the parser stack so that we can free
 * them again on error instead of leaking.
 */

#define MAX_WRAP 1000

struct wrap_entry {
	void *ptr;
	unsigned typecode;
};

static struct wrap_entry wrapstack[MAX_WRAP];
static unsigned wrap_depth;

/*
 * Remember pointer PTR with type-code CODE.
 */
static void
wrap_alloc(void *ptr, unsigned code)
{
	unsigned pos;

	if (wrap_depth >= MAX_WRAP) {
		panic("allocation wrapper stack overflow");
	}
	pos = wrap_depth++;
	wrapstack[pos].ptr = ptr;
	wrapstack[pos].typecode = code;
}

/*
 * We succeeded; commit to keeping everything that's been allocated so
 * far and clear the stack.
 */
static void
wrap_continue(void)
{
	wrap_depth = 0;
}

/*
 * We failed; destroy all the objects allocated.
 */
static void
wrap_cleanup(void)
{
	unsigned i;

	/*
	 * Destroy each item. Note that because everything allocated
	 * is entered on the list separately, lists and trees need to
	 * have their links blanked before being destroyed. Also note
	 * that strings are interned elsewhere and not handled by this
	 * mechanism.
	 */

	for (i=0; i<wrap_depth; i++) {
		switch (wrapstack[i].typecode) {
		    case WRAP_CODE_nvlist:
			nvfree(wrapstack[i].ptr);
			break;
		    case WRAP_CODE_defoptlist:
			{
				struct defoptlist *dl = wrapstack[i].ptr;

				dl->dl_next = NULL;
				defoptlist_destroy(dl);
			}
			break;
		    case WRAP_CODE_loclist:
			{
				struct loclist *ll = wrapstack[i].ptr;

				ll->ll_next = NULL;
				loclist_destroy(ll);
			}
			break;
		    case WRAP_CODE_attrlist:
			{
				struct attrlist *al = wrapstack[i].ptr;

				al->al_next = NULL;
				al->al_this = NULL;
				attrlist_destroy(al);
			}
			break;
		    case WRAP_CODE_condexpr:
			{
				struct condexpr *cx = wrapstack[i].ptr;

				cx->cx_type = CX_ATOM;
				cx->cx_atom = NULL;
				condexpr_destroy(cx);
			}
			break;
		    default:
			panic("invalid code %u on allocation wrapper stack",
			      wrapstack[i].typecode);
		}
	}

	wrap_depth = 0;
}

/*
 * Instantiate the wrapper functions.
 *
 * Each one calls wrap_alloc to save the pointer and then returns the
 * pointer again; these need to be generated with the preprocessor in
 * order to be typesafe.
 */
#define DEF_ALLOCWRAP(t) \
	static struct t *				\
	wrap_mk_##t(struct t *arg)			\
	{						\
		wrap_alloc(arg, WRAP_CODE_##t);		\
		return arg;				\
	}

DEF_ALLOCWRAP(nvlist);
DEF_ALLOCWRAP(defoptlist);
DEF_ALLOCWRAP(loclist);
DEF_ALLOCWRAP(attrlist);
DEF_ALLOCWRAP(condexpr);

/************************************************************/

/*
 * Data constructors
 *
 * (These are *beneath* the allocation wrappers.)
 */

static struct defoptlist *
mk_defoptlist(const char *name, const char *val, const char *lintval)
{
	return defoptlist_create(name, val, lintval);
}

static struct loclist *
mk_loc(const char *name, const char *str, long long num)
{
	return loclist_create(name, str, num);
}

static struct loclist *
mk_loc_val(const char *str, struct loclist *next)
{
	struct loclist *ll;

	ll = mk_loc(NULL, str, 0);
	ll->ll_next = next;
	return ll;
}

static struct attrlist *
mk_attrlist(struct attrlist *next, struct attr *a)
{
	return attrlist_cons(next, a);
}

static struct condexpr *
mk_cx_atom(const char *s)
{
	struct condexpr *cx;

	cx = condexpr_create(CX_ATOM);
	cx->cx_atom = s;
	return cx;
}

static struct condexpr *
mk_cx_not(struct condexpr *sub)
{
	struct condexpr *cx;

	cx = condexpr_create(CX_NOT);
	cx->cx_not = sub;
	return cx;
}

static struct condexpr *
mk_cx_and(struct condexpr *left, struct condexpr *right)
{
	struct condexpr *cx;

	cx = condexpr_create(CX_AND);
	cx->cx_and.left = left;
	cx->cx_and.right = right;
	return cx;
}

static struct condexpr *
mk_cx_or(struct condexpr *left, struct condexpr *right)
{
	struct condexpr *cx;

	cx = condexpr_create(CX_OR);
	cx->cx_or.left = left;
	cx->cx_or.right = right;
	return cx;
}

/************************************************************/

static void
setmachine(const char *mch, const char *mcharch, struct nvlist *mchsubarches,
	int isioconf)
{
	char buf[MAXPATHLEN];
	struct nvlist *nv;

	if (isioconf) {
		if (include(_PATH_DEVNULL, ENDDEFS, 0, 0) != 0)
			exit(1);
		ioconfname = mch;
		return;
	}

	machine = mch;
	machinearch = mcharch;
	machinesubarches = mchsubarches;

	/*
	 * Define attributes for all the given names
	 */
	if (defattr(machine, NULL, NULL, 0) != 0 ||
	    (machinearch != NULL &&
	     defattr(machinearch, NULL, NULL, 0) != 0))
		exit(1);
	for (nv = machinesubarches; nv != NULL; nv = nv->nv_next) {
		if (defattr(nv->nv_name, NULL, NULL, 0) != 0)
			exit(1);
	}

	/*
	 * Set up the file inclusion stack.  This empty include tells
	 * the parser there are no more device definitions coming.
	 */
	if (include(_PATH_DEVNULL, ENDDEFS, 0, 0) != 0)
		exit(1);

	/* Include arch/${MACHINE}/conf/files.${MACHINE} */
	(void)snprintf(buf, sizeof(buf), "arch/%s/conf/files.%s",
	    machine, machine);
	if (include(buf, ENDFILE, 0, 0) != 0)
		exit(1);

	/* Include any arch/${MACHINE_SUBARCH}/conf/files.${MACHINE_SUBARCH} */
	for (nv = machinesubarches; nv != NULL; nv = nv->nv_next) {
		(void)snprintf(buf, sizeof(buf), "arch/%s/conf/files.%s",
		    nv->nv_name, nv->nv_name);
		if (include(buf, ENDFILE, 0, 0) != 0)
			exit(1);
	}

	/* Include any arch/${MACHINE_ARCH}/conf/files.${MACHINE_ARCH} */
	if (machinearch != NULL)
		(void)snprintf(buf, sizeof(buf), "arch/%s/conf/files.%s",
		    machinearch, machinearch);
	else
		strlcpy(buf, _PATH_DEVNULL, sizeof(buf));
	if (include(buf, ENDFILE, 0, 0) != 0)
		exit(1);

	/*
	 * Include the global conf/files.  As the last thing
	 * pushed on the stack, it will be processed first.
	 */
	if (include("conf/files", ENDFILE, 0, 0) != 0)
		exit(1);

	oktopackage = 1;
}

static void
check_maxpart(void)
{

	if (maxpartitions <= 0 && ioconfname == NULL) {
		stop("cannot proceed without maxpartitions specifier");
	}
}

static void
check_version(void)
{
	/*
	 * In essence, version is 0 and is not supported anymore
	 */
	if (version < CONFIG_MINVERSION)
		stop("your sources are out of date -- please update.");
}

/*
 * Prepend a blank entry to the locator definitions so the code in
 * sem.c can distinguish "empty locator list" from "no locator list".
 * XXX gross.
 */
static struct loclist *
present_loclist(struct loclist *ll)
{
	struct loclist *ret;

	ret = MK3(loc, "", NULL, 0);
	ret->ll_next = ll;
	return ret;
}

static void
app(struct loclist *p, struct loclist *q)
{
	while (p->ll_next)
		p = p->ll_next;
	p->ll_next = q;
}

static struct loclist *
locarray(const char *name, int count, struct loclist *adefs, int opt)
{
	struct loclist *defs = adefs;
	struct loclist **p;
	char buf[200];
	int i;

	if (count <= 0) {
		fprintf(stderr, "config: array with <= 0 size: %s\n", name);
		exit(1);
	}
	p = &defs;
	for(i = 0; i < count; i++) {
		if (*p == NULL)
			*p = MK3(loc, NULL, "0", 0);
		snprintf(buf, sizeof(buf), "%s%c%d", name, ARRCHR, i);
		(*p)->ll_name = i == 0 ? name : intern(buf);
		(*p)->ll_num = i > 0 || opt;
		p = &(*p)->ll_next;
	}
	*p = 0;
	return defs;
}


static struct loclist *
namelocvals(const char *name, struct loclist *vals)
{
	struct loclist *p;
	char buf[200];
	int i;

	for (i = 0, p = vals; p; i++, p = p->ll_next) {
		snprintf(buf, sizeof(buf), "%s%c%d", name, ARRCHR, i);
		p->ll_name = i == 0 ? name : intern(buf);
	}
	return vals;
}

