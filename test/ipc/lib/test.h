/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan/
 */

#ifndef __TEST_H__
#define __TEST_H__

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define TPASS    0    /* Test passed flag */
#define TFAIL    1    /* Test failed flag */
#define TBROK    2    /* Test broken flag */
#define TWARN    4    /* Test warning flag */
#define TRETR    8    /* Test retire flag */
#define TINFO    16   /* Test information flag */
#define TCONF    32   /* Test not appropriate for configuration flag */

/*
 * To determine if you are on a Umk or Unicos system,
 * use sysconf(_SC_CRAY_SYSTEM).  But since _SC_CRAY_SYSTEM
 * is not defined until 90, it will be define here if not already
 * defined.
 * if ( sysconf(_SC_CRAY_SYSTEM) == 1 )
 *    on UMK
 * else   # returned 0 or -1 
 *    on Unicos
 * This is only being done on CRAY systems.
 */
#ifdef CRAY
#ifndef _SC_CRAY_SYSTEM
#define _SC_CRAY_SYSTEM  140
#endif /* ! _SC_CRAY_SYSTEM */
#endif /* CRAY */

/*
 * Ensure that NUMSIGS is defined.
 * It should be defined in signal.h or sys/signal.h on
 * UNICOS/mk and IRIX systems.   On UNICOS systems,
 * it is not defined, thus it is being set to UNICOS's NSIG.
 * Note:  IRIX's NSIG (signals are 1-(NSIG-1)) 
 *      is not same meaning as UNICOS/UMK's NSIG  (signals 1-NSIG)
 */
#define NSIG _NSIG
#define SIGCLD SIGCHLD
#ifndef NUMSIGS
#define NUMSIGS NSIG
#endif


/* defines for unexpected signal setup routine (set_usig.c) */
#define FORK    1		/* SIGCLD is to be ignored */
#define NOFORK  0		/* SIGCLD is to be caught */
#define DEF_HANDLER 0	/* tells set_usig() to use default signal handler */

/*
 * The following defines are used to control tst_res and t_result reporting.
 */

#define TOUTPUT	   "TOUTPUT"		/* The name of the environment variable */
					/* that can be set to one of the following */
					/* strings to control tst_res output */
					/* If not set, TOUT_VERBOSE_S is assumed */

#define TOUT_VERBOSE_S  "VERBOSE"	/* All test cases reported */
#define TOUT_CONDENSE_S "CONDENSE"	/* ranges are used where identical messages*/
					/* occur for sequential test cases */
#define TOUT_NOPASS_S   "NOPASS"	/* No pass test cases are reported */
#define TOUT_DISCARD_S  "DISCARD"	/* No output is reported */

#define TST_NOBUF	"TST_NOBUF"	/* The name of the environment variable */
					/* that can be set to control whether or not */
					/* tst_res will buffer output into 4096 byte */
					/* blocks of output */
					/* If not set, buffer is done.  If set, no */
					/* internal buffering will be done in tst_res */
					/* t_result does not have internal buffering */

/*
 * The following defines are used to control tst_tmpdir, tst_wildcard and t_mkchdir
 */

#define TDIRECTORY  "TDIRECTORY"	/* The name of the environment variable */
					/* that if is set, the value (directory) */
					/* is used by all tests as their working */
					/* directory.  tst_rmdir and t_rmdir will */
					/* not attempt to clean up. */
					/* This environment variable should only */
					/* be set when doing system testing since */
					/* tests will collide and break and fail */
					/* because of setting it. */

#define TEMPDIR	"/tmp"			/* This is the default temporary directory. */
					/* The environment variable TMPDIR is */
					/* used prior to this valid by tempnam(3). */
					/* To control the base location of the */
					/* temporary directory, set the TMPDIR */
					/* environment variable to desired path */

/*
 * The following contains support for error message passing.
 * See test_error.c for details.
 */
#define  TST_ERR_MESG_SIZE      1023    /* max size of error message */
#define  TST_ERR_FILE_SIZE      511     /* max size of module name used by compiler */
#define  TST_ERR_FUNC_SIZE      127     /* max size of func name */

