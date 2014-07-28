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
 *    FUNCTION NAME     :
 *      tst_res() -       Print result message (include file contents)
 *      tst_resm() -      Print result message
 *      tst_brk() -       Print result message (include file contents)
 *                        and break remaining test cases
 *      tst_brkm() -      Print result message and break remaining test
 *                        cases
 *      tst_brkloop() -   Print result message (include file contents)
 *                        and break test cases remaining in current loop
 *      tst_brkloopm() -  Print result message and break test case
 *                        remaining in current loop
 *      tst_flush() -     Print any messages pending because of
 *                        CONDENSE mode, and flush output stream
 *      tst_exit() -      Exit test with a meaningful exit value.
 *      tst_environ() -   Keep results coming to original stdout
 *
 *    FUNCTION TITLE    : Standard automated test result reporting mechanism
 *
 *    SYNOPSIS:
 *      #include "test.h"
 *
 *      void tst_res(ttype, fname, tmesg [,arg]...)
 *      int  ttype;
 *      char *fname;
 *      char *tmesg;
 *
 *      void tst_resm(ttype, tmesg [,arg]...)
 *      int  ttype;
 *      char *tmesg;
 *
 *      void tst_brk(ttype, fname, cleanup, tmesg, [,argv]...)
 *      int  ttype;
 *      char *fname;
 *      void (*cleanup)();
 *      char *tmesg;
 *
 *      void tst_brkm(ttype, cleanup, tmesg [,arg]...)
 *      int  ttype;
 *      void (*cleanup)();
 *      char *tmesg;
 *
 *      void tst_brkloop(ttype, fname, cleanup, char *tmesg, [,argv]...)
 *      int  ttype;
 *      char *fname;
 *      void (*cleanup)();
 *      char *tmesg;
 *
 *      void tst_brkloopm(ttype, cleanup, tmesg [,arg]...)
 *      int  ttype;
 *      void (*cleanup)();
 *      char *tmesg;
 *
 *      void tst_flush()
 *
 *      void tst_exit()
 *
 *      int  tst_environ()
 *
 *    AUTHOR            : Kent Rogers (from Dave Fenner's original)
 *
 *    CO-PILOT          : Rich Logan
 *
 *    DATE STARTED      : 05/01/90 (rewritten 1/96)
 *
 *    DESCRIPTION
 *      See the man page(s).
 *
 *#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#**/
#include <errno.h>
#include <string.h>
#include <stdio.h>         /* for I/O functions, BUFSIZ */
#include <stdlib.h>        /* for getenv() */
#include <stdarg.h>        /* for varargs stuff */
#include <unistd.h>        /* for access() */
#include "test.h"          /* for output display mode & result type */
                           /* defines */

/*
 * Define some useful macros.
 */
#define VERBOSE      1     /* flag values for the T_mode variable */
#define CONDENSE     2
#define NOPASS       3
#define DISCARD      4

#define MAXMESG      80    /* max length of internal messages */
#define USERMESG     2048  /* max length of user message */
#define TRUE         1
#define FALSE        0

/*
 * EXPAND_VAR_ARGS - Expand the variable portion (arg_fmt) of a result
 *                   message into the specified string.
 */
#define EXPAND_VAR_ARGS(arg_fmt, str) {   \
   va_list ap; /* varargs mechanism */    \
                                          \
   if ( arg_fmt != NULL ) {               \
      if ( Expand_varargs == TRUE ) {     \
         va_start(ap, arg_fmt);           \
         vsprintf(str, arg_fmt, ap);      \
         va_end(ap);                      \
         Expand_varargs = FALSE;          \
      } else {                            \
         strcpy(str, arg_fmt);            \
      }                                   \
   } else {                               \
      str[0] = '\0';                      \
   }                                      \
}  /* EXPAND_VAR_ARGS() */

/*
 * Define local function prototypes.
 */
static void check_env(void);
static void tst_condense(int tnum, int ttype, char *tmesg);
static void tst_print(char *tcid, int tnum, int trange, int ttype, char *tmesg);
static void cat_file(char *filename);


/*
 * Define some static/global variables.
 */
