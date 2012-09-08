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
 *    OS Testing - Silicon Graphics, Inc.
 * 
 *    FUNCTION NAME 	: parse_opts
 * 
 *    FUNCTION TITLE	: parse standard & user options for system call tests
 * 
 *    SYNOPSIS:
 *	#include "usctest.h"
 *
 * 	char *parse_opts(ac, av, user_optarr, uhf)
 *	int    ac;
 * 	char **av;
 *	option_t user_optarr[];
 *	void (*uhf)();
 * 
 *    AUTHOR		: William Roske/Richard Logan
 * 
 *    INITIAL RELEASE	: UNICOS 7.0
 * 
 *    DESCRIPTION
 * 	The parse_opts library routine takes that argc and argv parameters
 *	recevied by main() and an array of structures defining user options.
 *	It parses the command line setting flag and argument locations
 *      associated with the options.  It uses getopt to do the actual cmd line
 *      parsing.  uhf() is a function to print user define help
 *
 *      This module contains the functions usc_global_setup_hook and
 *      usc_test_looping, which are called by marcos defined in usctest.h.
 * 
 *    RETURN VALUE
 * 	parse_opts returns a pointer to an error message if an error occurs.
 *	This pointer is (char *)NULL if parsing is successful.
 * 
 *#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#**/
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>


#if UNIT_TEST
#include <time.h>
#endif /* UNIT_TEST */

#include "test.h"
#define _USC_LIB_   1	/* indicates we are the library to the usctest.h include */
#include "usctest.h"

#ifndef USC_COPIES
#define USC_COPIES   "USC_COPIES"
#endif

#ifndef UNIT_TEST
#define UNIT_TEST	0
#endif

#ifndef DEBUG
#define DEBUG	0
#endif

/* The timing information block. */
struct tblock tblock={0,((long) -1)>>1,0,0};


/* Define flags and args for standard options */
int STD_FUNCTIONAL_TEST=1,	/* flag indicating to do functional testing code */
    STD_TIMING_ON=0,		/* flag indicating to print timing stats */
    STD_PAUSE=0,		/* flag indicating to pause before actual start, */
				/* for contention mode */
    STD_INFINITE=0,		/* flag indciating to loop forever */
    STD_LOOP_COUNT=1,		/* number of iterations */
    STD_COPIES=1,		/* number of copies */
    STD_ERRNO_LOG=0;		/* flag indicating to do errno logging */

float STD_LOOP_DURATION=0.0,    /* duration value in fractional seconds */
      STD_LOOP_DELAY=0.0;	/* loop delay value in fractional seconds */


char **STD_opt_arr = NULL;	/* array of option strings */
int    STD_nopts=0,		/* number of elements in STD_opt_arr */
       STD_argind=1;		/* argv index to next argv element */
				/* (first argument) */
				/* To getopt users, it is like optind */

/*
 * The following variables are to support system testing additions.
 */
static int  STD_TP_barrier=0;	/* flag to do barrier in TEST_PAUSE */
				/* 2 - wait_barrier(), 3 - set_barrier(), * - barrier() */
static int  STD_LP_barrier=0;	/* flag to do barrier in TEST_LOOPING */
				/* 2 - wait_barrier(), 3 - set_barrier(), * - barrier() */
static int  STD_TP_shmem_sz=0;	/* shmalloc this many words per pe in TEST_PAUSE */
static int  STD_LD_shmem=0; 	/* flag to do shmem_puts and shmem_gets during delay */
static int  STD_LP_shmem=0; 	/* flag to do shmem_puts and gets during TEST_LOOPING */
static int  STD_LD_recfun=0;	/* do recressive function calls in loop delay */
static int  STD_LP_recfun=0;	/* do recressive function calls in TEST_LOOPING */
static int  STD_TP_sbrk=0;	/* do sbrk in TEST_PAUSE */
static int  STD_LP_sbrk=0;	/* do sbrk in TEST_LOOPING */
static char *STD_start_break=0; /* original sbrk size */
static int  Debug=0;

