/* Utility to start or stop system services.  Requests are sent to the 
 * reincarnation server that does the actual work. 
 *
 * Changes:
 *   Nov 22, 2009: added basic live update support  (Cristiano Giuffrida)
 *   Jul 22, 2005: Created  (Jorrit N. Herder)
 */

#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <limits.h>
#include <lib.h>
#include <minix/config.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/rs.h>
#include <minix/syslib.h>
#include <minix/bitmap.h>
#include <paths.h>
#include <minix/sef.h>
#include <minix/dmap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <configfile.h>

#include <machine/archtypes.h>
#include <timers.h>
#include <err.h>
#include "kernel/proc.h"

#include "config.h"
#include "proto.h"

/* This array defines all known requests. */
static char *known_requests[] = {
  "up", 
  "down",
  "refresh", 
  "restart",
  "shutdown", 
  "update",
  "clone",
  "edit",
  "catch for illegal requests"
};
#define ILLEGAL_REQUEST  sizeof(known_requests)/sizeof(char *)

/* Global error number set for failed system calls. */
#define OK 0

#define RUN_CMD		"run"
#define RUN_SCRIPT	"/etc/rs.single"	/* Default script for 'run' */
#define SELF_BINARY     "self"
#define SELF_REQ_PATH   "/dev/null"
#define PATH_CONFIG	_PATH_SYSTEM_CONF	/* Default config file */
#define DEFAULT_LU_STATE   SEF_LU_STATE_WORK_FREE /* Default lu state */
#define DEFAULT_LU_MAXTIME 0                    /* Default lu max time */

/* Define names for options provided to this utility. */
#define OPT_COPY	"-c"		/* copy executable image */
#define OPT_REUSE	"-r"		/* reuse executable image */
#define OPT_NOBLOCK	"-n"		/* unblock caller immediately */
#define OPT_REPLICA	"-p"		/* create replica for the service */

/* Define names for arguments provided to this utility. The first few 
 * arguments are required and have a known index. Thereafter, some optional
 * argument pairs like "-args arglist" follow.
 */
#define ARG_NAME	0		/* own application name */

/* The following are relative to optind */
#define ARG_REQUEST	0		/* request to perform */
#define ARG_PATH	1		/* system service */
#define ARG_LABEL	1		/* name of system service */

#define MIN_ARG_COUNT	1		/* require an action */

#define ARG_ARGS	"-args"		/* list of arguments to be passed */
#define ARG_DEV		"-dev"		/* major device number for drivers */
#define ARG_MAJOR	"-major"	/* major number */
#define ARG_DEVSTYLE	"-devstyle"	/* device style */
#define ARG_PERIOD	"-period"	/* heartbeat period in ticks */
#define ARG_SCRIPT	"-script"	/* name of the script to restart a
					 * system service
					 */
#define ARG_LABELNAME	"-label"	/* custom label name */
#define ARG_CONFIG	"-config"	/* name of the file with the resource
					 * configuration 
					 */

#define ARG_LU_STATE	"-state"	/* the live update state required */
#define ARG_LU_MAXTIME	"-maxtime"      /* max time to prepare for the update */
#define ARG_DEVMANID	"-devid"    /* the id of the devman device this
                                       driver should be able to access  */

/* The function parse_arguments() verifies and parses the command line 
 * parameters passed to this utility. Request parameters that are needed
 * are stored globally in the following variables:
 */
static int req_type;
static int do_run= 0;		/* 'run' command instead of 'up' */
static char *req_label = NULL;
static char *req_path = NULL;
static char *req_path_self = SELF_REQ_PATH;
static char *req_args = "";
static int req_major = 0;
static int devman_id = 0;
static int req_dev_style = STYLE_NDEV;
static long req_period = 0;
static char *req_script = NULL;
static char *req_config = PATH_CONFIG;
static int custom_config_file = 0;
static int req_lu_state = DEFAULT_LU_STATE;
static int req_lu_maxtime = DEFAULT_LU_MAXTIME;

/* Buffer to build "/command arg1 arg2 ..." string to pass to RS server. */
static char command[4096];	

/* An error occurred. Report the problem, print the usage, and exit. 
 */
