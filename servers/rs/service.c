/* Utility to start or stop system services.  Requests are sent to the 
 * reincarnation server that does the actual work. 
 *
 * Changes:
 *   Jul 22, 2005:	Created  (Jorrit N. Herder)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <minix/config.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <sys/types.h>
#include <sys/stat.h>


/* This array defines all known requests. */
PRIVATE char *known_requests[] = {
  "up", 
  "down",
  "catch for illegal requests"
};
#define ILLEGAL_REQUEST  sizeof(known_requests)/sizeof(char *)

/* Global error number set for failed system calls. */
#define OK 0
extern int errno;

/* Define names for arguments provided to this utility. The first few 
 * arguments are required and have a known index. Thereafter, some optional
 * argument pairs like "-args arglist" follow.
 */
#define ARG_NAME	0		/* own application name */
#define ARG_REQUEST	1		/* request to perform */
#define ARG_PATH	2		/* binary of system service */

#define MIN_ARG_COUNT	3		/* minimum number of arguments */

#define ARG_ARGS	"-args"		/* list of arguments to be passed */
#define ARG_DEV		"-dev"		/* major device number for drivers */
#define ARG_PRIV	"-priv"		/* required privileges */

/* The function parse_arguments() verifies and parses the command line 
 * parameters passed to this utility. Request parameters that are needed
 * are stored globally in the following variables:
 */
PRIVATE int req_type;
PRIVATE char *req_path;
PRIVATE char *req_args;
PRIVATE int req_major;
PRIVATE char *req_priv;

/* An error occurred. Report the problem, print the usage, and exit. 
 */
PRIVATE void print_usage(char *app_name, char *problem) 
{
  printf("Warning, %s\n", problem);
  printf("Usage:\n");
  printf("    %s <request> <binary> [%s <args>] [%s <special>]\n", 
	app_name, ARG_ARGS, ARG_DEV);
  printf("\n");
}

/* An unexpected, unrecoverable error occurred. Report and exit. 
 */
PRIVATE void panic(char *app_name, char *mess, int num) 
{
  printf("Panic in %s: %s", app_name, mess);
  if (num != NO_NUM) printf(": %d", num);
  printf("\n");
  exit(EGENERIC);
}


/* Parse and verify correctness of arguments. Report problem and exit if an 
 * error is found. Store needed parameters in global variables.
 */
PRIVATE int parse_arguments(int argc, char **argv)
{
  struct stat stat_buf;
  int i;

  /* Verify argument count. */ 
  if (! argc >= MIN_ARG_COUNT) {
      print_usage(argv[ARG_NAME], "wrong number of arguments");
      exit(EINVAL);
  }

  /* Verify request type. */
  for (req_type=0; req_type< ILLEGAL_REQUEST; req_type++) {
      if (strcmp(known_requests[req_type],argv[ARG_REQUEST])==0) break;
  }
  if (req_type == ILLEGAL_REQUEST) {
      print_usage(argv[ARG_NAME], "illegal request type");
      exit(ENOSYS);
  }

  /* Verify the name of the binary of the system service. */
  req_path = argv[ARG_PATH];
  if (req_path[0] != '/') {
      print_usage(argv[ARG_NAME], "binary should be absolute path");
      exit(EINVAL);
  }
  if (stat(req_path, &stat_buf) == -1) {
      print_usage(argv[ARG_NAME], "couldn't get status of binary");
      exit(errno);
  }
  if (! (stat_buf.st_mode & S_IFREG)) {
      print_usage(argv[ARG_NAME], "binary is not a regular file");
      exit(EINVAL);
  }

  /* Check optional arguments that come in pairs like "-args arglist". */
  for (i=MIN_ARG_COUNT; i<argc; i=i+2) {
      if (! (i+1 < argc)) {
          print_usage(argv[ARG_NAME], "optional argument not complete");
          exit(EINVAL);
      }
      if (strcmp(argv[i], ARG_ARGS)==0) {
          req_args = argv[i+1];
      }
      else if (strcmp(argv[i], ARG_DEV)==0) {
          if (stat(argv[i+1], &stat_buf) == -1) {
              print_usage(argv[ARG_NAME], "couldn't get status of device node");
              exit(errno);
          }
	  if ( ! (stat_buf.st_mode & (S_IFBLK | S_IFCHR))) {
              print_usage(argv[ARG_NAME], "special file is not a device node");
              exit(EINVAL);
	  } 
          req_major = (stat_buf.st_rdev >> MAJOR) & BYTE;
      }
      else if (strcmp(argv[i], ARG_ARGS)==0) {
          req_priv = argv[i+1];
      }
      else {
          print_usage(argv[ARG_NAME], "unknown optional argument given");
          exit(EINVAL);
      }
  }

  /* Return the request number if no error were found. */
  return(i);
}


/* Main program. 
 */
PUBLIC int main(int argc, char **argv)
{
  message m;
  int result;
  int s;

  /* Verify and parse the command line arguments. All arguments are checked
   * here. If an error occurs, the problem is reported and exit(2) is called. 
   * all needed parameters to perform the request are extracted and stored
   * global variables. 
   */
  parse_arguments(argc, argv);

  /* Arguments seem fine. Try to perform the request. Only valid requests 
   * should end up here. The default is used for not yet supported requests. 
   */
  switch(req_type+SRV_RQ_BASE) {
  case SRV_UP:
      m.SRV_PATH_ADDR = req_path;
      m.SRV_PATH_LEN = strlen(req_path);
      m.SRV_ARGS_ADDR = req_args;
      m.SRV_ARGS_LEN = strlen(req_args);
      m.SRV_DEV_MAJOR = req_major;
      if (OK != (s=_taskcall(RS_PROC_NR, SRV_UP, &m))) 
          panic(argv[ARG_NAME], "sendrec to manager server failed", s);
      result = m.m_type;
      break;
  case SRV_DOWN:
  case SRV_STATUS:
  default:
      print_usage(argv[ARG_NAME], "request is not yet supported");
      result = EGENERIC;
  }
  return(result);
}