struct std_option_t {
    char *optstr;
    char *help;
    char *flag;
    char **arg;
} std_options[] = {
    { "c:", "  -c n    Run n copies concurrently\n", NULL, NULL},
    { "e" , "  -e      Turn on errno logging\n", NULL, NULL},
    { "f" , "  -f      Turn off functional testing\n", NULL, NULL},
    { "h" , "  -h      Show this help screen\n", NULL, NULL},
    { "i:", "  -i n    Execute test n times\n", NULL, NULL},
    { "I:", "  -I x    Execute test for x seconds\n", NULL, NULL},
    { "p" , "  -p      Pause for SIGUSR1 before starting\n", NULL, NULL}, 
    { "P:", "  -P x    Pause for x seconds between iterations\n", NULL, NULL},
    { "t" , "  -t      Turn on syscall timing\n", NULL, NULL},
#ifdef UCLINUX
    { "C:", "  -C ARG  Run the child process with arguments ARG (for internal use)\n",
      NULL, NULL},
#endif
    {NULL, NULL, NULL, NULL}};

void print_help(void (*user_help)(void));

/*
 * Structure for usc_recressive_func argument
 */
struct usc_bigstack_t {
   char space[4096];
};

static struct usc_bigstack_t *STD_bigstack=NULL;

/*
 * Counter of errnos returned (-e option).  Indexed by errno.
 * Make the array USC_MAX_ERRNO long.  That is the first Fortran
 * Lib errno.  No syscall should return an errno that high.
 */
int STD_ERRNO_LIST[USC_MAX_ERRNO];

/* define the string length for Mesg and Mesg2 strings */
#define STRLEN 2048

static char Mesg2[STRLEN];	/* holds possible return string */
static void usc_recressive_func(int, int, struct usc_bigstack_t *);

/*
 * Define bits for options that might have env variable default
 */
#define  OPT_iteration 		01
#define  OPT_nofunccheck	02
#define  OPT_duration		04
#define  OPT_delay		010
#define  OPT_copies		020

#ifdef UCLINUX
/* Allocated and used in self_exec.c: */
extern char *child_args;	/* Arguments to child when -C is used */
#endif

/**********************************************************************
 * parse_opts: 
 **********************************************************************/
