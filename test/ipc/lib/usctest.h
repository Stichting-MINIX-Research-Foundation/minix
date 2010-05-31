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

/**********************************************************
 * 
 *    IRIX/Linux Feature Test and Evaluation - Silicon Graphics, Inc.
 * 
 *    FUNCTION NAME 	: usctest.h
 * 
 *    FUNCTION TITLE	: System Call Test Macros
 * 
 *    SYNOPSIS:
 *	See DESCRIPTION below.
 * 
 *    AUTHOR		: William Roske
 * 
 *    INITIAL RELEASE	: UNICOS 7.0
 * 
 *    DESCRIPTION
 * 	TEST(SCALL) - calls a system call
 *	TEST_VOID(SCALL) - same as TEST() but for syscalls with no return value.
 *	TEST_CLEANUP - print the log of errno return counts if STD_ERRNO_LOG 
 *		       is set.
 *	TEST_PAUSEF(HAND) - Pause for SIGUSR1 if the pause flag is set.
 *		      Use "hand" as the interrupt handling function
 *	TEST_PAUSE -  Pause for SIGUSR1 if the pause flag is set.
 *		      Use internal function to do nothing on signal and go on.
 *	TEST_LOOPING(COUNTER) - Conditional to check if test should
 *		      loop.  Evaluates to TRUE (1) or FALSE (0).
 *	TEST_ERROR_LOG(eno) - log that this errno was received,
 *		      if STD_ERRNO_LOG is set.
 *	TEST_EXP_ENOS(array) - set the bits in TEST_VALID_ENO array at
 *		      positions specified in integer "array"
 *
 *    RETURN VALUE
 * 	TEST(SCALL) - Global Variables set:
 *			int TEST_RETURN=return code from SCALL
 *			int TEST_ERRNO=value of errno at return from SCALL
 * 	TEST_VOID(SCALL) - Global Variables set:
 *			int TEST_ERRNO=value of errno at return from SCALL
 *	TEST_CLEANUP - None.
 *	TEST_PAUSEF(HAND) -  None.
 *	TEST_PAUSE -  None.
 *	TEST_LOOPING(COUNTER) - True if COUNTER < STD_LOOP_COUNT or
 *			STD_INFINITE is set.
 *	TEST_ERROR_LOG(eno) - None
 *	TEST_EXP_ENOS(array) - None
 *
 *    KNOWN BUGS
 *      If you use the TEST_PAUSE or TEST_LOOPING macros, you must
 *	link in parse_opts.o, which contains the code for those functions.
 *
 *#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#**/

#ifndef  __USCTEST_H__
#define __USCTEST_H__ 1

#ifndef _SC_CLK_TCK
#include <unistd.h>
#endif

#include <sys/param.h>

/* 
 * Ensure that PATH_MAX is defined 
 */
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX  MAXPATHLEN
#else
#define PATH_MAX  1024
#endif
#endif

#ifndef CRAY
#ifndef BSIZE 
#define BSIZE BBSIZE
#endif
#endif

/***********************************************************************
 * Define option_t structure type.
 * Entries in this struct are used by the parse_opts routine
 * to indicate valid options and return option arguments
 ***********************************************************************/
typedef struct {		
  char *option;      	/* Valid option string (one option only) like "a:" */
  int  *flag;		/* pointer to location to set true if option given */
  char **arg;		/* pointer to location to place argument, if needed */
} option_t;

/***********************************************************************
 * The following globals are defined in parse_opts.c but must be 
 * externed here because they are used in the macros defined below.
 ***********************************************************************/
extern int STD_FUNCTIONAL_TEST,	/* turned off by -f to not do functional test */
           STD_TIMING_ON,	/* turned on by -t to print timing stats */
           STD_PAUSE,		/* turned on by -p to pause before loop */
           STD_INFINITE,	/* turned on by -i0 to loop forever */
           STD_LOOP_COUNT,	/* changed by -in to set loop count to n */
           STD_ERRNO_LOG,	/* turned on by -e to log errnos returned */
           STD_ERRNO_LIST[],	/* counts of errnos returned.  indexed by errno */
	   STD_COPIES,
	   STD_argind;

extern float STD_LOOP_DURATION, /* wall clock time to iterate */
	     STD_LOOP_DELAY;    /* delay time after each iteration */

#define USC_MAX_ERRNO	2000
    
/**********************************************************************
 * Prototype for parse_opts routine
 **********************************************************************/
extern char *parse_opts(int ac, char **av, option_t *user_optarr, void (*uhf)(void));


/*
 * define a structure 
 */
struct usc_errno_t {
    int flag;
};

/***********************************************************************
 ****
 **** 
 ****
 **********************************************************************/
#ifdef  _USC_LIB_

extern long TEST_RETURN;
extern long TEST_ERRNO;
extern struct usc_errno_t TEST_VALID_ENO[USC_MAX_ERRNO];
extern long btime, etime, tmptime;

#else
/***********************************************************************
 * Global array of bit masks to indicate errnos that are expected.
 * Bits set by TEST_EXP_ENOS() macro and used by TEST_CLEANUP() macro.
 ***********************************************************************/