static void print_usage(char *app_name, char *problem) 
{
  fprintf(stderr, "Warning, %s\n", problem);
  fprintf(stderr, "Usage:\n");
  fprintf(stderr,
  "    %s [%s %s %s %s] (up|run|edit|update) <binary|%s> [%s <args>] [%s <special>] [%s <style>] [%s <major_nr>] [%s <dev_id>] [%s <ticks>] [%s <path>] [%s <name>] [%s <path>] [%s <state>] [%s <time>]\n", 
	app_name, OPT_COPY, OPT_REUSE, OPT_NOBLOCK, OPT_REPLICA, SELF_BINARY,
	ARG_ARGS, ARG_DEV, ARG_DEVSTYLE, ARG_MAJOR, ARG_DEVMANID, ARG_PERIOD, ARG_SCRIPT,
	ARG_LABELNAME, ARG_CONFIG, ARG_LU_STATE, ARG_LU_MAXTIME);
  fprintf(stderr, "    %s down <label>\n", app_name);
  fprintf(stderr, "    %s refresh <label>\n", app_name);
  fprintf(stderr, "    %s restart <label>\n", app_name);
  fprintf(stderr, "    %s clone <label>\n", app_name);
  fprintf(stderr, "    %s shutdown\n", app_name);
  fprintf(stderr, "\n");
}

/* A request to the RS server failed. Report and exit. 
 */
static void failure(int request)
{
  fprintf(stderr, "Request 0x%x to RS failed: %s (error %d)\n", request, strerror(errno), errno);
  exit(errno);
}


/* Parse and verify correctness of arguments. Report problem and exit if an 
 * error is found. Store needed parameters in global variables.
 */
