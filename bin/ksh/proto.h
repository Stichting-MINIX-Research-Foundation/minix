/*	$NetBSD: proto.h,v 1.7 2005/06/26 19:09:00 christos Exp $	*/

/*
 * prototypes for PD-KSH
 * originally generated using "cproto.c 3.5 92/04/11 19:28:01 cthuang "
 * $Id: proto.h,v 1.7 2005/06/26 19:09:00 christos Exp $
 */

/* alloc.c */
Area *	ainit		ARGS((Area *));
void 	afreeall	ARGS((Area *));
void *	alloc		ARGS((size_t, Area *));
void *	aresize		ARGS((void *, size_t, Area *));
void 	afree		ARGS((void *, Area *));
/* c_ksh.c */
int 	c_hash		ARGS((char **));
int 	c_cd		ARGS((char **));
int 	c_pwd		ARGS((char **));
int 	c_print		ARGS((char **));
int 	c_whence	ARGS((char **));
int 	c_command	ARGS((char **));
int 	c_typeset	ARGS((char **));
int 	c_alias		ARGS((char **));
int 	c_unalias	ARGS((char **));
int 	c_let		ARGS((char **));
int 	c_jobs		ARGS((char **));
int 	c_fgbg		ARGS((char **));
int 	c_kill		ARGS((char **));
void	getopts_reset	ARGS((int));
int	c_getopts	ARGS((char **));
int 	c_bind		ARGS((char **));
/* c_sh.c */
int 	c_label		ARGS((char **));
int 	c_shift		ARGS((char **));
int 	c_umask		ARGS((char **));
int 	c_dot		ARGS((char **));
int 	c_wait		ARGS((char **));
int 	c_read		ARGS((char **));
int 	c_eval		ARGS((char **));
int 	c_trap		ARGS((char **));
int 	c_brkcont	ARGS((char **));
int 	c_exitreturn	ARGS((char **));
int 	c_set		ARGS((char **));
int 	c_unset		ARGS((char **));
int 	c_ulimit	ARGS((char **));
int 	c_times		ARGS((char **));
int 	timex		ARGS((struct op *, int));
void	timex_hook	ARGS((struct op *, char ** volatile *));
int 	c_exec		ARGS((char **));
int 	c_builtin	ARGS((char **));
/* c_test.c */
int 	c_test		ARGS((char **));
/* edit.c: most prototypes in edit.h */
void 	x_init		ARGS((void));
int 	x_read		ARGS((char *, size_t));
void	set_editmode	ARGS((const char *));
/* emacs.c: most prototypes in edit.h */
int 	x_bind		ARGS((const char *, const char *, int, int));
/* eval.c */
char *	substitute	ARGS((const char *, int));
char **	eval		ARGS((char **, int));
char *	evalstr		ARGS((char *, int));
char *	evalonestr	ARGS((char *, int));
char	*debunk		ARGS((char *, const char *, size_t));
void	expand		ARGS((char *, XPtrV *, int));
int glob_str		ARGS((char *, XPtrV *, int));
/* exec.c */
int	fd_clexec	ARGS((int));
int 	execute		ARGS((struct op * volatile, volatile int));
int 	shcomexec	ARGS((char **));
struct tbl * findfunc	ARGS((const char *, unsigned int, int));
int 	define		ARGS((const char *, struct op *));
void 	builtin		ARGS((const char *, int (*)(char **)));
struct tbl *	findcom	ARGS((const char *, int));
void 	flushcom	ARGS((int all));
char *	search		ARGS((const char *, const char *, int, int *));
int	search_access	ARGS((const char *, int, int *));
int	pr_menu		ARGS((char *const *));
int	pr_list		ARGS((char *const *));
/* expr.c */
int 	evaluate	ARGS((const char *, long *, int));
int	v_evaluate	ARGS((struct tbl *, const char *, volatile int));
/* history.c */
void	init_histvec	ARGS((void));
void 	hist_init	ARGS((Source *));
void 	hist_finish	ARGS((void));
void	histsave	ARGS((int, const char *, int));
#ifdef HISTORY
int 	c_fc	 	ARGS((char **));
void	sethistsize	ARGS((int));
void	sethistfile	ARGS((const char *));
# ifdef EASY_HISTORY
void 	histappend	ARGS((const char *, int));
# endif
char **	histpos	 	ARGS((void));
int 	histN	 	ARGS((void));
int 	histnum	 	ARGS((int));
int	findhist	ARGS((int, int, const char *, int));
#endif /* HISTORY */
/* io.c */
void 	errorf		ARGS((const char *, ...))
				GCC_FUNC_ATTR2(noreturn, format(printf, 1, 2));