static FILE *T_out = NULL;    /* tst_res() output file descriptor */
static char *File;            /* file whose contents is part of result */
static int  T_exitval = 0;    /* exit value used by tst_exit() */
static int  T_mode = VERBOSE; /* flag indicating print mode: VERBOSE, */
                              /* CONDENSE, NOPASS, DISCARD */

static int  Expand_varargs = TRUE;  /* if TRUE, expand varargs stuff */
static char Warn_mesg[MAXMESG];  /* holds warning messages */

/*
 * These are used for condensing output when NOT in verbose mode.
 */
static int  Buffered = FALSE; /* TRUE if condensed output is currently */
                              /* buffered (i.e. not yet printed) */
static char *Last_tcid;       /* previous test case id */
static int  Last_num;         /* previous test case number */
static int  Last_type;        /* previous test result type */
static char *Last_mesg;       /* previous test result message */


/*
 * These globals may be externed by the test.
 */
int Tst_count = 0;      /* current count of test cases executed; NOTE: */
                        /* Tst_count may be externed by other programs */
int Tst_lptotal = 0;    /* tst_brkloop() external */
int Tst_lpstart = 0;    /* tst_brkloop() external */
int Tst_range = 1;      /* for specifying multiple results */
int Tst_nobuf = 1;      /* this is a no-op; buffering is never done, but */
                        /* this will stay for compatibility reasons */

/*
 * These globals must be defined in the test.
 */
extern char *TCID;      /* Test case identifier from the test source */
extern int  TST_TOTAL;  /* Total number of test cases from the test */
                        /* source */

/*
 * This global is used by the temp. dir. maintenance functions,
 * tst_tmpdir()/tst_rmdir(), tst_wildcard()/tst_tr_rmdir().  It is the
 * name of the directory created (if any).  It is defined here, so that
 * it only has to be declared once and can then be referenced from more
 * than one module.  Also, since the temp. dir. maintenance functions
 * rely on the tst_res.c package this seemed like a reasonable place.
 */
char *TESTDIR = NULL;

/*
 * tst_res() - Main result reporting function.  Handle test information
 *             appropriately depending on output display mode.  Call
 *             tst_condense() or tst_print() to actually print results.
 *             All result functions (tst_resm(), tst_brk(), etc.)
 *             eventually get here to print the results.
 */
void
tst_res(int ttype, char *fname, char *arg_fmt, ...)
{
   int  i;
   char tmesg[USERMESG];     /* expanded message */

#if DEBUG
   printf("IN tst_res; Tst_count = %d; Tst_range = %d\n",
          Tst_count, Tst_range); fflush(stdout);
#endif

   /*
    * Expand the arg_fmt string into tmesg, if necessary.
    */
   EXPAND_VAR_ARGS(arg_fmt, tmesg);

   /*
    * Save the test result type by ORing ttype into the current exit
    * value (used by tst_exit()).
    */
   T_exitval |= ttype;

   /*
    * Unless T_out has already been set by tst_environ(), make tst_res()
    * output go to standard output.
    */
   if ( T_out == NULL )
       T_out = stdout;

   /*
    * Check TOUTPUT environment variable (if first time) and set T_mode
    * flag.
    */
   check_env();

   /*
    * A negative or NULL range is invalid.
    */
   if ( Tst_range <= 0 ) {
      Tst_range = 1;
      tst_print(TCID, 0, 1, TWARN,
                "tst_res(): Tst_range must be positive");
   }

   /*
    * If a filename was specified, set 'File' if it exists.
    */
   if ( fname != NULL && access(fname, F_OK) == 0 )
      File = fname;

   /*
    * Set the test case number and print the results, depending on the
    * display type.
    */
   if ( ttype == TWARN || ttype == TINFO ) {
      /*
       * Handle WARN and INFO results (test case number is 0).
       */
      if ( Tst_range > 1 ) {
         tst_print(TCID, 0, 1, TWARN,
                   "tst_res(): Range not valid for TINFO or TWARN types");
      }
      tst_print(TCID, 0, 1, ttype, tmesg);
   } else {
      /*
       * Handle all other types of results other than WARN and INFO.
       */
      if ( Tst_count < 0 )
         tst_print(TCID, 0, 1, TWARN,
                   "tst_res(): Tst_count < 0 is not valid");

      /*
       * Process each display type.
       */
      switch ( T_mode ) {
      case DISCARD:
         /* do not print any results */
         break;

      case NOPASS:   /* passing result types are filtered by tst_print() */
      case CONDENSE:
         tst_condense(Tst_count + 1, ttype, tmesg);
         break;

      default: /* VERBOSE */
         for ( i = 1 ; i <= Tst_range ; i++ )
            tst_print(TCID, Tst_count + i, Tst_range, ttype, tmesg);
         break;
      }  /* end switch() */

      /*
       * Increment Tst_count.
       */
      Tst_count += Tst_range;
   }  /* if ( ttype == TWARN || ttype == TINFO ) */

   /*
    * Reset some values.
    */
   Tst_range = 1;
   Expand_varargs = TRUE;
}  /* tst_res() */


