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
#include <minix/sysinfo.h>
#include <minix/bitmap.h>
#include <minix/paths.h>
#include <minix/sef.h>
#include <minix/dmap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <configfile.h>

#include <machine/archtypes.h>
#include <minix/timers.h>
#include <err.h>

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
  "unclone",
  "edit",
  "sysctl",
  "fi",
  "catch for illegal requests"
};
static int known_request_types[] = {
  RS_UP,
  RS_DOWN,
  RS_REFRESH, 
  RS_RESTART,
  RS_SHUTDOWN, 
  RS_UPDATE,
  RS_CLONE,
  RS_UNCLONE,
  RS_EDIT,
  RS_SYSCTL,
  RS_FI,
  0
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
#define OPT_NO_BIN_EXP	"-b"		/* no binary exponential backoff */
#define OPT_BATCH	"-q"		/* batch mode */
#define OPT_ASR_LU	"-a"		/* asr update */
#define OPT_PREPARE_ONLY_LU    "-o"	/* prepare-only update */
#define OPT_FORCE_SELF_LU      "-s"	/* force self update */
#define OPT_FORCE_INIT_CRASH   "-x"	/* force init crash (for debugging) */
#define OPT_FORCE_INIT_FAIL    "-y"	/* force init failure (for debugging) */
#define OPT_FORCE_INIT_TIMEOUT "-z"	/* force init timeout (for debugging) */
#define OPT_FORCE_INIT_DEFCB   "-d"	/* force init default callback */
#define OPT_NOMMAP_LU          "-m"     /* don't inherit mmaped regions */
#define OPT_DETACH             "-e"     /* detach on update/restart */
#define OPT_NORESTART          "-f"     /* don't restart */
#define OPT_FORCE_INIT_ST      "-t"	/* force init state transfer */

/* Define names for arguments provided to this utility. The first few 
 * arguments are required and have a known index. Thereafter, some optional
 * argument pairs like "-args arglist" follow.
 */
#define ARG_NAME	0		/* own application name */

/* The following are relative to optind */
#define ARG_REQUEST	0		/* request to perform */
#define ARG_PATH	1		/* system service */
#define ARG_LABEL	1		/* name of system service */
#define ARG_SYSCTL_TYPE	1		/* sysctl action type */


#define MIN_ARG_COUNT	1		/* require an action */

#define ARG_ARGS	"-args"		/* list of arguments to be passed */
#define ARG_DEV		"-dev"		/* major device number for drivers */
#define ARG_MAJOR	"-major"	/* major number */
#define ARG_PERIOD	"-period"	/* heartbeat period in ticks */
#define ARG_SCRIPT	"-script"	/* name of the script to restart a
					 * system service
					 */
#define ARG_LABELNAME	"-label"	/* custom label name */
#define ARG_PROGNAME	"-progname"	/* custom program name */
#define ARG_CONFIG	"-config"	/* name of the file with the resource
					 * configuration 
					 */

#define ARG_LU_STATE	"-state"	/* the live update state required */
#define ARG_LU_MAXTIME	"-maxtime"      /* max time to prepare for the update */
#define ARG_DEVMANID	"-devid"    /* the id of the devman device this
                                       driver should be able to access  */
#define ARG_HEAP_PREALLOC "-heap-prealloc" /* preallocate heap regions */
#define ARG_MAP_PREALLOC  "-map-prealloc"  /* preallocate mmapped regions */
#define ARG_TRG_LABELNAME "-trg-label"	/* target label name */
#define ARG_LU_IPC_BL	"-ipc_bl"       /* IPC blacklist filter */
#define ARG_LU_IPC_WL	"-ipc_wl"       /* IPC whitelist filter */
#define ARG_ASR_COUNT	"-asr-count"    /* number of ASR live updates */
#define ARG_RESTARTS	"-restarts"    /* number of restarts */

/* The function parse_arguments() verifies and parses the command line 
 * parameters passed to this utility. Request parameters that are needed
 * are stored globally in the following variables:
 */
static int req_type;
static int do_run= 0;		/* 'run' command instead of 'up' */
static char *req_label = NULL;
static char *req_trg_label = NULL;
static char *req_path = NULL;
static char *req_path_self = SELF_REQ_PATH;
static char *req_progname = NULL;
static char *req_args = "";
static int req_major = 0;
static int devman_id = 0;
static long req_period = 0;
static char *req_script = NULL;
static char *req_config = PATH_CONFIG;
static int custom_config_file = 0;
static int req_lu_state = DEFAULT_LU_STATE;
static int req_lu_maxtime = DEFAULT_LU_MAXTIME;
static int req_restarts = 0;
static int req_asr_count = -1;
static long req_heap_prealloc = 0;
static long req_map_prealloc = 0;
static int req_sysctl_type = 0;
static struct rs_ipc_filter_el rs_ipc_filter_els[RS_MAX_IPC_FILTERS][IPCF_MAX_ELEMENTS];
static int num_ipc_filters = 0;
static char *req_state_eval = NULL;

/* Buffer to build "/command arg1 arg2 ..." string to pass to RS server. */
static char command[4096];	

/* An error occurred. Report the problem, print the usage, and exit. 
 */
static void print_usage(char *app_name, char *problem) 
{
  fprintf(stderr, "Warning, %s\n", problem);
  fprintf(stderr, "Usage:\n");
  fprintf(stderr,
      "    %s [%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s] (up|run|edit|update) <binary|%s> [%s <args>] [%s <special>] [%s <major_nr>] [%s <dev_id>] [%s <ticks>] [%s <path>] [%s <name>] [%s <name>] [%s <path>] [%s <state value|eval_expression>] [%s <time>] [%s <bytes>] [%s <bytes>] [%s <name>] [(%s|%s <src_label1,src_type1:src_label2,:,src_type3:...>)*] [%s <count>] [%s <restarts>]\n",
	app_name, OPT_COPY, OPT_REUSE, OPT_NOBLOCK, OPT_REPLICA, OPT_NO_BIN_EXP,
	OPT_BATCH, OPT_ASR_LU, OPT_PREPARE_ONLY_LU, OPT_FORCE_SELF_LU,
	OPT_FORCE_INIT_CRASH, OPT_FORCE_INIT_FAIL, OPT_FORCE_INIT_TIMEOUT,
	OPT_FORCE_INIT_DEFCB, OPT_NOMMAP_LU, OPT_DETACH,
	OPT_NORESTART, OPT_FORCE_INIT_ST, SELF_BINARY,
	ARG_ARGS, ARG_DEV, ARG_MAJOR, ARG_DEVMANID, ARG_PERIOD,
	ARG_SCRIPT, ARG_LABELNAME, ARG_PROGNAME, ARG_CONFIG, ARG_LU_STATE, ARG_LU_MAXTIME,
	ARG_HEAP_PREALLOC, ARG_MAP_PREALLOC, ARG_TRG_LABELNAME, ARG_LU_IPC_BL, ARG_LU_IPC_WL,
	ARG_ASR_COUNT, ARG_RESTARTS);
  fprintf(stderr, "    %s down <label>\n", app_name);
  fprintf(stderr, "    %s refresh <label>\n", app_name);
  fprintf(stderr, "    %s restart <label>\n", app_name);
  fprintf(stderr, "    %s clone <label>\n", app_name);
  fprintf(stderr, "    %s unclone <label>\n", app_name);
  fprintf(stderr, "    %s fi <label>\n", app_name);
  fprintf(stderr, "    %s sysctl <srv_status|upd_start|upd_run|upd_stop|upd_status>\n", app_name);
  fprintf(stderr, "    %s shutdown\n", app_name);
  fprintf(stderr, "    Options:\n");
  fprintf(stderr, "      %s: copy executable image             \n", OPT_COPY);
  fprintf(stderr, "      %s: reuse executable image            \n", OPT_REUSE);
  fprintf(stderr, "      %s: unblock caller immediately        \n", OPT_NOBLOCK);
  fprintf(stderr, "      %s: create replica for the service    \n", OPT_REPLICA);
  fprintf(stderr, "      %s: batch mode                        \n", OPT_BATCH);
  fprintf(stderr, "      %s: asr update                        \n", OPT_ASR_LU);
  fprintf(stderr, "      %s: prepare-only update               \n", OPT_PREPARE_ONLY_LU);
  fprintf(stderr, "      %s: force self update                 \n", OPT_FORCE_SELF_LU);
  fprintf(stderr, "      %s: force init crash (for debugging)  \n", OPT_FORCE_INIT_CRASH);
  fprintf(stderr, "      %s: force init failure (for debugging)\n", OPT_FORCE_INIT_FAIL);
  fprintf(stderr, "      %s: force init timeout (for debugging)\n", OPT_FORCE_INIT_TIMEOUT);
  fprintf(stderr, "      %s: force init default callback       \n", OPT_FORCE_INIT_DEFCB);
  fprintf(stderr, "      %s: don't inherit mmaped regions      \n", OPT_NOMMAP_LU);
  fprintf(stderr, "      %s: detach on update/restart          \n", OPT_DETACH);
  fprintf(stderr, "      %s: don't restart                     \n", OPT_NORESTART);
  fprintf(stderr, "      %s: force init state transfer         \n", OPT_FORCE_INIT_ST);

  fprintf(stderr, "\n");
}

/* A request to the RS server failed. Report and exit. 
 */
static void failure(int request)
{
  fprintf(stderr, "Request 0x%x to RS failed: %s (error %d)\n", request, strerror(errno), errno);
  exit(errno);
}

static int parse_ipc_filter(char* str, int type_flag)
{
  char *el_str, *label_str, *type_str;
  char el_strings[IPCF_MAX_ELEMENTS][RS_MAX_IPCF_STR_LEN];
  int i, num_els, label_len, type_len;
  struct rs_ipc_filter_el *ipcf_el;
  if(num_ipc_filters+1 > RS_MAX_IPC_FILTERS) {
	return E2BIG;
  }
  el_str = (char*) strsep(&str,":");
  num_els = 0;
  while (el_str != NULL)
  {
	if(num_els >= IPCF_MAX_ELEMENTS) {
		return E2BIG;
	}
	if(strlen(el_str)>=RS_MAX_IPCF_STR_LEN) {
		return ENOMEM;
	}
	strcpy(el_strings[num_els], el_str);
	el_str = (char*) strsep(&str, ":");
	num_els++;
  }
  memset(rs_ipc_filter_els[num_ipc_filters],0,IPCF_MAX_ELEMENTS);
  for(i=0;i<num_els;i++) {
	el_str = el_strings[i];
	ipcf_el = &rs_ipc_filter_els[num_ipc_filters][i];
	label_str = (char*) strsep(&el_str,",");
	type_str = (char*) strsep(&el_str,",");
	if(!label_str || !type_str || strsep(&el_str,",")) {
		return EINVAL;
	}
	label_len = strlen(label_str);
	type_len = strlen(type_str);
	ipcf_el->flags = 0;
	if(label_len>0) {
		if(label_len >= RS_MAX_LABEL_LEN) {
			return ENOMEM;
		}
		strcpy(ipcf_el->m_label, label_str);
		ipcf_el->flags |= IPCF_MATCH_M_SOURCE;
	}
	if(type_len>0) {
	        char *buff;
		errno=0;
		ipcf_el->m_type = strtol(type_str, &buff, 10);
		if(errno || strcmp(buff, "")) {
			return EINVAL;
		}
		ipcf_el->flags |= IPCF_MATCH_M_TYPE;
	}
	if(ipcf_el->flags == 0) {
		return EINVAL;
	}
	ipcf_el->flags |= type_flag;
  }
  num_ipc_filters++;
  return OK;
}

/* Parse and verify correctness of arguments. Report problem and exit if an 
 * error is found. Store needed parameters in global variables.
 */
static int parse_arguments(int argc, char **argv, u32_t *rss_flags)
{
  struct stat stat_buf;
  char *hz, *buff;
  int req_nr;
  int c, i, j, r;
  int b_flag, c_flag, r_flag, n_flag, p_flag, q_flag,
      a_flag, o_flag, s_flag, x_flag, y_flag,
      z_flag, d_flag, u_flag, m_flag, e_flag,
      f_flag, t_flag;
  int label_required;

  b_flag = 0;
  c_flag = 0;
  r_flag = 0;
  n_flag = 0;
  p_flag = 0;
  q_flag = 0;
  a_flag = 0;
  o_flag = 0;
  s_flag = 0;
  x_flag = 0;
  y_flag = 0;
  z_flag = 0;
  d_flag = 0;
  u_flag = 0;
  m_flag = 0;
  e_flag = 0;
  f_flag = 0;
  t_flag = 0;
  while (c= getopt(argc, argv, "rbcnpqaosxyzdumeft?"), c != -1)
  {
	switch(c)
	{
	case '?':
		print_usage(argv[ARG_NAME], "wrong number of arguments");
		exit(EINVAL);
	case 'b':
		b_flag = 1;
		break;
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
	case 'q':
		q_flag = 1;
		break;
	case 'a':
		a_flag = 1;
		break;
	case 'o':
		o_flag = 1;
		break;
	case 's':
		s_flag = 1;
		break;
	case 'x':
		x_flag = 1;
		break;
	case 'y':
		y_flag = 1;
		break;
	case 'z':
		z_flag = 1;
		break;
	case 'd':
		d_flag = 1;
		break;
	case 'u':
		u_flag = 1;
		break;
	case 'm':
		m_flag = 1;
		break;
	case 'e':
		e_flag = 1;
		break;
	case 'f':
		f_flag = 1;
		break;
	case 't':
		t_flag = 1;
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
	req_nr = known_request_types[req_type];
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

      if(b_flag)
	*rss_flags |= RSS_NO_BIN_EXP;

      if(q_flag)
          *rss_flags |= RSS_BATCH;

      if(a_flag)
          *rss_flags |= RSS_ASR_LU;

      if(s_flag)
          *rss_flags |= RSS_FORCE_SELF_LU;

      if(o_flag)
          *rss_flags |= RSS_PREPARE_ONLY_LU;

      if(x_flag)
          *rss_flags |= RSS_FORCE_INIT_CRASH;

      if(y_flag)
          *rss_flags |= RSS_FORCE_INIT_FAIL;

      if(z_flag)
          *rss_flags |= RSS_FORCE_INIT_TIMEOUT;

      if(d_flag)
          *rss_flags |= RSS_FORCE_INIT_DEFCB;

      if(m_flag)
          *rss_flags |= RSS_NOMMAP_LU;

      if(e_flag)
          *rss_flags |= RSS_DETACH;

      if(f_flag)
          *rss_flags |= RSS_NORESTART;

      if(t_flag)
          *rss_flags |= RSS_FORCE_INIT_ST;

      /* Verify argument count. */ 
      if (argc - 1 < optind+ARG_PATH) {
          print_usage(argv[ARG_NAME], "action requires a binary to start");
          exit(EINVAL);
      }

      if(req_nr != RS_UPDATE && q_flag) {
	    print_usage(argv[ARG_NAME], "action does not support batch mode");
	    exit(EINVAL);
      }

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
		  }
          else if (strcmp(argv[i], ARG_SCRIPT)==0) {
              req_script = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_LABELNAME)==0) {
              req_label = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_TRG_LABELNAME)==0) {
              req_trg_label = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_PROGNAME)==0) {
              req_progname = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_CONFIG)==0) {
              req_config = argv[i+1];
 	      custom_config_file = 1;
          }
          else if (strcmp(argv[i], ARG_LU_STATE)==0) {
              errno=0;
              req_lu_state = strtol(argv[i+1], &buff, 10);
              if(errno || strcmp(buff, "")) {
                  /* State is not a number, assume it's an eval expression. */
                  req_lu_state = SEF_LU_STATE_EVAL;
                  req_state_eval = argv[i+1];
              }
              else if(req_lu_state == SEF_LU_STATE_NULL) {
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
              if(req_lu_maxtime == 0) {
                  /* no timeout requested. */
                  req_lu_maxtime = -1;
              }
              else if (strcmp(hz,"HZ")==0) req_lu_maxtime *= system_hz;
          }
          else if (strcmp(argv[i], ARG_DEVMANID) == 0) {
              if (i+1 < argc) {
                  devman_id = atoi(argv[i+1]);
              } else {
                  exit(EINVAL);
              }
          }
          else if (strcmp(argv[i], ARG_HEAP_PREALLOC)==0) {
              errno=0;
              req_heap_prealloc = strtol(argv[i+1], &buff, 10);
              if(errno || strcmp(buff, "") || req_heap_prealloc <= 0) {
                  print_usage(argv[ARG_NAME], "bad heap prealloc bytes");
                  exit(EINVAL);
              }
          }
          else if (strcmp(argv[i], ARG_MAP_PREALLOC)==0) {
              errno=0;
              req_map_prealloc = strtol(argv[i+1], &buff, 10);
              if(errno || strcmp(buff, "") || req_map_prealloc <= 0) {
                  print_usage(argv[ARG_NAME], "bad heap prealloc bytes");
                  exit(EINVAL);
              }
          }
          else if (strcmp(argv[i], ARG_LU_IPC_BL)==0) {
              if((r=parse_ipc_filter(argv[i+1], IPCF_EL_BLACKLIST)) != OK) {
                  print_usage(argv[ARG_NAME], "bad IPC blacklist filter");
                  exit(r);
              }
          }
          else if (strcmp(argv[i], ARG_LU_IPC_WL)==0) {
              if((r=parse_ipc_filter(argv[i+1], IPCF_EL_WHITELIST)) != OK) {
                  print_usage(argv[ARG_NAME], "bad IPC whitelist filter");
                  exit(r);
              }
          }
          else if (strcmp(argv[i], ARG_ASR_COUNT)==0) {
              errno=0;
              req_asr_count = strtol(argv[i+1], &buff, 10);
              if(errno || strcmp(buff, "") || req_asr_count<0) {
                  print_usage(argv[ARG_NAME], "bad ASR count");
                  exit(EINVAL);
              }
          }
          else if (strcmp(argv[i], ARG_RESTARTS)==0) {
              errno=0;
              req_restarts = strtol(argv[i+1], &buff, 10);
              if(errno || strcmp(buff, "") || req_restarts<0) {
                  print_usage(argv[ARG_NAME], "bad number of restarts");
                  exit(EINVAL);
              }
          }
          else {
              print_usage(argv[ARG_NAME], "unknown optional argument given");
              exit(EINVAL);
          }
      }
  }
  else if (req_nr == RS_DOWN || req_nr == RS_REFRESH || req_nr == RS_RESTART
      || req_nr == RS_CLONE || req_nr == RS_UNCLONE || req_nr == RS_FI) {

      /* Verify argument count. */ 
      if (argc - 1 < optind+ARG_LABEL) {
          print_usage(argv[ARG_NAME], "action requires a target label");
          exit(EINVAL);
      }
      req_label= argv[optind+ARG_LABEL];
  }
  else if (req_nr == RS_SYSCTL) {
      char* sysctl_types[] = { "srv_status", "upd_start", "upd_run", "upd_stop",
          "upd_status", NULL };
      char* sysctl_type;
      int sysctl_type_values[] = { RS_SYSCTL_SRV_STATUS, RS_SYSCTL_UPD_START,
          RS_SYSCTL_UPD_RUN, RS_SYSCTL_UPD_STOP, RS_SYSCTL_UPD_STATUS };

      /* Verify argument count. */ 
      if (argc - 1 < optind+ARG_SYSCTL_TYPE) {
          print_usage(argv[ARG_NAME], "sysctl requires an action type");
          exit(EINVAL);
      }

      sysctl_type = argv[optind+ARG_SYSCTL_TYPE];
      for(i=0;sysctl_types[i]!=NULL;i++) {
          if(!strcmp(sysctl_types[i], sysctl_type)) {
              break;
          }
      }
      if(sysctl_types[i] == NULL) {
          print_usage(argv[ARG_NAME], "bad sysctl type");
          exit(EINVAL);
      }
      req_sysctl_type = sysctl_type_values[i];
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
  memset(&m, 0, sizeof(m));
  switch(request) {
  case RS_UPDATE:
      m.m_rs_update.state = req_lu_state;
      m.m_rs_update.prepare_maxtime = req_lu_maxtime;
      /* fall through */
  case RS_UP:
  case RS_EDIT:
      /* Build space-separated command string to be passed to RS server. */
      if (req_progname != NULL) {
        progname = req_progname;
      } else {
        progname = strrchr(req_path, '/');
        assert(progname);	/* an absolute path was required */
        progname++;	/* skip last slash */
      }
      strcpy(command, req_path);
      command[strlen(req_path)] = ' ';
      strcpy(command+strlen(req_path)+1, req_args);

      if (req_config) {
	assert(progname);
	memset(&config, 0, sizeof(config));
      	if(!parse_config(progname, custom_config_file, req_config, &config))
		errx(1, "couldn't parse config");
	assert(config.rs_start.rss_priority < NR_SCHED_QUEUES);
	assert(config.rs_start.rss_quantum > 0);
      }

      /* Set specifics */
      config.rs_start.rss_cmd= command;
      config.rs_start.rss_cmdlen= strlen(command);
      config.rs_start.rss_progname= progname;
      config.rs_start.rss_prognamelen= strlen(progname);
      config.rs_start.rss_major= req_major;
      config.rs_start.rss_period= req_period;
      config.rs_start.rss_script= req_script;
      config.rs_start.rss_asr_count= req_asr_count;
      config.rs_start.rss_restarts= req_restarts;
      config.rs_start.devman_id= devman_id;
      config.rs_start.rss_heap_prealloc_bytes= req_heap_prealloc;
      config.rs_start.rss_map_prealloc_bytes= req_map_prealloc;
      config.rs_start.rss_flags |= rss_flags;
      if(req_label) {
        config.rs_start.rss_label.l_addr = req_label;
        config.rs_start.rss_label.l_len = strlen(req_label);
      } else {
        config.rs_start.rss_label.l_addr = progname;
        config.rs_start.rss_label.l_len = strlen(progname);
      }
      if(req_trg_label) {
        config.rs_start.rss_trg_label.l_addr = req_trg_label;
        config.rs_start.rss_trg_label.l_len = strlen(req_trg_label);
      } else {
        config.rs_start.rss_trg_label.l_addr = 0;
        config.rs_start.rss_trg_label.l_len = 0;
      }
      if (req_script)
	      config.rs_start.rss_scriptlen= strlen(req_script);
      else
	      config.rs_start.rss_scriptlen= 0;

      /* State-related data. */
      config.rs_start.rss_state_data.size =
        sizeof(config.rs_start.rss_state_data);
      if(num_ipc_filters > 0) {
        config.rs_start.rss_state_data.ipcf_els = rs_ipc_filter_els;
        config.rs_start.rss_state_data.ipcf_els_size =
          num_ipc_filters*sizeof(rs_ipc_filter_els[0]);
      }
      else {
        config.rs_start.rss_state_data.ipcf_els = NULL;
        config.rs_start.rss_state_data.ipcf_els_size = 0;
      }
      if(req_state_eval) {
        config.rs_start.rss_state_data.eval_addr = req_state_eval;
        config.rs_start.rss_state_data.eval_len = strlen(req_state_eval);
      }
      else {
        config.rs_start.rss_state_data.eval_addr = NULL;
        config.rs_start.rss_state_data.eval_len = 0;
      }

      m.m_rs_req.addr = (char *) &config.rs_start;
      break;
  case RS_DOWN:
  case RS_REFRESH:
  case RS_RESTART:
  case RS_CLONE:
  case RS_UNCLONE:
  case RS_FI:
      m.m_rs_req.addr = req_label;
      m.m_rs_req.len = strlen(req_label);
      break;
  case RS_SYSCTL:
      m.m_rs_req.subtype = req_sysctl_type;
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

