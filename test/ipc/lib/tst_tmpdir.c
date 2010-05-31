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
 *    FUNCTION NAME     : tst_tmpdir, tst_rmdir
 *
 *    FUNCTION TITLE    : Create/remove a testing temp dir
 *
 *    SYNOPSIS:
 *      void tst_tmpdir();
 *      void tst_rmdir();
 *
 *    AUTHOR            : Dave Fenner
 *
 *    INITIAL RELEASE   : UNICOS 8.0
 *
 *    DESCRIPTION
 *      tst_tmpdir() is used to create a unique, temporary testing
 *      directory, and make it the current working directory.
 *      tst_rmdir() is used to remove the directory created by
 *      tst_tmpdir().
 *
 *      Setting the env variable "TDIRECTORY" will override the creation
 *      of a new temp dir.  The directory specified by TDIRECTORY will
 *      be used as the temporary directory, and no removal will be done
 *      in tst_rmdir().
 *
 *    RETURN VALUE
 *      Neither tst_tmpdir() or tst_rmdir() has a return value.
 *
 *#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#**/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>        /* for getenv() */
#include <string.h>        /* for string functions */
#include <unistd.h>        /* for sysconf(), getcwd(), rmdir() */
#include <sys/types.h>     /* for mkdir() */
#include <sys/stat.h>      /* for mkdir() */
#include "test.h"
#include "rmobj.h"

/*
 * Define some useful macros.
 */
#define PREFIX_SIZE     4
#define STRING_SIZE     256
#define DIR_MODE        0777  /* mode of tmp dir that will be created */

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif

/*
 * Define function prototypes.
 */
static void tmpdir_cleanup(void);

/*
 * Define global variables.
 */
extern char *TCID;            /* defined/initialized in main() */
extern int  TST_TOTAL;        /* defined/initialized in main() */
extern char *TESTDIR;         /* the directory created; defined in */
                              /* tst_res.c */

/*
 * tst_tmpdir() - Create a unique temporary directory and chdir() to it.
 *                It expects the caller to have defined/initialized the
 *                TCID/TST_TOTAL global variables.  The TESTDIR global
 *                variable will be set to the directory that gets used
 *                as the testing directory.
 *
 *                NOTE: This function must be called BEFORE any activity
 *                that would require CLEANUP.  If tst_tmpdir() fails, it
 *                cleans up afer itself and calls tst_exit() (i.e. does
 *                not return).
 */
#undef   FN_NAME
#define  FN_NAME  "tst_tmpdir()"