char *
parse_opts(int ac, char **av, option_t *user_optarr, void (*uhf)(void))
{
    int found;		/* flag to indicate that an option specified was */
			/* found in the user's list */
    int k;		/* scratch integer for returns and short time usage */
    float  ftmp;	/* tmp float for parsing env variables */
    char *ptr;		/* used in getting env variables */
    int options=0;	/* no options specified */
    int optstrlen, i;
    char *optionstr;
    int opt;		/* return of getopt */

    /*
     * If not the first time this function is called, release the old STD_opt_arr
     * vector.
     */

    if ( STD_opt_arr != NULL ) {
	free(STD_opt_arr);
	STD_opt_arr=NULL;
    }
    /* Calculate how much space we need for the option string */
    optstrlen = 0;
    for (i = 0; std_options[i].optstr; ++i) 
	optstrlen += strlen(std_options[i].optstr);
    if (user_optarr)
	for (i = 0; user_optarr[i].option; ++i) {
	    if (strlen(user_optarr[i].option) > 2)
		return "parse_opts: ERROR - Only short options are allowed";
	    optstrlen += strlen(user_optarr[i].option);
	}
    optstrlen += 1;

    /* Create the option string for getopt */
    optionstr = (char *)malloc(optstrlen);
    if (!optionstr) 
	return "parse_opts: ERROR - Could not allocate memory for optionstr";

    optionstr[0] = '\0';

    for (i = 0; std_options[i].optstr; ++i)
	strcat(optionstr, std_options[i].optstr);
    if (user_optarr)
	for (i = 0; user_optarr[i].option; ++i)
	    /* only add the option if it wasn't there already */
	    if (strchr(optionstr, user_optarr[i].option[0]) == NULL) 
		strcat(optionstr, user_optarr[i].option);

#if DEBUG > 1
    printf("STD_nopts = %d\n", STD_nopts);
#endif

    /*
     *  Loop through av parsing options.
     */
    while ( (opt = getopt(ac, av, optionstr)) > 0) {

	STD_argind = optind;
#if DEBUG > 0
	printf("parse_opts: getopt returned '%c'\n", opt);
#endif

	switch (opt) {
		case '?': /* Unknown option */
			return "Unknown option";
			break;
		case ':': /* Missing Arg */
			return "Missing argument";
			break;
		case 'i': /* Iterations */
			options |= OPT_iteration;
			STD_LOOP_COUNT = atoi(optarg);
			if (STD_LOOP_COUNT == 0) STD_INFINITE = 1;
			break;
		case 'P': /* Delay between iterations */
			options |= OPT_delay;
			STD_LOOP_DELAY = atof(optarg);
			break;
		case 'I': /* Time duration */
			options |= OPT_duration;
			STD_LOOP_DURATION = atof(optarg);
			if ( STD_LOOP_DURATION == 0.0 ) STD_INFINITE=1; 
			break;
		case 'c': /* Copies */
			options |= OPT_copies;
			STD_COPIES = atoi(optarg);
			break;
		case 'f': /* Functional testing */
			STD_FUNCTIONAL_TEST = 0;
			break;
		case 'p': /* Pause for SIGUSR1 */
			STD_PAUSE = 1;
			break;
		case 't': /* syscall timing */
			STD_TIMING_ON = 1;
			break;
		case 'e': /* errno loggin */
			STD_ERRNO_LOG = 1;
			break;
		case 'h': /* Help */
			print_help(uhf);
			exit(0);
			break;
#ifdef UCLINUX
		case 'C': /* Run child */
			child_args = optarg;
			break;
#endif
		default:
			
            /* Check all the user specified options */
            found=0;
	    for(i = 0; user_optarr[i].option; ++i) {

		if (opt == user_optarr[i].option[0]) {
                    /* Yup, This is a user option, set the flag and look for argument */
		    if ( user_optarr[i].flag ) {
                        *user_optarr[i].flag=1;
		    }
                    found++;

		    /* save the argument at the user's location */
                    if ( user_optarr[i].option[strlen(user_optarr[i].option)-1] == ':' ) {  
                        *user_optarr[i].arg=optarg;
                    }
                    break;  /* option found - break out of the for loop */
                }
            }
	    /* This condition "should never happen".  SO CHECK FOR IT!!!! */
            if ( ! found ) { 
                sprintf(Mesg2,
		    "parse_opts: ERROR - option:\"%c\" NOT FOUND... INTERNAL ERROR", opt);
                return(Mesg2);
            }
	}

    }    /* end of while */
    free(optionstr);

    STD_argind = optind;

    /*
     * Turn on debug
     */
    if ( (ptr=getenv("USC_DEBUG")) != NULL ) {
	Debug=1;
        printf("env USC_DEBUG is defined, turning on debug\n");
    }
    if ( (ptr=getenv("USC_VERBOSE")) != NULL ) {
	Debug=1;
        printf("env USC_VERBOSE is defined, turning on debug\n");
    }

    /*
     * If the USC_ITERATION_ENV environmental variable is set to
     * a number, use that number as iteration count (same as -c option).
     * The -c option with arg will be used even if this env var is set.
     */
    if ( !(options & OPT_iteration) && (ptr=getenv(USC_ITERATION_ENV)) != NULL ) {
        if ( sscanf(ptr, "%i", &k) == 1) {
            if ( k == 0 ) {   /* if arg is 0, set infinite loop flag */
                STD_INFINITE=1;
		if ( Debug )
		   printf("Using env %s, set STD_INFINITE to 1\n",
			USC_ITERATION_ENV);
            } else {            /* else, set the loop count to the arguement */
                STD_LOOP_COUNT=k;
		if ( Debug )
		   printf("Using env %s, set STD_LOOP_COUNT to %d\n",
			USC_ITERATION_ENV, k);
            }
        }
    }

    /*
     * If the USC_NO_FUNC_CHECK environmental variable is set, we'll
     * unset the STD_FUNCTIONAL_TEST variable.
     */
    if ( !(options & OPT_nofunccheck) && (ptr=getenv(USC_NO_FUNC_CHECK)) != NULL ) {
        STD_FUNCTIONAL_TEST=0; /* Turn off functional testing */
	if ( Debug )
	    printf("Using env %s, set STD_FUNCTIONAL_TEST to 0\n",
		USC_NO_FUNC_CHECK);
    }

    /*
     * If the USC_LOOP_WALLTIME environmental variable is set,
     * use that number as duration (same as -I option).
     * The -I option with arg will be used even if this env var is set.
     */

    if ( !(options & OPT_duration) && (ptr=getenv(USC_LOOP_WALLTIME)) != NULL ) {
        if ( sscanf(ptr, "%f", &ftmp) == 1 && ftmp >= 0.0 ) {
	    STD_LOOP_DURATION=ftmp;
	    if ( Debug )
	        printf("Using env %s, set STD_LOOP_DURATION to %f\n",
		    USC_LOOP_WALLTIME, ftmp);
            if ( STD_LOOP_DURATION == 0.0 ) {   /* if arg is 0, set infinite loop flag */
                STD_INFINITE=1;
	        if ( Debug )
	            printf("Using env %s, set STD_INFINITE to 1\n", USC_LOOP_WALLTIME);
	    }
        }
    }
    if ( !(options & OPT_duration) && (ptr=getenv("USC_DURATION")) != NULL ) {
        if ( sscanf(ptr, "%f", &ftmp) == 1 && ftmp >= 0.0 ) {
	    STD_LOOP_DURATION=ftmp;
	    if ( Debug )
	        printf("Using env USC_DURATION, set STD_LOOP_DURATION to %f\n", ftmp);
            if ( STD_LOOP_DURATION == 0.0 ) {   /* if arg is 0, set infinite loop flag */
                STD_INFINITE=1;
	        if ( Debug )
	            printf("Using env USC_DURATION, set STD_INFINITE to 1\n");
	    }
        }
    }
    /*
     * If the USC_LOOP_DELAY environmental variable is set,
     * use that number as delay in factional seconds (same as -P option).
     * The -P option with arg will be used even if this env var is set.
     */
    if ( !(options & OPT_delay) && (ptr=getenv(USC_LOOP_DELAY)) != NULL ) {
        if ( sscanf(ptr, "%f", &ftmp) == 1 && ftmp >= 0.0 ) {
	    STD_LOOP_DELAY=ftmp;
	    if ( Debug )
		printf("Using env %s, set STD_LOOP_DELAY = %f\n",
		    USC_LOOP_DELAY, ftmp);
        }
    }

    /*
     * If the USC_COPIES environmental variable is set,
     * use that number as copies (same as -c option).
     * The -c option with arg will be used even if this env var is set.
     */
    if ( !(options & OPT_copies) && (ptr=getenv(USC_COPIES)) != NULL ) {
        if ( sscanf(ptr, "%d", &STD_COPIES) == 1 && STD_COPIES >= 0 ) {
	    if ( Debug )
		printf("Using env %s, set STD_COPIES = %d\n",
		    USC_COPIES, STD_COPIES);
        }
    }

    /*
     * The following are special system testing envs to turn on special
     * hooks in the code.
     */
    if ( (ptr=getenv("USC_TP_BARRIER")) != NULL ) {
        if ( sscanf(ptr, "%i", &k) == 1 && k >= 0 ) {
	    STD_TP_barrier=k;
	}
        else
	    STD_TP_barrier=1;
	if ( Debug )
	    printf("using env USC_TP_BARRIER, Set STD_TP_barrier to %d\n",
	        STD_TP_barrier);
    }

    if ( (ptr=getenv("USC_LP_BARRIER")) != NULL ) {
        if ( sscanf(ptr, "%i", &k) == 1 && k >= 0 ) {
	    STD_LP_barrier=k;
	}
        else
	    STD_LP_barrier=1;
	if ( Debug ) 
	    printf("using env USC_LP_BARRIER, Set STD_LP_barrier to %d\n",
	        STD_LP_barrier);
    }

    if ( (ptr=getenv("USC_TP_SHMEM")) != NULL ) {
        if ( sscanf(ptr, "%i", &k) == 1 && k >= 0 ) {
            STD_TP_shmem_sz=k;
	    if ( Debug )
	        printf("Using env USC_TP_SHMEM, Set STD_TP_shmem_sz to %d\n",
		    STD_TP_shmem_sz);
        }
    }

    if ( (ptr=getenv("USC_LP_SHMEM")) != NULL ) {
        if ( sscanf(ptr, "%i", &k) == 1 && k >= 0 ) {
            STD_LP_shmem=k;
	    if ( Debug )
	        printf("Using env USC_LP_SHMEM, Set STD_LP_shmem to %d\n",
		    STD_LP_shmem);
        }
    }

    if ( (ptr=getenv("USC_LD_SHMEM")) != NULL ) {
        if ( sscanf(ptr, "%i", &k) == 1 && k >= 0 ) {
            STD_LD_shmem=k;
	    if ( Debug )
	        printf("Using env USC_LD_SHMEM, Set STD_LD_shmem to %d\n",
		    STD_LD_shmem);
        }
    }

    if ( (ptr=getenv("USC_TP_SBRK")) != NULL ) {
        if ( sscanf(ptr, "%i", &k) == 1 && k >= 0 ) {
            STD_TP_sbrk=k;
	    if ( Debug )
	        printf("Using env USC_TP_SBRK, Set STD_TP_sbrk to %d\n",
		    STD_TP_sbrk);
        }
    }

#if !defined(UCLINUX)
    if ( (ptr=getenv("USC_LP_SBRK")) != NULL ) {
        if ( sscanf(ptr, "%i", &k) == 1 && k >= 0 ) {
            STD_LP_sbrk=k;
	    if ( Debug )
	        printf("Using env USC_LP_SBRK, Set STD_LP_sbrk to %d\n",
		    STD_LP_sbrk);
        }
    }
#endif /* if !defined(UCLINUX) */

    if ( (ptr=getenv("USC_LP_RECFUN")) != NULL ) {
        if ( sscanf(ptr, "%i", &k) == 1 && k >= 0 ) {
	    STD_LP_recfun=k;
	    if ( STD_bigstack != (struct usc_bigstack_t *)NULL )
		STD_bigstack=(struct usc_bigstack_t *)
			malloc(sizeof(struct usc_bigstack_t));
	    if ( Debug )
                printf("Using env USC_LP_RECFUN, Set STD_LP_recfun to %d\n",
		    STD_LP_recfun);
        }
    }

    if ( (ptr=getenv("USC_LD_RECFUN")) != NULL ) {
        if ( sscanf(ptr, "%i", &k) == 1 && k >= 0 ) {
	    STD_LD_recfun=k;
	    if ( STD_bigstack != (struct usc_bigstack_t *)NULL )
		STD_bigstack=(struct usc_bigstack_t *)
			malloc(sizeof(struct usc_bigstack_t));
	    if ( Debug )
                printf("Using env USC_LD_RECFUN, Set STD_LD_recfun to %d\n",
		    STD_LD_recfun);
        }
    }
 
#if UNIT_TEST
    printf("The following variables after option and env parsing:\n");
    printf("STD_FUNCTIONAL_TEST = %d\n", STD_FUNCTIONAL_TEST);
    printf("STD_LOOP_DURATION   = %f\n", STD_LOOP_DURATION);
    printf("STD_LOOP_DELAY      = %f\n", STD_LOOP_DELAY);
    printf("STD_COPIES          = %d\n", STD_COPIES);
    printf("STD_LOOP_COUNT      = %d\n", STD_LOOP_COUNT);
    printf("STD_INFINITE        = %d\n", STD_INFINITE);
    printf("STD_TIMING_ON       = %d\n", STD_TIMING_ON);
    printf("STD_ERRNO_LOG       = %d\n", STD_ERRNO_LOG);
    printf("STD_PAUSE           = %d\n", STD_PAUSE);
#endif

    return((char *) NULL);

}    /* end of parse_opts */