void 	warningf	ARGS((int, const char *, ...))
				GCC_FUNC_ATTR(format(printf, 2, 3));
void 	bi_errorf	ARGS((const char *, ...))
				GCC_FUNC_ATTR(format(printf, 1, 2));
void 	internal_errorf	ARGS((int, const char *, ...))
				GCC_FUNC_ATTR(format(printf, 2, 3));
void	error_prefix	ARGS((int));
void 	shellf		ARGS((const char *, ...))
				GCC_FUNC_ATTR(format(printf, 1, 2));
void 	shprintf	ARGS((const char *, ...))
				GCC_FUNC_ATTR(format(printf, 1, 2));
#ifdef KSH_DEBUG
void 	kshdebug_init_	ARGS((void));
void 	kshdebug_printf_ ARGS((const char *, ...))
				GCC_FUNC_ATTR(format(printf, 1, 2));
void 	kshdebug_dump_	ARGS((const char *, const void *, int));
#endif /* KSH_DEBUG */
int	can_seek	ARGS((int));
void	initio		ARGS((void));
int	ksh_dup2	ARGS((int, int, int));
int 	savefd		ARGS((int, int));
void 	restfd		ARGS((int, int));
void 	openpipe	ARGS((int *));
void 	closepipe	ARGS((int *));
int	check_fd	ARGS((char *, int, const char **));
#ifdef KSH
void	coproc_init	ARGS((void));
void	coproc_read_close ARGS((int));
void	coproc_readw_close ARGS((int));
void	coproc_write_close ARGS((int));
int	coproc_getfd	ARGS((int, const char **));
void	coproc_cleanup	ARGS((int));
#endif /* KSH */
struct temp *maketemp	ARGS((Area *, Temp_type, struct temp **));
/* jobs.c */
void 	j_init		ARGS((int));
void 	j_exit		ARGS((void));
void 	j_change	ARGS((void));
int 	exchild		ARGS((struct op *, int, int));
void 	startlast	ARGS((void));
int 	waitlast	ARGS((void));
int 	waitfor		ARGS((const char *, int *));
int 	j_kill		ARGS((const char *, int));
int 	j_resume	ARGS((const char *, int));
int 	j_jobs		ARGS((const char *, int, int));
void 	j_notify	ARGS((void));
pid_t	j_async		ARGS((void));
int 	j_stopped_running	ARGS((void));
/* lex.c */
int 	yylex		ARGS((int));
void 	yyerror		ARGS((const char *, ...))
				GCC_FUNC_ATTR2(noreturn, format(printf, 1, 2));
Source * pushs		ARGS((int, Area *));
void	set_prompt	ARGS((int, Source *));
void 	pprompt		ARGS((const char *, int));
/* mail.c */
#ifdef KSH
void 	mcheck		ARGS((void));
void 	mcset		ARGS((long));
void 	mbset		ARGS((char *));
void 	mpset		ARGS((char *));
#endif /* KSH */
/* main.c */
int 	include		ARGS((const char *, int, char **, int));
int 	command		ARGS((const char *));
int 	shell		ARGS((Source *volatile, int volatile));
void 	unwind		ARGS((int)) GCC_FUNC_ATTR(noreturn);
void 	newenv		ARGS((int));
void 	quitenv		ARGS((void));
void	cleanup_parents_env ARGS((void));
void	cleanup_proc_env ARGS((void));
void 	aerror		ARGS((Area *, const char *))
				GCC_FUNC_ATTR(noreturn);
/* misc.c */
void 	setctypes	ARGS((const char *, int));
void 	initctypes	ARGS((void));
char *	ulton		ARGS((unsigned long, int));
char *	str_save	ARGS((const char *, Area *));
char *	str_nsave	ARGS((const char *, int, Area *));
int	option		ARGS((const char *));
char *	getoptions	ARGS((void));
void	change_flag	ARGS((enum sh_flag, int, int));
int	parse_args	ARGS((char **v, int what, int *));
int 	getn		ARGS((const char *, int *));
int 	bi_getn		ARGS((const char *, int *));
int 	gmatch		ARGS((const char *, const char *, int));
int	has_globbing	ARGS((const char *, const char *));
const unsigned char *pat_scan ARGS((const unsigned char *,
				const unsigned char *, int));
void 	qsortp		ARGS((void **, size_t, int (*)(void *, void *)));
int 	xstrcmp		ARGS((void *, void *));
void	ksh_getopt_reset ARGS((Getopt *, int));
int	ksh_getopt	ARGS((char **, Getopt *, const char *));
void	print_value_quoted ARGS((const char *));
void	print_columns	ARGS((struct shf *, int,
			      char *(*)(void *, int, char *, int),
			      void *, int, int));