/*
 * tst_condense() - Handle test cases in CONDENSE or NOPASS mode (i.e.
 *                  buffer the current result and print the last result
 *                  if different than the current).  If a file was
 *                  specified, print the current result and do not
 *                  buffer it.
 */
static void
tst_condense(int tnum, int ttype, char *tmesg)
{
   char *file;

#if DEBUG
   printf("IN tst_condense: tcid = %s, tnum = %d, ttype = %d, tmesg = %s\n",
          TCID, tnum, ttype, tmesg);
   fflush(stdout);
#endif

   /*
    * If this result is the same as the previous result, return.
    */
   if ( Buffered == TRUE ) {
      if ( strcmp(Last_tcid, TCID) == 0 && Last_type == ttype &&
           strcmp(Last_mesg, tmesg) == 0 && File == NULL )
         return;

      /*
       * This result is different from the previous result.  First,
       * print the previous result.
       */
      file = File;
      File = NULL;
      tst_print(Last_tcid, Last_num, tnum - Last_num, Last_type,
                Last_mesg);
      free(Last_tcid);
      free(Last_mesg);
      File = file;
   }  /* if ( Buffered == TRUE ) */

   /*
    * If a file was specified, print the current result since we have no
    * way of retaining the file contents for comparing with future
    * results.  Otherwise, buffer the current result info for next time.
    */
   if ( File != NULL ) {
      tst_print(TCID, tnum, Tst_range, ttype, tmesg);
      Buffered = FALSE;
   } else {
      Last_tcid = (char *)malloc(strlen(TCID) + 1);
      strcpy(Last_tcid, TCID);
      Last_num = tnum;
      Last_type = ttype;
      Last_mesg = (char *)malloc(strlen(tmesg) + 1);
      strcpy(Last_mesg, tmesg);
      Buffered = TRUE;
   }
}  /* tst_condense() */


/*
 * tst_flush() - Print any messages pending because of CONDENSE mode,
 *               and flush T_out.
 */
void
tst_flush(void)
{
#if DEBUG
   printf("IN tst_flush\n");
   fflush(stdout);
#endif

   /*
    * Print out last line if in CONDENSE or NOPASS mode.
    */
   if ( Buffered == TRUE && (T_mode == CONDENSE || T_mode == NOPASS) ) {
      tst_print(Last_tcid, Last_num, Tst_count - Last_num + 1,
                Last_type, Last_mesg);
      Buffered = FALSE;
   }
   fflush(T_out);
}  /* tst_flush() */


/*
 * tst_print() - Actually print a line or range of lines to the output
 *               stream.
 */