static int parse_arguments(int argc, char **argv, u32_t *rss_flags)
{
  struct stat stat_buf;
  char *hz, *buff;
  int req_nr;
  int c, i, j;
  int c_flag, r_flag, n_flag, p_flag;
  int label_required;

  c_flag = 0;
  r_flag = 0;
  n_flag = 0;
  p_flag = 0;
  while (c= getopt(argc, argv, "rcnp?"), c != -1)
  {
	switch(c)
	{
	case '?':
		print_usage(argv[ARG_NAME], "wrong number of arguments");
		exit(EINVAL);
	case 'c':
		c_flag = 1;
		break;
	case 'r':
	        c_flag = 1; /* -r implies -c */
		r_flag = 1;
		break;
	case 'n':
		n_flag = 1;
		break;
	case 'p':
		p_flag = 1;
		break;
	default:
		fprintf(stderr, "%s: getopt failed: %c\n",
			argv[ARG_NAME], c);
		exit(1);
	}
  }

  /* Verify argument count. */ 
  if (argc < optind+MIN_ARG_COUNT) {
      print_usage(argv[ARG_NAME], "wrong number of arguments");
      exit(EINVAL);
  }

  if (strcmp(argv[optind+ARG_REQUEST], RUN_CMD) == 0)
  {
	req_nr= RS_UP;
	do_run= TRUE;
  }
  else
  {
  	/* Verify request type. */
	for (req_type=0; req_type< ILLEGAL_REQUEST; req_type++) {
	    if (strcmp(known_requests[req_type],argv[optind+ARG_REQUEST])==0)
		break;
	}
	if (req_type == ILLEGAL_REQUEST) {
	    print_usage(argv[ARG_NAME], "illegal request type");
	    exit(ENOSYS);
	}
	req_nr = RS_RQ_BASE + req_type;
  }

  *rss_flags = 0;
  if (req_nr == RS_UP || req_nr == RS_UPDATE || req_nr == RS_EDIT) {
      u32_t system_hz;

      if (c_flag)
	*rss_flags |= RSS_COPY;

      if(r_flag)
        *rss_flags |= RSS_REUSE;

      if(n_flag)
        *rss_flags |= RSS_NOBLOCK;

      if(p_flag)
        *rss_flags |= RSS_REPLICA;

      req_path = argv[optind+ARG_PATH];
      if(req_nr == RS_UPDATE && !strcmp(req_path, SELF_BINARY)) {
          /* Self update needs no real path or configuration file. */
          req_config = NULL;
          req_path = req_path_self;
          *rss_flags |= RSS_SELF_LU;
      }

      if (do_run)
      {
	/* Set default recovery script for RUN */
        req_script = RUN_SCRIPT;
      }

      /* Verify argument count. */ 
      if (argc - 1 < optind+ARG_PATH) {
          print_usage(argv[ARG_NAME], "action requires a binary to start");
          exit(EINVAL);
      }

      /* Verify the name of the binary of the system service. */
      if(!(*rss_flags & RSS_SELF_LU)) {
          if (req_path[0] != '/') {
              print_usage(argv[ARG_NAME], "binary should be absolute path");
              exit(EINVAL);
          }
          if (stat(req_path, &stat_buf) == -1) {
	      perror(req_path);
              fprintf(stderr, "%s: couldn't get stat binary\n", argv[ARG_NAME]);
              exit(errno);
          }
          if (! (stat_buf.st_mode & S_IFREG)) {
              print_usage(argv[ARG_NAME], "binary is not a regular file");
              exit(EINVAL);
          }
      }

      /* Get HZ. */
      system_hz = (u32_t) sysconf(_SC_CLK_TCK);

      /* Check optional arguments that come in pairs like "-args arglist". */
      for (i=optind+MIN_ARG_COUNT+1; i<argc; i=i+2) {
          if (! (i+1 < argc)) {
              print_usage(argv[ARG_NAME], "optional argument not complete");
              exit(EINVAL);
          }
          if (strcmp(argv[i], ARG_ARGS)==0) {
              req_args = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_PERIOD)==0) {
	      req_period = strtol(argv[i+1], &hz, 10);
	      if (strcmp(hz,"HZ")==0) req_period *= system_hz;
	      if (req_period < 0) {
                  print_usage(argv[ARG_NAME], "bad period argument");
                  exit(EINVAL);
	      }
          }
          else if (strcmp(argv[i], ARG_DEV)==0) {
              if (stat(argv[i+1], &stat_buf) == -1) {
		  perror(argv[i+1]);
                  print_usage(argv[ARG_NAME], "couldn't get status of device");
                  exit(errno);
              }
	      if ( ! (stat_buf.st_mode & (S_IFBLK | S_IFCHR))) {
                  print_usage(argv[ARG_NAME], "special file is not a device");
                  exit(EINVAL);
       	      } 
              if (req_major != 0) {
				  print_usage(argv[ARG_NAME], "major already set");
				  exit(EINVAL);
			  }
              req_major = major(stat_buf.st_rdev);
              if(req_dev_style == STYLE_NDEV) {
                  req_dev_style = STYLE_DEV;
              }
          }
		  else if (strcmp(argv[i], ARG_MAJOR)==0) {
			  if (req_major != 0) {
				  print_usage(argv[ARG_NAME], "major already set");
				  exit(EINVAL);
			  }
			 if (i+1 < argc) { 
				 req_major = atoi(argv[i+1]);
			 } else {
				 exit(EINVAL);
			 }
			 if(req_dev_style == STYLE_NDEV) {
                  req_dev_style = STYLE_DEV;
              }
		  }
          else if (strcmp(argv[i], ARG_DEVSTYLE)==0) {
              char* dev_style_keys[] = { "STYLE_DEV", "STYLE_DEVA", "STYLE_TTY",
                  "STYLE_CTTY", "STYLE_CLONE", "STYLE_CLONE_A", NULL };
              int dev_style_values[] = { STYLE_DEV, STYLE_DEVA, STYLE_TTY,
                  STYLE_CTTY, STYLE_CLONE, STYLE_CLONE_A };
              for(j=0;dev_style_keys[j]!=NULL;j++) {
                  if(!strcmp(dev_style_keys[j], argv[i+1])) {
                      break;
                  }
              }
              if(dev_style_keys[j] == NULL) {
                  print_usage(argv[ARG_NAME], "bad device style");
                  exit(EINVAL);
              }
              req_dev_style = dev_style_values[j];
          }
          else if (strcmp(argv[i], ARG_SCRIPT)==0) {
              req_script = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_LABELNAME)==0) {
              req_label = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_CONFIG)==0) {
              req_config = argv[i+1];
 	      custom_config_file = 1;
          }
          else if (strcmp(argv[i], ARG_LU_STATE)==0) {
              errno=0;
              req_lu_state = strtol(argv[i+1], &buff, 10);
              if(errno || strcmp(buff, "")) {
                  print_usage(argv[ARG_NAME], "bad live update state");
                  exit(EINVAL);
              }
              if(req_lu_state == SEF_LU_STATE_NULL) {
                  print_usage(argv[ARG_NAME], "null live update state");
                  exit(EINVAL);
              }
          }
          else if (strcmp(argv[i], ARG_LU_MAXTIME)==0) {
              errno=0;
              req_lu_maxtime = strtol(argv[i+1], &hz, 10);
              if(errno || (strcmp(hz, "") && strcmp(hz, "HZ"))
                  || req_lu_maxtime<0) {
                  print_usage(argv[ARG_NAME],
                      "bad live update max time");
                  exit(EINVAL);
              }
              if (strcmp(hz,"HZ")==0) req_lu_maxtime *= system_hz;
          } else if (strcmp(argv[i], ARG_DEVMANID) == 0) {
			 if (i+1 < argc) { 
				 devman_id = atoi(argv[i+1]);
			 } else {
				 exit(EINVAL);
			 }
		  } else {
              print_usage(argv[ARG_NAME], "unknown optional argument given");
              exit(EINVAL);
          }
      }
  }
  else if (req_nr == RS_DOWN || req_nr == RS_REFRESH || req_nr == RS_RESTART
      || req_nr == RS_CLONE) {

      /* Verify argument count. */ 
      if (argc - 1 < optind+ARG_LABEL) {
          print_usage(argv[ARG_NAME], "action requires a target label");
          exit(EINVAL);
      }
      req_label= argv[optind+ARG_LABEL];
  } 
  else if (req_nr == RS_SHUTDOWN) {
        /* no extra arguments required */
  }

  label_required = (*rss_flags & RSS_SELF_LU) || (req_nr == RS_EDIT);
  if(label_required && !req_label) {
      print_usage(argv[ARG_NAME], "label option mandatory for target action");
      exit(EINVAL);
  }

  /* Return the request number if no error were found. */
  return(req_nr);
}