void
tst_tmpdir()
{
 	char template[PATH_MAX];      /* template for mkstemp, mkdtemp */
  	int  no_cleanup = 0;          /* !0 means TDIRECTORY env var was set */
	char *env_tmpdir;            /* temporary storage for TMPDIR env var */
/* This is an AWEFUL hack to figure out if mkdtemp() is available */
#if defined(__GLIBC_PREREQ)
# if __GLIBC_PREREQ(2,2)
#  define HAVE_MKDTEMP 1
# else
#  define HAVE_MKDTEMP 0
	int tfd;
# endif
#else 
# define HAVE_MKDTEMP 0
	int tfd;
#endif
   	/*
	 * If the TDIRECTORY env variable is not set, a temp dir will be
	 * created.
	 */
	if ((TESTDIR = getenv(TDIRECTORY))) {
		/*
		 * The TDIRECTORY env. variable is set, so no temp dir is created.
		 * Also, no clean up will be done via tst_rmdir().
		 */
		no_cleanup++;
#if UNIT_TEST
		printf("TDIRECTORY env var is set\n");
#endif
	} else {
		/*
		 * Create a template for the temporary directory.  Use the 
		 * environment variable TMPDIR if it is available, otherwise
		 * use our default TEMPDIR.
		 */
		if ((env_tmpdir = getenv("TMPDIR"))) {
			snprintf(template, PATH_MAX, "%s/%.3sXXXXXX", env_tmpdir, TCID);
		} else {
			snprintf(template, PATH_MAX, "%s/%.3sXXXXXX", TEMPDIR, TCID);
		}
		

#if HAVE_MKDTEMP
		/*
		 * Make the temporary directory in one shot using mkdtemp()
		 */
		if (mkdtemp(template) == NULL)
			tst_brkm(TBROK, tmpdir_cleanup, 
				"%s: mkdtemp(%s) failed; errno = %d: %s", 
				FN_NAME, template, errno, strerror(errno));
		TESTDIR = strdup(template);
#else 
		/*
		 * Make the template name, then the directory
		 */
		if ((tfd = mkstemp(template)) == -1)
			tst_brkm(TBROK, tmpdir_cleanup, 
				"%s: mkstemp(%s) failed; errno = %d: %s", 
				FN_NAME, template, errno, strerror(errno));
		close(tfd);
		unlink(template);
		TESTDIR = strdup(template);
		if (mkdir(TESTDIR, DIR_MODE)) {
			/* If we start failing with EEXIST, wrap this section in 
			 * a loop so we can try again.
			 */
			tst_brkm(TBROK, tmpdir_cleanup, 
				"%s: mkdir(%s, %#o) failed; errno = %d: %s", 
				FN_NAME, TESTDIR, DIR_MODE, errno, 
				strerror(errno));
		}
#endif

 	}
		/*
		 * Change the group on this temporary directory to be that of the
		 * gid of the person running the tests and permissions to 777.
		 */
		if ( chown(TESTDIR, -1, getgid()) == -1 )
			tst_brkm(TBROK, tmpdir_cleanup, 
				"chown(%s, -1, %d) failed; errno = %d: %s", 
				TESTDIR, getgid(), errno, strerror(errno));
		if ( chmod(TESTDIR,S_IRWXU | S_IRWXG | S_IRWXO) == -1 )
			tst_brkm(TBROK, tmpdir_cleanup,
				"chmod(%s,777) failed; errno %d: %s",
				TESTDIR, errno, strerror(errno)); 

#if UNIT_TEST
	printf("TESTDIR = %s\n", TESTDIR);
#endif

 	/*
  	 * Change to the temporary directory.  If the chdir() fails, issue
   	 * TBROK messages for all test cases, attempt to remove the
	 * directory (if it was created), and exit.  If the removal also
	 * fails, also issue a TWARN message.   
	 */
	if ( chdir(TESTDIR) == -1 ) {
		tst_brkm(TBROK, NULL, "%s: chdir(%s) failed; errno = %d: %s",
				FN_NAME, TESTDIR, errno, strerror(errno) );

		/* Try to remove the directory */
		if ( !no_cleanup && rmdir(TESTDIR) == -1 )
			tst_resm(TWARN, "%s: rmdir(%s) failed; errno = %d: %s",
				FN_NAME, TESTDIR, errno, strerror(errno) );

		tmpdir_cleanup();
	}
	
#if UNIT_TEST
	printf("CWD is %s\n", getcwd((char *)NULL, PATH_MAX));
#endif

	/*
	 *  If we made through all this stuff, return.
	 */
	return;
}  /* tst_tmpdir() */


/*
 *
 * tst_rmdir() - Recursively remove the temporary directory created by
 *               tst_tmpdir().  This function is intended ONLY as a
 *               companion to tst_tmpdir().  If the TDIRECTORY
 *               environment variable is set, no cleanup will be
 *               attempted.
 */ 
#undef   FN_NAME
#define  FN_NAME  "tst_rmdir()"