static void
tst_print(char *tcid, int tnum, int trange, int ttype, char *tmesg)
{
   char type[5];

#if DEBUG
   printf("IN tst_print: tnum = %d, trange = %d, ttype = %d, tmesg = %s\n",
          tnum, trange, ttype, tmesg);
   fflush(stdout);
#endif

   /*
    * Save the test result type by ORing ttype into the current exit
    * value (used by tst_exit()).  This is already done in tst_res(), but
    * is also done here to catch internal warnings.  For internal warnings, 
    * tst_print() is called directly with a case of TWARN.
    */
   T_exitval |= ttype;

   /*
    * If output mode is DISCARD, or if the output mode is NOPASS and
    * this result is not one of FAIL, BROK, or WARN, just return.  This
    * check is necessary even though we check for DISCARD mode inside of
    * tst_res(), since occasionally we get to this point without going
    * through tst_res() (e.g. internal TWARN messages).
    */
   if ( T_mode == DISCARD || (T_mode == NOPASS && ttype != TFAIL &&
                              ttype != TBROK && ttype != TWARN) )
      return;

   /*
    * Fill in the type string according to ttype.
    */
   switch ( ttype ) {
   case TPASS:
      strcpy(type, "PASS");
      break;
   case TFAIL:
      strcpy(type, "FAIL");
      break;
   case TBROK:
      strcpy(type, "BROK");
      break;
   case TRETR:
      strcpy(type, "RETR");
      break;
   case TCONF:
      strcpy(type, "CONF");
      break;
   case TWARN:
      strcpy(type, "WARN");
      break;
   case TINFO:
      strcpy(type, "INFO");
      break;
   default:
      strcpy(type, "????");
      break;
   }  /* switch ( ttype ) */

   /*
    * Build the result line and print it.
    */
   if ( T_mode == VERBOSE ) {
      fprintf(T_out, "%-8s %4d  %s  :  %s\n", tcid, tnum, type, tmesg);
   } else {
      /* condense results if a range is specified */
      if ( trange > 1 )
         fprintf(T_out, "%-8s %4d-%-4d  %s  :  %s\n",
                 tcid, tnum, tnum + trange - 1, type, tmesg);
      else
         fprintf(T_out, "%-8s %4d       %s  :  %s\n",
                 tcid, tnum, type, tmesg);
   }

   /*
    * If tst_res() was called with a file, append file contents to the
    * end of last printed result.
    */
   if ( File != NULL )
      cat_file(File);
   File = NULL;
}  /* tst_print() */


/*
 * check_env() - Check the value of the environment variable TOUTPUT and
 *               set the global variable T_mode.  The TOUTPUT environment
 *               variable should be set to "VERBOSE", "CONDENSE",
 *               "NOPASS", or "DISCARD".  If TOUTPUT does not exist or
 *               is not set to a valid value, the default is "VERBOSE".
 */
static void
check_env(void)
{
   static int first_time = 1;
   char       *value;      /* value of TOUTPUT environment variable */

#if DEBUG
   printf("IN check_env\n");
   fflush(stdout);
#endif

   if ( !first_time )
      return;

   first_time = 0;

   if ( (value = getenv(TOUTPUT)) == NULL ) {
      /* TOUTPUT not defined, use default */
      T_mode = VERBOSE;
   } else if ( strcmp(value, TOUT_CONDENSE_S) == 0 ) {
      T_mode = CONDENSE;
   } else if ( strcmp(value, TOUT_NOPASS_S) == 0 ) {
      T_mode = NOPASS;
   } else if ( strcmp(value, TOUT_DISCARD_S) == 0 ) {
      T_mode = DISCARD;
   } else {
      /* default */
      T_mode = VERBOSE;
   }

   return;
}  /* check_env() */


/*
 * tst_exit() - Call exit() with the value T_exitval, set up by
 *              tst_res().  T_exitval has a bit set for most of the
 *              result types that were seen (including TPASS, TFAIL,
 *              TBROK, TWARN, TCONF).  Also, print the last result (if
 *              necessary) before exiting.
 */
void
tst_exit(void)
{
#if DEBUG
   printf("IN tst_exit\n"); fflush(stdout);
   fflush(stdout);
#endif

   /*
    * Call tst_flush() flush any ouput in the buffer or the last
    * result not printed because of CONDENSE mode.
    */
   tst_flush();

   /*
    * Mask out TRETR, TINFO, and TCONF results from the exit status.
    */
   exit(T_exitval & ~(TRETR | TINFO | TCONF));
}  /* tst_exit() */


/*
 * tst_environ() - Preserve the tst_res() output location, despite any
 *                 changes to stdout.
 */