/* Main program. 
 */
int main(int argc, char **argv)
{
  message m;
  int result = EXIT_SUCCESS;
  int request;
  char *progname = NULL;
  /* Arguments for RS to start a new service */
  struct rs_config config;
  u32_t rss_flags = 0;

  /* Verify and parse the command line arguments. All arguments are checked
   * here. If an error occurs, the problem is reported and exit(2) is called. 
   * all needed parameters to perform the request are extracted and stored
   * global variables. 
   */
  request = parse_arguments(argc, argv, &rss_flags);

  /* Arguments seem fine. Try to perform the request. Only valid requests 
   * should end up here. The default is used for not yet supported requests. 
   */
  result = OK;
  switch(request) {
  case RS_UPDATE:
      m.RS_LU_STATE = req_lu_state;
      m.RS_LU_PREPARE_MAXTIME = req_lu_maxtime;
      /* fall through */
  case RS_UP:
  case RS_EDIT:
      /* Build space-separated command string to be passed to RS server. */
      progname = strrchr(req_path, '/');
      assert(progname);	/* an absolute path was required */
      progname++;	/* skip last slash */
      strcpy(command, req_path);
      command[strlen(req_path)] = ' ';
      strcpy(command+strlen(req_path)+1, req_args);

      if (req_config) {
	assert(progname);
	memset(&config, 0, sizeof(config));
      	if(!parse_config(progname, custom_config_file, req_config, &config))
		errx(1, "couldn't parse config");
      }

      /* Set specifics */
      config.rs_start.rss_cmd= command;
      config.rs_start.rss_cmdlen= strlen(command);
      config.rs_start.rss_major= req_major;
      config.rs_start.rss_dev_style= req_dev_style;
      config.rs_start.rss_period= req_period;
      config.rs_start.rss_script= req_script;
      config.rs_start.devman_id= devman_id;
      config.rs_start.rss_flags |= rss_flags;
      if(req_label) {
        config.rs_start.rss_label.l_addr = req_label;
        config.rs_start.rss_label.l_len = strlen(req_label);
      } else {
        config.rs_start.rss_label.l_addr = progname;
        config.rs_start.rss_label.l_len = strlen(progname);
      }
      if (req_script)
	      config.rs_start.rss_scriptlen= strlen(req_script);
      else
	      config.rs_start.rss_scriptlen= 0;

      assert(config.rs_start.rss_priority < NR_SCHED_QUEUES);
      assert(config.rs_start.rss_quantum > 0);

      m.RS_CMD_ADDR = (char *) &config.rs_start;
      break;
  case RS_DOWN:
  case RS_REFRESH:
  case RS_RESTART:
  case RS_CLONE:
      m.RS_CMD_ADDR = req_label;
      m.RS_CMD_LEN = strlen(req_label);
      break;
  case RS_SHUTDOWN:
      break;
  default:
      print_usage(argv[ARG_NAME], "request is not yet supported");
      result = EGENERIC;
  }

  /* Build request message and send the request. */
  if(result == OK) {
    if (_syscall(RS_PROC_NR, request, &m) == -1)
        failure(request);
    result = m.m_type;
  }

  return(result);
}