typedef struct {
    int  te_line;                       /* line where last error was reported.  Use */
                                        /* "__LINE__" and let compiler do the rest */
    int  te_level;                      /* If set, will prevent current stored */
                                        /* error to not be overwritten */
    char te_func[TST_ERR_FUNC_SIZE+1];  /* name of function of last error */
                                        /* Name of function or NULL */
    char te_file[TST_ERR_FILE_SIZE+1];  /* module of last error.  Use */
                                        /* "__FILE__" and let compiler do the rest */
    char te_mesg[TST_ERR_MESG_SIZE+1];  /* string of last error */

} _TST_ERROR;

extern _TST_ERROR Tst_error;            /* defined in test_error.c */
#if __STDC__
extern void tst_set_error(char *file, int line, char *func, char *fmt, ...);
#else
extern void tst_set_error(void);
#endif
extern void tst_clear_error(void);


/*
 * The following define contains the name of an environmental variable
 * that can be used to specify the number of iterations.
 * It is supported in parse_opts.c and USC_setup.c.
 */
#define USC_ITERATION_ENV       "USC_ITERATIONS"

/*
 * The following define contains the name of an environmental variable
 * that can be used to specify to iteration until desired time
 * in floating point seconds has gone by.
 * Supported in USC_setup.c.
 */
#define USC_LOOP_WALLTIME	"USC_LOOP_WALLTIME"

/*
 * The following define contains the name of an environmental variable
 * that can be used to specify that no functional checks are wanted.
 * It is supported in parse_opts.c and USC_setup.c.
 */
#define USC_NO_FUNC_CHECK	"USC_NO_FUNC_CHECK"

/*
 * The following define contains the name of an environmental variable
 * that can be used to specify the delay between each loop iteration.
 * The value is in seconds (fractional numbers are allowed).
 * It is supported in parse_opts.c.
 */
#define USC_LOOP_DELAY		"USC_LOOP_DELAY"

/*
 * fork() can't be used on uClinux systems, so use FORK_OR_VFORK instead,
 * which will run vfork() on uClinux.
 * mmap() doesn't support MAP_PRIVATE on uClinux systems, so use
 * MAP_PRIVATE_EXCEPT_UCLINUX instead, which will skip the option on uClinux.
 * If MAP_PRIVATE really is required, the test can not be run on uClinux.
 */
#ifdef UCLINUX
#define FORK_OR_VFORK			vfork
#define MAP_PRIVATE_EXCEPT_UCLINUX	0	
#else
#define FORK_OR_VFORK			fork
#define MAP_PRIVATE_EXCEPT_UCLINUX	MAP_PRIVATE
#endif

/*
 * The following prototypes are needed to remove compile errors
 * on IRIX systems when compiled with -n32 and -64.
 */
extern void tst_res(int ttype, char *fname, char *arg_fmt, ...);
extern void tst_resm(int ttype, char *arg_fmt, ...);
extern void tst_brk(int ttype, char *fname, void (*func)(void), 
							char *arg_fmt, ...);
extern void tst_brkloop(int ttype, char *fname, void (*func)(void), 
							char *arg_fmt, ...);
extern void tst_brkm(int ttype, void (*func)(void), char *arg_fmt, ...);
extern void tst_brkloopm(int ttype, void (*func)(void), char *arg_fmt, ...);

extern int  tst_environ(void);
extern void tst_exit(void);
extern void tst_flush(void);

/* prototypes for the t_res.c functions */
extern void t_result(char *tcid, int tnum, int ttype, char *tmesg);
extern void tt_exit(void);
extern int  t_environ(void);
extern void t_breakum(char *tcid, int total, int typ, char *msg, void (*fnc)(void));

extern void tst_sig(int fork_flag, void (*handler)(int), void (*cleanup)(void));
extern void tst_tmpdir(void);
extern void tst_rmdir(void);

extern char * get_high_address(void);

extern void get_kver(int*, int*, int*);
extern int tst_kvercmp(int, int, int);

extern int tst_is_cwd_tmpfs(void);
extern int tst_cwd_has_free(int required_kib);

extern int Tst_count;

/* self_exec.c functions */
void maybe_run_child(void (*child)(void), char *fmt, ...);
int self_exec(char *argv0, char *fmt, ...);

#endif	/* end of __TEST_H__ */