int	strip_nuls	ARGS((char *, int));
char	*str_zcpy	ARGS((char *, const char *, int));
int	blocking_read	ARGS((int, char *, int));
int	reset_nonblock	ARGS((int));
char	*ksh_get_wd	ARGS((char *, int));
/* path.c */
int	make_path	ARGS((const char *, const char *,
			      char **, XString *, int *));
void	simplify_path	ARGS((char *));
char	*get_phys_path	ARGS((const char *));
void	set_current_wd	ARGS((char *));
/* syn.c */
void 	initkeywords	ARGS((void));
struct op * compile	ARGS((Source *));
/* table.c */
unsigned int 	hash	ARGS((const char *));
void 	tinit		ARGS((struct table *, Area *, int));
struct tbl *	tsearch	ARGS((struct table *, const char *, unsigned int));
struct tbl *	tenter	ARGS((struct table *, const char *, unsigned int));
void 	tdelete		ARGS((struct tbl *));
void 	twalk		ARGS((struct tstate *, struct table *));
struct tbl *	tnext	ARGS((struct tstate *));
struct tbl **	tsort	ARGS((struct table *));
/* trace.c */
/* trap.c */
void	inittraps	ARGS((void));
#ifdef KSH
void	alarm_init	ARGS((void));
#endif /* KSH */
Trap *	gettrap		ARGS((const char *, int));
RETSIGTYPE trapsig	ARGS((int));
void	intrcheck	ARGS((void));
int	fatal_trap_check ARGS((void));
int	trap_pending	ARGS((void));
void 	runtraps	ARGS((int));
void 	runtrap		ARGS((Trap *));
void 	cleartraps	ARGS((void));
void 	restoresigs	ARGS((void));
void	settrap		ARGS((Trap *, char *));
int	block_pipe	ARGS((void));
void	restore_pipe	ARGS((int));
int	setsig		ARGS((Trap *, handler_t, int));
void	setexecsig	ARGS((Trap *, int));
/* tree.c */
int 	fptreef		ARGS((struct shf *, int, const char *, ...));
char *	snptreef	ARGS((char *, int, const char *, ...));
struct op *	tcopy	ARGS((struct op *, Area *));
char *	wdcopy		ARGS((const char *, Area *));
char *	wdscan		ARGS((const char *, int));
char *	wdstrip		ARGS((const char *));
void 	tfree		ARGS((struct op *, Area *));
/* var.c */
void 	newblock	ARGS((void));
void 	popblock	ARGS((void));
void	initvar		ARGS((void));
struct tbl *	global	ARGS((const char *));
struct tbl *	local	ARGS((const char *, bool_t));
char *	str_val		ARGS((struct tbl *));
long 	intval		ARGS((struct tbl *));
int 	setstr		ARGS((struct tbl *, const char *, int));
struct tbl *setint_v	ARGS((struct tbl *, struct tbl *));
void 	setint		ARGS((struct tbl *, long));
int	getint		ARGS((struct tbl *, long *));
struct tbl *	typeset	ARGS((const char *, Tflag, Tflag, int, int));
void 	unset		ARGS((struct tbl *, int));
char  * skip_varname	ARGS((const char *, int));
char	*skip_wdvarname ARGS((const char *, int));
int	is_wdvarname	ARGS((const char *, int));
int	is_wdvarassign	ARGS((const char *));
char **	makenv		ARGS((void));
void	change_random	ARGS((void));
int	array_ref_len	ARGS((const char *));
char *	arrayname	ARGS((const char *));
void    set_array	ARGS((const char *, int, char **));
/* version.c */
/* vi.c: see edit.h */


/* Hack to avoid billions of compile warnings on SunOS 4.1.x */
#if defined(MUN) && defined(sun) && !defined(__svr4__)
extern void bcopy ARGS((const void *, void *, size_t));
extern intclose ARGS((FILE *));
extern intprintf ARGS((FILE *, const char *, ...));
extern intread ARGS((void *, int, int, FILE *));
extern int ioctl ARGS((int, int, void *));
extern int killpg ARGS((int, int));
extern int nice ARGS((int));
extern int readlink ARGS((const char *, char *, int));
extern int setpgrp ARGS((int, int));
extern int strcasecmp ARGS((const char *, const char *));
extern int tolower ARGS((int));
extern int toupper ARGS((int));
/*  Include files aren't included yet */
extern int getrlimit ARGS(( /* int, struct rlimit * */ ));
extern int getrusage ARGS(( /* int, struct rusage * */ ));
extern int gettimeofday ARGS(( /* struct timeval *, struct timezone * */ ));
extern int setrlimit ARGS(( /* int, struct rlimit * */ ));
extern int lstat ARGS(( /* const char *, struct stat * */ ));
#endif