void
tst_rmdir()
{
   char *errmsg;
   char *tdirectory;
   char current_dir[PATH_MAX];   /* current working directory */
   char parent_dir[PATH_MAX];    /* directory above TESTDIR */
   char *basename;               /* basename of the TESTDIR */

   /*
    * If the TDIRECTORY env variable is set, this indicates that no
    * temp dir was created by tst_tmpdir().  Thus no cleanup will be
    * necessary.
    */
   if ( (tdirectory = getenv(TDIRECTORY)) != NULL ) {
#if UNIT_TEST
      printf("\"TDIRECORY\" env variable is set; no cleanup was performed\n");
#endif
      return;
   }
   
   /*
    * Check that TESTDIR is not NULL.
    */
   if ( TESTDIR == NULL ) {
      tst_resm(TWARN, "%s: TESTDIR was NULL; no removal attempted",
               FN_NAME);
      return;
   }

   /*
    * Check that the value of TESTDIR is not "*" or "/".  These could
    * have disastrous effects in a test run by root.
    */
   if ( strcmp(TESTDIR, "/") == 0 ) {
      tst_resm(TWARN,
               "%s: Recursive remove of root directory not attempted",
               FN_NAME);
      return;
   }

   if ( strchr(TESTDIR, '*') != NULL ) {
      tst_resm(TWARN, "%s: Recursive remove of '*' not attempted",
               FN_NAME);
      return;
   }

   /*
    * Get the directory name of TESTDIR.  If TESTDIR is a relative path,
    * get full path.
    */
   if ( TESTDIR[0] != '/' ) {
      if ( getcwd(current_dir,PATH_MAX) == NULL )
         strcpy(parent_dir, TESTDIR);
      else
         sprintf(parent_dir, "%s/%s", current_dir, TESTDIR);
   } else {
      strcpy(parent_dir, TESTDIR);
   }
   if ( (basename = strrchr(parent_dir, '/')) != NULL ) {
      *basename='\0';   /* terminate at end of parent_dir */
   }

   /*
    * Change directory to parent_dir (The dir above TESTDIR).
    */
   if ( chdir(parent_dir) != 0 )
      tst_resm(TWARN,
               "%s: chdir(%s) failed; errno = %d: %s\nAttempting to remove temp dir anyway",
               FN_NAME, parent_dir, errno, strerror(errno));
   
   /*
    * Attempt to remove the "TESTDIR" directory, using rmobj().
    */
   if ( rmobj(TESTDIR, &errmsg) == -1 )
      tst_resm(TWARN, "%s: rmobj(%s) failed: %s",
               FN_NAME, TESTDIR, errmsg);

   return;
}  /* tst_rmdir() */


/*
 * tmpdir_cleanup() - This function is used when tst_tmpdir()
 *                    encounters an error, and must cleanup and exit.
 *                    It prints a warning message via tst_resm(), and
 *                    then calls tst_exit().
 */
#undef  FN_NAME
#define FN_NAME "tst_tmpdir()"

static void
tmpdir_cleanup()
{
   /*
    * Print a warning message and call tst_exit() to exit the test.
    */
   tst_resm(TWARN, "%s: No user cleanup function called before exiting",
            FN_NAME);
   tst_exit();
}  /* tmpdir_cleanup() */


#ifdef UNIT_TEST
/****************************************************************************
 * Unit test code: Takes input from stdin and can make the following
 *                 calls: tst_tmpdir(), tst_rmdir().
 ****************************************************************************/
int  TST_TOTAL = 10;
char *TCID = "TESTTCID";

main()
{
   int  option;
   char *chrptr;

   printf("UNIT TEST of tst_tmpdir.c.  Options to try:\n\
   -1 : call tst_exit()\n\
    0 : call tst_tmpdir()\n\
    1 : call tst_rmdir()\n\n");

   while ( 1 ) {
      printf("Enter options (-1, 0, 1): ");
      (void) scanf("%d%c", &option, &chrptr);

      switch ( option ) {
      case -1:
         tst_exit();
         break;

      case 0:
         tst_tmpdir();
         break;

      case 1:
         tst_rmdir();
         break;
      }  /* switch() */
   }  /* while() */
}
#endif  /* UNIT_TEST */
