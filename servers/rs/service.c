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
  "refresh", 
  "rescue", 
  "shutdown", 
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
#define ARG_PATH	2		/* rescue dir or system service */
#define ARG_PID		2		/* pid of system service */

#define MIN_ARG_COUNT	2		/* require an action */

#define ARG_ARGS	"-args"		/* list of arguments to be passed */
#define ARG_DEV		"-dev"		/* major device number for drivers */
#define ARG_PRIV	"-priv"		/* required privileges */
#define ARG_PERIOD	"-period"	/* heartbeat period in ticks */

/* The function parse_arguments() verifies and parses the command line 
 * parameters passed to this utility. Request parameters that are needed
 * are stored globally in the following variables:
 */
PRIVATE int req_type;
PRIVATE int req_pid;
PRIVATE char *req_path;
PRIVATE char *req_args;
PRIVATE int req_major;
PRIVATE long req_period;
PRIVATE char *req_priv;

/* Buffer to build "/command arg1 arg2 ..." string to pass to RS server. */
PRIVATE char command[4096];	

/* An error occurred. Report the problem, print the usage, and exit. 
 */
PRIVATE void print_usage(char *app_name, char *problem) 
{
  printf("Warning, %s\n", problem);
  printf("Usage:\n");
  printf("    %s up <binary> [%s <args>] [%s <special>] [%s <ticks>]\n", 
	app_name, ARG_ARGS, ARG_DEV, ARG_PERIOD);
  printf("    %s down <pid>\n", app_name);
  printf("    %s refresh <pid>\n", app_name);
  printf("    %s rescue <dir>\n", app_name);
  printf("    %s shutdown\n", app_name);
  printf("\n");
}

/* A request to the RS server failed. Report and exit. 
 */
PRIVATE void failure(int num) 
{
  printf("Request to RS failed: %s (%d)\n", strerror(num), num);
  exit(num);
}


/* Parse and verify correctness of arguments. Report problem and exit if an 
 * error is found. Store needed parameters in global variables.
 */
PRIVATE int parse_arguments(int argc, char **argv)
{
  struct stat stat_buf;
  char *hz;
  int req_nr;
  int i;

  /* Verify argument count. */ 
  if (argc < MIN_ARG_COUNT) {
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
  req_nr = RS_RQ_BASE + req_type;

  if (req_nr == RS_UP) {

      /* Verify argument count. */ 
      if (argc - 1 < ARG_PATH) {
          print_usage(argv[ARG_NAME], "action requires a binary to start");
          exit(EINVAL);
      }

      /* Verify the name of the binary of the system service. */
      req_path = argv[ARG_PATH];
      if (req_path[0] != '/') {
          print_usage(argv[ARG_NAME], "binary should be absolute path");
          exit(EINVAL);
      }
      if (stat(req_path, &stat_buf) == -1) {
	  perror(req_path);
          fprintf(stderr, "couldn't get stat binary\n");
          exit(errno);
      }
      if (! (stat_buf.st_mode & S_IFREG)) {
          print_usage(argv[ARG_NAME], "binary is not a regular file");
          exit(EINVAL);
      }

      /* Check optional arguments that come in pairs like "-args arglist". */
      for (i=MIN_ARG_COUNT+1; i<argc; i=i+2) {
          if (! (i+1 < argc)) {
              print_usage(argv[ARG_NAME], "optional argument not complete");
              exit(EINVAL);
          }
          if (strcmp(argv[i], ARG_ARGS)==0) {
              req_args = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_PERIOD)==0) {
	      req_period = strtol(argv[i+1], &hz, 10);
	      if (strcmp(hz,"HZ")==0) req_period *= HZ;
	      if (req_period < 1) {
                  print_usage(argv[ARG_NAME], "period is at least be one tick");
                  exit(EINVAL);
	      }
          }
          else if (strcmp(argv[i], ARG_DEV)==0) {
              if (stat(argv[i+1], &stat_buf) == -1) {
                  print_usage(argv[ARG_NAME], "couldn't get status of device");
                  exit(errno);
              }
	      if ( ! (stat_buf.st_mode & (S_IFBLK | S_IFCHR))) {
                  print_usage(argv[ARG_NAME], "special file is not a device");
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
  }
  else if (req_nr == RS_DOWN || req_nr == RS_REFRESH) {

      /* Verify argument count. */ 
      if (argc - 1 < ARG_PID) {
          print_usage(argv[ARG_NAME], "action requires a pid to stop");
          exit(EINVAL);
      }
      if (! (req_pid = atoi(argv[ARG_PID])) > 0) {
          print_usage(argv[ARG_NAME], "pid must be greater than zero");
          exit(EINVAL);
      }
  } 
  else if (req_nr == RS_RESCUE) {

      /* Verify argument count. */ 
      if (argc - 1 < ARG_PATH) {
          print_usage(argv[ARG_NAME], "action requires rescue directory");
          exit(EINVAL);
      }
      req_path = argv[ARG_PATH];
      if (req_path[0] != '/') {
          print_usage(argv[ARG_NAME], "rescue dir should be absolute path");
          exit(EINVAL);
      }
      if (stat(argv[ARG_PATH], &stat_buf) == -1) {
          print_usage(argv[ARG_NAME], "couldn't get status of directory");
          exit(errno);
      }
      if ( ! (stat_buf.st_mode & S_IFDIR)) {
          print_usage(argv[ARG_NAME], "file is not a directory");
          exit(EINVAL);
      } 
  } 
  else if (req_nr == RS_SHUTDOWN) {
        /* no extra arguments required */
  }

  /* Return the request number if no error were found. */
  return(req_nr);
}


/* Main program. 
 */
PUBLIC int main(int argc, char **argv)
{
  message m;
  int result;
  int request;
  int s;

  /* Verify and parse the command line arguments. All arguments are checked
   * here. If an error occurs, the problem is reported and exit(2) is called. 
   * all needed parameters to perform the request are extracted and stored
   * global variables. 
   */
  request = parse_arguments(argc, argv);

  /* Arguments seem fine. Try to perform the request. Only valid requests 
   * should end up here. The default is used for not yet supported requests. 
   */
  switch(request) {
  case RS_UP:
      /* Build space-separated command string to be passed to RS server. */
      strcpy(command, req_path);
      command[strlen(req_path)] = ' ';
      strcpy(command+strlen(req_path)+1, req_args);

      /* Build request message and send the request. */
      m.RS_CMD_ADDR = command;
      m.RS_CMD_LEN = strlen(command);
      m.RS_DEV_MAJOR = req_major;
      m.RS_PERIOD = req_period;
      if (OK != (s=_taskcall(RS_PROC_NR, request, &m))) 
          failure(s);
      result = m.m_type;
      break;
  case RS_DOWN:
  case RS_REFRESH:
      m.RS_PID = req_pid;
      if (OK != (s=_taskcall(RS_PROC_NR, request, &m))) 
          failure(s);
      break;
  case RS_RESCUE:
      m.RS_CMD_ADDR = req_path;
      m.RS_CMD_LEN = strlen(req_path);
      if (OK != (s=_taskcall(RS_PROC_NR, request, &m))) 
          failure(s);
      break;
  case RS_SHUTDOWN:
      if (OK != (s=_taskcall(RS_PROC_NR, request, &m))) 
          failure(s);
      break;
  default:
      print_usage(argv[ARG_NAME], "request is not yet supported");
      result = EGENERIC;
  }
  return(result);
}