/*********************************************************************
 * print_help() - print help message and user help message
 *********************************************************************/
void print_help(void (*user_help)(void))
{
    STD_opts_help();

    if (user_help) user_help();
}

/*********************************************************************
 * STD_opts_help() - return a help string for the STD_OPTIONS.
 *********************************************************************/
void
STD_opts_help(void)
{
    int i;

    for(i = 0; std_options[i].optstr; ++i) {
	if (std_options[i].help)
	    fputs(std_options[i].help, stdout);
    }
}

/* 
 * routine to goto when we get the SIGUSR1 for STD_PAUSE
 */
void STD_go(int sig)
{
   return;
}

/***********************************************************************
 * This function will do desired end of global setup test
 * hooks.
 * Currently it will only do a pause waiting for sigusr1 if
 * STD_PAUSE is set.
 *
 ***********************************************************************/
int
usc_global_setup_hook(void)
{
#ifndef UCLINUX
    int cnt;
    /* temp variable to store old signal action to be restored after pause */
    int (*_TMP_FUNC)(void);

    /*
     * Fork STD_COPIES-1 copies.
     */
    for(cnt=1;cnt<STD_COPIES;cnt++) {
        switch(fork() ) {
	    case -1:
		fprintf(stderr, "%s: fork() failed, errno:%d %s\n",
                __FILE__, errno, strerror(errno));
		break;
	    case 0:  /* child */
	        cnt=STD_COPIES;   /* to stop the forking */
		break;

	    default: /* parent */
		break;
	}
    }
    
    /*
     * pause waiting for sigusr1.
     */
    if ( STD_PAUSE ) {                                      
        _TMP_FUNC = (int (*)(void))signal(SIGUSR1, STD_go);   
        pause();                                          
        signal(SIGUSR1, (void (*)(int))_TMP_FUNC);          
    }

#if !defined(UCLINUX)

    if ( STD_TP_sbrk || STD_LP_sbrk) {
	STD_start_break=sbrk(0);	/* get original sbreak size */
    }

    if ( STD_TP_sbrk ) {
	sbrk(STD_TP_sbrk);
	if ( Debug ) 
	    printf("after sbrk(%d)\n", STD_TP_sbrk);
    }

#endif /* if !defined(UCLINUX) */
#endif
    return 0;
}