int
tst_environ()
{
#if defined UCLINUX && EMBED && CONFIG_BLACKFIN
   FILE *fdopen;
#else
   FILE *fdopen(int fd, const char *mode);
#endif

   if ( (T_out = fdopen(dup(fileno(stdout)), "w")) == NULL )
      return(-1);
   else
      return(0);
}  /* tst_environ() */


/*
 * tst_brk() - Fail or break current test case, and break the remaining
 *             tests cases.
 */
void
tst_brk(int ttype, char *fname, void (*func)(void), char *arg_fmt, ...)
{
   char    tmesg[USERMESG];      /* expanded message */

#if DEBUG
   printf("IN tst_brk\n"); fflush(stdout);
   fflush(stdout);
#endif

   /*
    * Expand the arg_fmt string into tmesg, if necessary.
    */
   EXPAND_VAR_ARGS(arg_fmt, tmesg);

   /*
    * Only FAIL, BROK, CONF, and RETR are supported by tst_brk().
    */
   if ( ttype != TFAIL && ttype != TBROK && ttype != TCONF &&
        ttype != TRETR ) {
      sprintf(Warn_mesg, "tst_brk(): Invalid Type: %d.  Using TBROK",
              ttype);
      tst_print(TCID, 0, 1, TWARN, Warn_mesg);
      ttype = TBROK;
   }

   /*
    * Print the first result, if necessary.
    */
   if ( Tst_count < TST_TOTAL )
      tst_res(ttype, fname, tmesg);

   /*
    * Determine the number of results left to report.
    */
   Tst_range = TST_TOTAL - Tst_count;

   /*
    * Print the rest of the results, if necessary.
    */
   if ( Tst_range > 0 ) {
      if ( ttype == TCONF )
         tst_res(ttype, NULL,
                 "Remaining cases not appropriate for configuration");
      else if ( ttype == TRETR )
         tst_res(ttype, NULL, "Remaining cases retired");
      else
         tst_res(TBROK, NULL, "Remaining cases broken");
   } else {
      Tst_range = 1;
      Expand_varargs = TRUE;
   }  /* if ( Tst_range > 0 ) */

   /*
    * If no cleanup function was specified, just return to the caller.
    * Otherwise call the specified function.  If specified function
    * returns, call tst_exit().
    */
   if ( func != NULL ) {
      (*func)();
      tst_exit();
   }

   return;
}  /* tst_brk() */


/*
 * tst_brkloop() - Fail or break current test case, and break the
 *                 remaining test cases within test case loop.
 */
void
tst_brkloop(int ttype, char *fname, void (*func)(void), char *arg_fmt, ...)
{
   char    tmesg[USERMESG];      /* expanded message */

#if DEBUG
   printf("IN tst_brkloop\n"); fflush(stdout);
   fflush(stdout);
#endif

   /*
    * Expand the arg_fmt string into tmesg.
    */
   EXPAND_VAR_ARGS(arg_fmt, tmesg);

   /*
    * Verify that Tst_lpstart & Tst_lptotal are valid.
    */
   if ( Tst_lpstart < 0 || Tst_lptotal < 0 ) {
      tst_print(TCID, 0, 1, TWARN,
                "tst_brkloop(): Tst_lpstart & Tst_lptotal must both be assigned non-negative values");
      Expand_varargs = TRUE;
      return;
   }

   /*
    * Only FAIL, BROK, CONF, and RETR are supported by tst_brkloop().
    */
   if ( ttype != TFAIL && ttype != TBROK && ttype != TCONF &&
        ttype != TRETR ) {
      sprintf(Warn_mesg,
              "tst_brkloop(): Invalid Type: %d.  Using TBROK",
              ttype);
      tst_print(TCID, 0, 1, TWARN, Warn_mesg);
      ttype = TBROK;
   }

   /*
    * Print the first result, if necessary.
    */
   if ( Tst_count < Tst_lpstart + Tst_lptotal )
      tst_res(ttype, fname, tmesg);

   /*
    * Determine the number of results left to report.
    */
   Tst_range = Tst_lptotal + Tst_lpstart - Tst_count;

   /*
    * Print the rest of the results, if necessary.
    */
   if ( Tst_range > 0 ) {
      if ( ttype == TCONF )
         tst_res(ttype, NULL,
                 "Remaining cases in loop not appropriate for configuration");
      else if ( ttype == TRETR )
         tst_res(ttype, NULL, "Remaining cases in loop retired");
      else
         tst_res(TBROK, NULL, "Remaining cases in loop broken");
   } else {
      Tst_range = 1;
      Expand_varargs = TRUE;
   }  /* if ( Tst_range > 0 ) */

   /*
    * If a cleanup function was specified, call it.
    */
   if ( func != NULL )
      (*func)();
}  /* tst_brkloop() */