struct usc_errno_t TEST_VALID_ENO[USC_MAX_ERRNO];

/***********************************************************************
 * Globals for returning the return code and errno from the system call
 * test macros.
 ***********************************************************************/
long TEST_RETURN;
long TEST_ERRNO;

/***********************************************************************
 * temporary variables for determining max and min times in TEST macro
 ***********************************************************************/
long btime, etime, tmptime;	

#endif  /* _USC_LIB_ */

/***********************************************************************
 * structure for timing accumulator and counters 
 ***********************************************************************/
struct tblock {
    long tb_max;
    long tb_min;
    long tb_total;
    long tb_count;
};

/***********************************************************************
 * The following globals are externed here so that they are accessable
 * in the macros that follow.
 ***********************************************************************/
extern struct tblock tblock;
extern void STD_go(int);
extern int (*_TMP_FUNC)(void);
extern void STD_opts_help(void);


/***********************************************************************
 * TEST: calls a system call 
 * 
 * parameters:
 *	SCALL = system call and parameters to execute
 *
 ***********************************************************************/
#define TEST(SCALL) \
	do { \
		errno = 0; \
		TEST_RETURN = SCALL; \
		TEST_ERRNO = errno; \
	} while (0)

/***********************************************************************
 * TEST_VOID: calls a system call
 * 
 * parameters:
 *	SCALL = system call and parameters to execute
 *
 * Note: This is IDENTICAL to the TEST() macro except that it is intended
 * for use with syscalls returning no values (void syscall()).  The 
 * Typecasting nothing (void) into an unsigned integer causes compilation
 * errors.
 *
 ***********************************************************************/
#define TEST_VOID(SCALL)  errno=0; SCALL; TEST_ERRNO=errno;

/***********************************************************************
 * TEST_CLEANUP: print system call timing stats and errno log entries
 * to stdout if STD_TIMING_ON and STD_ERRNO_LOG are set, respectively.
 * Do NOT print ANY information if no system calls logged.
 * 
 * parameters:
 *	none
 *
 ***********************************************************************/
#define TEST_CLEANUP 	\
if ( STD_ERRNO_LOG ) {						\
	for (tmptime=0; tmptime<USC_MAX_ERRNO; tmptime++) {		\
	     if ( STD_ERRNO_LIST[tmptime] )	{			\
	         if ( TEST_VALID_ENO[tmptime].flag )			\
		     tst_resm(TINFO, "ERRNO %d:\tReceived %d Times",	\
			      tmptime, STD_ERRNO_LIST[tmptime]);	\
	         else							\
		     tst_resm(TINFO,					\
			      "ERRNO %d:\tReceived %d Times ** UNEXPECTED **",	\
			      tmptime, STD_ERRNO_LIST[tmptime]);	\
	     }								\
	}								\
}

/***********************************************************************
 * TEST_PAUSEF: Pause for SIGUSR1 if the pause flag is set.
 * 		 Set the user specified function as the interrupt
 *		 handler instead of "STD_go"
 * 
 * parameters:
 *	none
 *
 ***********************************************************************/
#define TEST_PAUSEF(HANDLER) 					\
if ( STD_PAUSE ) { 					\
    _TMP_FUNC = (int (*)(void))signal(SIGUSR1, HANDLER); 	\
    pause(); 						\
    signal(SIGUSR1, (void (*)(void))_TMP_FUNC);		\
}

/***********************************************************************
 * TEST_PAUSE: Pause for SIGUSR1 if the pause flag is set.
 *	       Just continue when signal comes in.
 * 
 * parameters:
 *	none
 *
 ***********************************************************************/
#define TEST_PAUSE usc_global_setup_hook();
int usc_global_setup_hook(void);

/***********************************************************************
 * TEST_LOOPING now call the usc_test_looping function.
 * The function will return 1 if the test should continue
 * iterating.
 *
 ***********************************************************************/
#define TEST_LOOPING usc_test_looping
int usc_test_looping(int counter);

/***********************************************************************
 * TEST_ERROR_LOG(eno): log this errno if STD_ERRNO_LOG flag set
 * 
 * parameters:
 *	int eno: the errno location in STD_ERRNO_LIST to log.
 *
 ***********************************************************************/
#define TEST_ERROR_LOG(eno)		\
    if ( STD_ERRNO_LOG )		\
        if ( eno < USC_MAX_ERRNO )		\
            STD_ERRNO_LIST[eno]++;	\


/***********************************************************************
 * TEST_EXP_ENOS(array): set the bits associated with the nput errnos
 *	in the TEST_VALID_ENO array.
 * 
 * parameters:
 *	int array[]: a zero terminated array of errnos expected.
 *
 ***********************************************************************/
#define TEST_EXP_ENOS(array)				\
    tmptime=0;						\
    while (array[tmptime] != 0) {			\
	if (array[tmptime] < USC_MAX_ERRNO)		\
	    TEST_VALID_ENO[array[tmptime]].flag=1;	\
	tmptime++;					\
    }
					

#endif  /* end of __USCTEST_H__ */