#define USECS_PER_SEC	1000000  /* microseconds per second */

/***********************************************************************
 * This function returns the number of get_current_time()'s return
 * per second.
 ***********************************************************************/

static int
get_timepersec(void)
{
    return  USECS_PER_SEC;   /* microseconds per second */

}

/***********************************************************************
 * this function will get current time in microseconds since 1970.
 ***********************************************************************/
static int
get_current_time(void)
{
    struct timeval curtime;

    gettimeofday(&curtime, NULL);

    /* microseconds since 1970 */
    return (curtime.tv_sec*USECS_PER_SEC) + curtime.tv_usec;


}

/***********************************************************************
 *
 * This function will determine if test should continue iterating
 * If the STD_INFINITE flag is set, return 1.
 * If the STD_LOOP_COUNT variable is set, compare it against
 * the counter.
 * If the STD_LOOP_DURATION variable is set, compare current time against
 * calculated stop_time.
 * This function will return 1 until all desired looping methods
 * have been met.  
 *
 * counter integer is supplied by the user program.
 ***********************************************************************/
int
usc_test_looping(counter)
int counter;
{
    static int first_time = 1;
    static int stop_time = 0;	/* stop time in rtc or usecs */
    static int delay;		/* delay in clocks or usecs  */
    int hertz=0;			/* clocks per second or usecs per second */
    int ct, end;		/* current time, end delay time */
    int keepgoing=0;		/* used to determine return value */

    /*
     * If this is the first iteration and we are looping for 
     * duration of STD_LOOP_DURATION seconds (fractional) or
     * doing loop delays, get the clocks per second.
     */
    if ( first_time ) {

	first_time=0;
	if ( STD_LOOP_DELAY || STD_LOOP_DURATION ) {
	    hertz = get_timepersec();
	}

	/*
	 * If looping for duration, calculate stop time in
	 * clocks.
	 */
	
	if ( STD_LOOP_DURATION) {
	    ct=get_current_time();
	    stop_time=(int)((float)hertz * STD_LOOP_DURATION) + ct;
	}

	/*
	 * If doing delay each iteration, calcuate the number
	 * of clocks for each delay.
	 */
	if ( STD_LOOP_DELAY ) {
	    delay=(int)((float)hertz * STD_LOOP_DELAY);
	}

    }
	
    /*
     * if delay each iteration, loop for delay clocks.
     * This will not be done on first iteration.
     * The delay will happen before determining if
     * there will be another iteration.
     */
    else if ( STD_LOOP_DELAY ) {
	ct=get_current_time();
        end=ct+delay;
        while ( ct < end ) {
	    /*
	     * The following are special test hooks in the delay loop.
	     */
	    if ( STD_LD_recfun ) {
		if ( Debug )
		    printf("calling usc_recressive_func(0, %d, STD_bigstack)\n", 
		        STD_LD_recfun);
		usc_recressive_func(0, STD_LD_recfun, STD_bigstack);
	    }

	    ct=get_current_time();
	}
    }

    if ( STD_INFINITE ) {
	keepgoing++;
    }

    if ( STD_LOOP_COUNT && counter < STD_LOOP_COUNT ) {
	keepgoing++;
    }

    if ( STD_LOOP_DURATION != 0.0 && get_current_time() < stop_time ) {
	keepgoing++;
    }

    if ( keepgoing == 0 )
	return 0;

    /*
     * The following code allows special system testing hooks.
     */

    if ( STD_LP_recfun ) {
	if ( Debug )
	    printf("calling usc_recressive_func(0, %d, STD_bigstack)\n", 
	        STD_LP_recfun);
	usc_recressive_func(0, STD_LP_recfun, STD_bigstack);
    }

#if !defined(UCLINUX)

    if ( STD_LP_sbrk ) {
	if ( Debug )
	    printf("about to do sbrk(%d)\n", STD_LP_sbrk);
	sbrk(STD_LP_sbrk);
    }
#endif


    if ( keepgoing )
	return 1;
    else
        return 0;	/* done - stop iterating */
}