/*
 * tst_resm() - Interface to tst_res(), with no filename.
 */
void
tst_resm(int ttype, char *arg_fmt, ...)
{
   char    tmesg[USERMESG];      /* expanded message */

#if DEBUG
   printf("IN tst_resm\n"); fflush(stdout);
   fflush(stdout);
#endif

   /*
    * Expand the arg_fmt string into tmesg.
    */
   EXPAND_VAR_ARGS(arg_fmt, tmesg);

   /*
    * Call tst_res with a null filename argument.
    */
   tst_res(ttype, NULL, tmesg);
}  /* tst_resm() */


/*
 * tst_brkm() - Interface to tst_brk(), with no filename.
 */
void
tst_brkm(int ttype, void (*func)(void), char *arg_fmt, ...)
{
   char    tmesg[USERMESG];      /* expanded message */

#if DEBUG
   printf("IN tst_brkm\n"); fflush(stdout);
   fflush(stdout);
#endif

   /*
    * Expand the arg_fmt string into tmesg.
    */
   EXPAND_VAR_ARGS(arg_fmt, tmesg);

   /*
    * Call tst_brk with a null filename argument.
    */
   tst_brk(ttype, NULL, func, tmesg);
}  /* tst_brkm() */


/*
 * tst_brkloopm() - Interface to tst_brkloop(), with no filename.
 */
void
tst_brkloopm(int ttype, void (*func)(void), char *arg_fmt, ...)
{
   char    tmesg[USERMESG];      /* expanded message */

#if DEBUG
   printf("IN tst_brkloopm\n");
   fflush(stdout);
#endif

   /*
    * Expand the arg_fmt string into tmesg.
    */
   EXPAND_VAR_ARGS(arg_fmt, tmesg);

   /*
    * Call tst_brkloop with a null filename argument.
    */
   tst_brkloop(ttype, NULL, func, tmesg);
}  /* tst_brkloopm() */


/*
 * cat_file() - Print the contents of a file to standard out.
 */
static void
cat_file(char *filename)
{
   FILE *fp;                  /* file pointer */
   int  b_read;               /* number of bytes read with read() */
   int  b_written;            /* number of bytes written with write() */
   char buffer[BUFSIZ];       /* read/write buffer; BUFSIZ defined in */
                              /* stdio.h */

#if DEBUG
   printf("IN cat_file\n"); fflush(stdout);
#endif

   /*
    * Open the file for reading.
    */
   if ( (fp = fopen(filename, "r")) == NULL ) {
      sprintf(Warn_mesg,
              "tst_res(): fopen(%s, \"r\") failed; errno = %d: %s",
              filename, errno, strerror(errno));
      tst_print(TCID, 0, 1, TWARN, Warn_mesg);
      return;
   }  /* if ( fopen(filename, "r") == -1 ) */

   /*
    * While something to read, continue to read blocks.
    * From fread(3) man page: 
    *   If an error occurs, or the end-of-file is reached, the return
    *   value is zero.
    */
   errno = 0;
   while ( (b_read = fread((void *)buffer, 1, BUFSIZ, fp)) != (size_t)0 ) {
      /*
       * Write what was read to the result output stream.
       */
      if ( (b_written = fwrite((void *)buffer, 1, b_read, T_out)) !=
           b_read ) {
         sprintf(Warn_mesg,
                 "tst_res(): While trying to cat \"%s\", fwrite() wrote only %d of %d bytes",
                 filename, b_written, b_read);
         tst_print(TCID, 0, 1, TWARN, Warn_mesg);
         break;
      }  /* if ( b_written != b_read ) */
   }  /* while ( fread() != 0 ) */

   /*
    * Check for an fread() error.
    */
   if ( !feof(fp) ) {
      sprintf(Warn_mesg,
              "tst_res(): While trying to cat \"%s\", fread() failed, errno = %d: %s",
              filename, errno, strerror(errno));
      tst_print(TCID, 0, 1, TWARN, Warn_mesg);
   }  /* if ( !feof() ) */

   /*
    * Close the file.
    */
   if ( fclose(fp) == EOF ) {
      sprintf(Warn_mesg,
              "tst_res(): While trying to cat \"%s\", fclose() failed, errno = %d: %s",
              filename, errno, strerror(errno));
      tst_print(TCID, 0, 1, TWARN, Warn_mesg);
   }  /* if ( fclose(fp) == EOF ) */
}  /* cat_file() */



