/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

#define DO_ASSERT 0	/* define when debugging */
# ifdef DO_ASSERT
# define STATIC
# else
# define STATIC static
# endif

#define VOID void	/* preferably void, but int if your compiler does
			   not recognize void
			*/

#if _POSIX_SOURCE
#define POSIX_OPEN 1	/* POSIX "open" system call */
#else
#define USG_OPEN 0	/* USG "open" system call (include <fcntl.h>) */
#define BSD4_2_OPEN 0 /* BSD 4.2 "open" system call (include <sys/file.h>)*/
#endif

/* Sanity check 1 */
# if (!!USG_OPEN) + (!!BSD4_2_OPEN) + (!!POSIX_OPEN) > 1
Oops, now why did you do that?
O, never mind, just try it again with
USG_OPEN = 1 or		for System III, System V etc.
BSD4_2_OPEN = 1 or	for Berkeley 4.2, Ultrix etc.
POSIX_OPEN = 1 or	for POSIX compliant systems.
USG_OPEN = 0 and BSD4_2_OPEN = 0 and POSIX_OOPEN
	for Berkeley 4.1, v7 and whatever else
# endif

#define BSD_REGEX 0	/* Berkeley style re_comp/re_exec */
#define V8_REGEX  1	/* V8 style regexec/regcomp */
#define USG_REGEX 0	/* USG style regex/regcmp */

/* Sanity check 2 */
# if USG_REGEX + BSD_REGEX + V8_REGEX > 1
Select one style for the regular expressions please!
# endif

#define USG_TTY 0	/* define if you have an USG tty driver (termio) */
			/* If you do not define this, you get either the
			 * V7 tty driver or the BSD one.
			 */
#if _POSIX_SOURCE
#define POSIX_TTY 1
#endif

#if __minix && !__minix_vmd
#define MAXNBLOCKS 10	/* Limit the number of blocks that yap will use to keep
			 * the input in core.
			 * This was needed to let yap run on an IBM XT
			 * running PC/IX. The problem is that malloc can
			 * allocate almost all available space, leaving no
			 * space for the stack, which causes a memory fault.
			 * Internal yap blocks are 2K, but there is a lot of
			 * additional information that yap keeps around. You
			 * can also use it if you want to limit yap's maximum
			 * size. If defined, it should be at least 3.
			 * 10 is probably a reasonable number.
			 */
#endif
/* Sanity check 3 */
# ifdef MAXNBLOCKS
# if MAXNBLOCKS < 3
Read the above comment!
# endif
# endif

#define VT100_PATCH	/* This involves a patch that will insert lines
			 * correctly on a VT100 terminal. The termcap entry
			 * for it contains an "al" with %-escapes. According
			 * to the termcap-documentation this is not allowed,
			 * but ...
			 * If VT100_PATCH is defined, the "al" capability will
			 * be offered to "tgoto", before "tputs"-ing it.
			 * I don't know if there are any terminals out there
			 * that have a % in their "al" capability. If there
			 * are, yap will not work properly when compiled with
			 * VT100_PATCH defined.
			 * Also, escape sequences for standout and underline
			 * will be tputs-ed if VT100_PATCH is defined.
			 */

#if _MINIX
#define	LCASE	0	/* Minix doesn;t have LCASE */
#endif