/*
 * This function recressively calls itself max times.
 */ 
static void
usc_recressive_func(cnt, max, bstack)
int cnt;
int max;
struct usc_bigstack_t *bstack;
{
    if ( cnt < max )
	usc_recressive_func(cnt+1, max, bstack);

}

#if UNIT_TEST
/******************************************************************************
 * UNIT TEST CODE
 * UNIT TEST CODE
 * 
 * this following code is provide so that unit testing can
 * be done fairly easily.
 ******************************************************************************/

int Help = 0;
int Help2 = 0;
char *ptr;

/*
 * Code from usctest.h that not part of this file since we are the library.
 */

struct usc_errno_t TEST_VALID_ENO[USC_MAX_ERRNO];

  /***********************************************************************
   * Globals for returning the return code and errno from the system call
   * test macros.
   ***********************************************************************/
int TEST_RETURN;
int TEST_ERRNO;

  /***********************************************************************
   * temporary variables for determining max and min times in TEST macro
   ***********************************************************************/
long btime, etime, tmptime;



/* for test specific parse_opts options */
option_t Options[] = {
        { "help",  &Help2, NULL },      /* -help option */
        { "h",  &Help, NULL },          /* -h option */
 	{ TIMING, NULL, NULL},		/* disable -timing option */

#if INVALID_TEST_CASES
 	{ "missingflag", NULL, &ptr },  /* error */
 	{ "missingarg:", &Help, NULL },  /* error */
#endif  /* INVALID_TEST_CASES */

        { NULL, NULL, NULL }
};


main(argc, argv)
int argc;
char **argv;
{
    int lc;
    char *msg;
    struct timeval t;
    int cnt;

    if ( (msg=parse_opts(argc, argv,
			(option_t *) Options)) != (char *) NULL ) {
	printf("ERROR : %s\n", msg);
	exit(1);
    }

    TEST_PAUSE;

    for (lc=0; TEST_LOOPING(lc); lc++) {
    
        TEST( gettimeofday(&t, NULL) );
        printf("iter=%d: sec:%d, usec:%6.6d %s", lc+1, t.tv_sec,
	    t.tv_usec, ctime(&t.tv_sec));
    }


    TEST_CLEANUP;

    exit(0);
}

#endif /* UNIT_TEST */