#ifdef UNIT_TEST
/****************************************************************************
 * Unit test code: Takes input from stdin and can make the following
 *                 calls: tst_res(), tst_resm(), tst_brk(), tst_brkm(),
 *                 tst_flush_buf(), tst_exit().
 ****************************************************************************/
int  TST_TOTAL = 10;
char *TCID = "TESTTCID";

#define RES  "tst_res.c UNIT TEST message; ttype = %d; contents of \"%s\":"
#define RESM "tst_res.c UNIT TEST message; ttype = %d"

main(void)
{
   int  ttype;
   int  range;
   char *chrptr;
   char chr;
   char fname[MAXMESG];

   printf("UNIT TEST of tst_res.c.  Options for ttype:\n\
   -1 : call tst_exit()\n\
   -2 : call tst_flush()\n\
   -3 : call tst_brk()\n\
   -4 : call tst_brkloop()\n\
   -5 : call tst_res() with a range\n\
    0 : call tst_res(TPASS, ...)\n\
    1 : call tst_res(TFAIL, ...)\n\
    2 : call tst_res(TBROK, ...)\n\
    4 : call tst_res(TWARN, ...)\n\
    8 : call tst_res(TRETR, ...)\n\
   16 : call tst_res(TINFO, ...)\n\
   32 : call tst_res(TCONF, ...)\n\n");

   while ( 1 ) {
      printf("Enter ttype (-5,-4,-3,-2,-1,0,1,2,4,8,16,32): ");
      (void) scanf("%d%c", &ttype, &chr);


      switch ( ttype ) {
      case -1:
         tst_exit();
         break;

      case -2:
         tst_flush();
         break;

      case -3:
         printf("Enter the current type (1=FAIL, 2=BROK, 8=RETR, 32=CONF): ");
         (void) scanf("%d%c", &ttype, &chr);
         printf("Enter file name (<cr> for none): ");
         gets(fname);
         if ( strcmp(fname, "") == 0 )
            tst_brkm(ttype, tst_exit, RESM, ttype);
         else
            tst_brk(ttype, fname, tst_exit, RES, ttype, fname);
         break;

      case -4:
         printf("Enter the size of the loop: ");
         (void) scanf("%d%c", &range, &chr);
         Tst_lpstart = Tst_count;
         Tst_lptotal = range;
         printf("Enter the current type (1=FAIL, 2=BROK, 8=RETR, 32=CONF): ");
         (void) scanf("%d%c", &ttype, &chr);
         printf("Enter file name (<cr> for none): ");
         gets(fname);
         if ( strcmp(fname, "") == 0 )
            tst_brkloopm(ttype, NULL, RESM, ttype);
         else
            tst_brkloop(ttype, fname, NULL, RES, ttype, fname);
         break;

      case -5:
         printf("Enter the size of the range: ");
         (void) scanf("%d%c", &Tst_range, &chr);
         printf("Enter the current type (0,1,2,4,8,16,32): ");
         (void) scanf("%d%c", &ttype, &chr);
         /* fall through to tst_res() call */

      default:
         printf("Enter file name (<cr> for none): ");
         gets(fname);
         if ( strcmp(fname, "") == 0 )
            tst_resm(ttype, RESM, ttype);
         else
            tst_res(ttype, fname, RES, ttype, fname);
         break;
      }  /* switch() */
   }  /* while() */
}
#endif  /* UNIT_TEST */
