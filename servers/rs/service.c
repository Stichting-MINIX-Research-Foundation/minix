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
#include <sys/types.h>
#include <sys/stat.h>
#include <configfile.h>


/* This array defines all known requests. */
PRIVATE char *known_requests[] = {
  "up", 
  "down",
  "refresh", 
  "restart",
  "shutdown", 
  "update",
  "catch for illegal requests"
};
#define ILLEGAL_REQUEST  sizeof(known_requests)/sizeof(char *)

/* Global error number set for failed system calls. */
#define OK 0

#define RUN_CMD		"run"
#define RUN_SCRIPT	"/etc/rs.single"	/* Default script for 'run' */
#define PATH_CONFIG	_PATH_SYSTEM_CONF	/* Default config file */

/* Define names for arguments provided to this utility. The first few 
 * arguments are required and have a known index. Thereafter, some optional
 * argument pairs like "-args arglist" follow.
 */
#define ARG_NAME	0		/* own application name */

/* The following are relative to optind */
#define ARG_REQUEST	0		/* request to perform */
#define ARG_PATH	1		/* system service */
#define ARG_LABEL	1		/* name of system service */
#define ARG_LU_STATE		2	/* the state required to update */
#define ARG_PREPARE_MAXTIME	3	/* max time to prepare for the update */

#define MIN_ARG_COUNT	1		/* require an action */

#define ARG_ARGS	"-args"		/* list of arguments to be passed */
#define ARG_DEV		"-dev"		/* major device number for drivers */
#define ARG_PERIOD	"-period"	/* heartbeat period in ticks */
#define ARG_SCRIPT	"-script"	/* name of the script to restart a
					 * system service
					 */
#define ARG_LABELNAME	"-label"	/* custom label name */
#define ARG_CONFIG	"-config"	/* name of the file with the resource
					 * configuration 
					 */
#define ARG_PRINTEP	"-printep"	/* print endpoint number after start */

#define SERVICE_LOGIN	"service"	/* passwd file entry for services */

#define MAX_CLASS_RECURS	100	/* Max nesting level for classes */

/* The function parse_arguments() verifies and parses the command line 
 * parameters passed to this utility. Request parameters that are needed
 * are stored globally in the following variables:
 */
PRIVATE int req_type;
PRIVATE int do_run= 0;		/* 'run' command instead of 'up' */
PRIVATE char *req_label;
PRIVATE char *req_path;
PRIVATE char *req_args = "";
PRIVATE int req_major;
PRIVATE long req_period;
PRIVATE char *req_script;
PRIVATE char *req_ipc;
PRIVATE char *req_config = PATH_CONFIG;
PRIVATE int req_printep;
PRIVATE int class_recurs;	/* Nesting level of class statements */
PRIVATE int req_lu_state;
PRIVATE int req_prepare_maxtime;

/* Buffer to build "/command arg1 arg2 ..." string to pass to RS server. */
PRIVATE char command[4096];	

/* Arguments for RS to start a new service */
PRIVATE struct rs_start rs_start;

/* An error occurred. Report the problem, print the usage, and exit. 
 */
PRIVATE void print_usage(char *app_name, char *problem) 
{
  fprintf(stderr, "Warning, %s\n", problem);
  fprintf(stderr, "Usage:\n");
  fprintf(stderr,
  "    %s [-c -r] (up|run) <binary> [%s <args>] [%s <special>] [%s <ticks>]\n", 
	app_name, ARG_ARGS, ARG_DEV, ARG_PERIOD);
  fprintf(stderr, "    %s down label\n", app_name);
  fprintf(stderr, "    %s refresh label\n", app_name);
  fprintf(stderr, "    %s restart label\n", app_name);
  fprintf(stderr, "    %s update label state maxtime\n", app_name);
  fprintf(stderr, "    %s shutdown\n", app_name);
  fprintf(stderr, "\n");
}

/* A request to the RS server failed. Report and exit. 
 */
PRIVATE void failure(void) 
{
  fprintf(stderr, "Request to RS failed: %s (error %d)\n", strerror(errno), errno);
  exit(errno);
}


/* Parse and verify correctness of arguments. Report problem and exit if an 
 * error is found. Store needed parameters in global variables.
 */
PRIVATE int parse_arguments(int argc, char **argv)
{
  struct stat stat_buf;
  char *hz, *buff;
  int req_nr;
  int c, i;
  int c_flag, r_flag;

  c_flag = 0;
  r_flag = 0;
  while (c= getopt(argc, argv, "rci?"), c != -1)
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
	case 'i':
		/* Legacy - remove later */
		fputs("WARNING: obsolete -i flag passed to service(8)\n",
			stderr);
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

  if (req_nr == RS_UP) {
      rs_start.rss_flags= RSS_IPC_VALID;
      if (c_flag)
	rs_start.rss_flags |= RSS_COPY;

      if(r_flag)
        rs_start.rss_flags |= RSS_REUSE;
        
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
      req_path = argv[optind+ARG_PATH];
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
      for (i=optind+MIN_ARG_COUNT+1; i<argc; i=i+2) {
          if (! (i+1 < argc)) {
              print_usage(argv[ARG_NAME], "optional argument not complete");
              exit(EINVAL);
          }
          if (strcmp(argv[i], ARG_ARGS)==0) {
              req_args = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_PERIOD)==0) {
		u32_t system_hz;
		if(getsysinfo_up(PM_PROC_NR,
			SIU_SYSTEMHZ, sizeof(system_hz), &system_hz) < 0) {
			system_hz = DEFAULT_HZ;
			fprintf(stderr, "WARNING: reverting to default HZ %d\n",
				system_hz);
		} 

	      req_period = strtol(argv[i+1], &hz, 10);
	      if (strcmp(hz,"HZ")==0) req_period *= system_hz;
	      if (req_period < 1) {
                  print_usage(argv[ARG_NAME],
			"period is at least be one tick");
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
              req_major = (stat_buf.st_rdev >> MAJOR) & BYTE;
          }
          else if (strcmp(argv[i], ARG_SCRIPT)==0) {
              req_script = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_LABELNAME)==0) {
              req_label = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_CONFIG)==0) {
              req_config = argv[i+1];
          }
          else if (strcmp(argv[i], ARG_PRINTEP)==0) {
              req_printep = 1;
          }
          else {
              print_usage(argv[ARG_NAME], "unknown optional argument given");
              exit(EINVAL);
          }
      }
  }
  else if (req_nr == RS_DOWN || req_nr == RS_REFRESH || req_nr == RS_RESTART) {

      /* Verify argument count. */ 
      if (argc - 1 < optind+ARG_LABEL) {
          print_usage(argv[ARG_NAME], "action requires a label to stop");
          exit(EINVAL);
      }
      req_label= argv[optind+ARG_LABEL];
  } 
  else if (req_nr == RS_SHUTDOWN) {
        /* no extra arguments required */
  }
  else if (req_nr == RS_UPDATE) {
      /* Check for mandatory arguments */ 
      if (argc - 1 < optind+ARG_LU_STATE) {
          print_usage(argv[ARG_NAME],
              "action requires at least a label and a live update state");
          exit(EINVAL);
      }
      
      /* Label. */
      req_label= argv[optind+ARG_LABEL];
      
      /* Live update state. */
      errno=0;
      req_lu_state=strtol(argv[optind+ARG_LU_STATE], &buff, 10);
      if(errno || strcmp(buff, "")) {
          print_usage(argv[ARG_NAME],
              "action requires a correct live update state");
          exit(EINVAL);
      }
      if(req_lu_state == SEF_LU_STATE_NULL) {
          print_usage(argv[ARG_NAME],
              "action requires a non-null live update state.");
          exit(EINVAL);
      }
      
      /* Prepare max time. */
      req_prepare_maxtime=0;
      if (argc - 1 >= optind+ARG_PREPARE_MAXTIME) {
          req_prepare_maxtime=strtol(argv[optind+ARG_PREPARE_MAXTIME],
              &buff, 10);
          if(errno || strcmp(buff, "") || req_prepare_maxtime<0) {
              print_usage(argv[ARG_NAME],
                "action requires a correct max time to prepare for the update");
              exit(EINVAL);
          }
      }
  }

  /* Return the request number if no error were found. */
  return(req_nr);
}

PRIVATE void fatal(char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "fatal error: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	exit(1);
}

#define KW_SERVICE	"service"
#define KW_UID		"uid"
#define KW_NICE		"nice"
#define KW_IRQ		"irq"
#define KW_IO		"io"
#define KW_PCI		"pci"
#define KW_DEVICE	"device"
#define KW_CLASS	"class"
#define KW_SYSTEM	"system"
#define KW_IPC		"ipc"
#define KW_VM		"vm"
#define KW_CONTROL	"control"

FORWARD void do_service(config_t *cpe, config_t *config);

PRIVATE void do_class(config_t *cpe, config_t *config)
{
	config_t *cp, *cp1;

	if (class_recurs > MAX_CLASS_RECURS)
	{
		fatal(
		"do_class: nesting level too high for class '%s' at %s:%d",
			cpe->word, cpe->file, cpe->line);
	}
	class_recurs++;

	/* Process classes */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_class: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_uid: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}

		/* Find entry for the class */
		for (cp= config; cp; cp= cp->next)
		{
			if (!(cp->flags & CFG_SUBLIST))
			{
				fatal("do_class: expected list at %s:%d",
					cp->file, cp->line);
			}
			cp1= cp->list;
			if ((cp1->flags & CFG_STRING) ||
				(cp1->flags & CFG_SUBLIST))
			{
				fatal("do_class: expected word at %s:%d",
					cp1->file, cp1->line);
			}

			/* At this place we expect the word KW_SERVICE */
			if (strcmp(cp1->word, KW_SERVICE) != 0)
				fatal("do_class: exected word '%S' at %s:%d",
					KW_SERVICE, cp1->file, cp1->line);

			cp1= cp1->next;
			if ((cp1->flags & CFG_STRING) ||
				(cp1->flags & CFG_SUBLIST))
			{
				fatal("do_class: expected word at %s:%d",
					cp1->file, cp1->line);
			}

			/* At this place we expect the name of the service */
			if (strcmp(cp1->word, cpe->word) == 0)
				break;
		}
		if (cp == NULL)
		{
			fatal(
			"do_class: no entry found for class '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}
		do_service(cp1->next, config);
	}

	class_recurs--;
}

PRIVATE void do_uid(config_t *cpe)
{
	uid_t uid;
	struct passwd *pw;
	char *check;

	/* Process a uid */
	if (cpe->next != NULL)
	{
		fatal("do_uid: just one uid/login expected at %s:%d",
			cpe->file, cpe->line);
	}	

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_uid: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->flags & CFG_STRING)
	{
		fatal("do_uid: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}
	pw= getpwnam(cpe->word);
	if (pw != NULL)
		uid= pw->pw_uid;
	else
	{
		uid= strtol(cpe->word, &check, 0);
		if (check[0] != '\0')
		{
			fatal("do_uid: bad uid/login '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}
	}

	rs_start.rss_uid= uid;
}

PRIVATE void do_nice(config_t *cpe)
{
	int nice_val;
	char *check;

	/* Process a nice value */
	if (cpe->next != NULL)
	{
		fatal("do_nice: just one nice value expected at %s:%d",
			cpe->file, cpe->line);
	}	
	

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_nice: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->flags & CFG_STRING)
	{
		fatal("do_nice: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}
	nice_val= strtol(cpe->word, &check, 0);
	if (check[0] != '\0')
	{
		fatal("do_nice: bad nice value '%s' at %s:%d",
			cpe->word, cpe->file, cpe->line);
	}
	/* Check range? */

	rs_start.rss_nice= nice_val;
}

PRIVATE void do_irq(config_t *cpe)
{
	int irq;
	char *check;

	/* Process a list of IRQs */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_irq: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_irq: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}
		irq= strtoul(cpe->word, &check, 0);
		if (check[0] != '\0')
		{
			fatal("do_irq: bad irq '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}
		if (rs_start.rss_nr_irq >= RSS_NR_IRQ)
			fatal("do_irq: too many IRQs (max %d)", RSS_NR_IRQ);
		rs_start.rss_irq[rs_start.rss_nr_irq]= irq;
		rs_start.rss_nr_irq++;
	}
}

PRIVATE void do_io(config_t *cpe)
{
	int irq;
	unsigned base, len;
	char *check;

	/* Process a list of I/O ranges */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_io: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_io: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}
		base= strtoul(cpe->word, &check, 0x10);
		len= 1;
		if (check[0] == ':')
		{
			len= strtoul(check+1, &check, 0x10);
		}
		if (check[0] != '\0')
		{
			fatal("do_io: bad I/O range '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}

		if (rs_start.rss_nr_io >= RSS_NR_IO)
			fatal("do_io: too many I/O ranges (max %d)", RSS_NR_IO);
		rs_start.rss_io[rs_start.rss_nr_io].base= base;
		rs_start.rss_io[rs_start.rss_nr_io].len= len;
		rs_start.rss_nr_io++;
	}
}

PRIVATE void do_pci_device(config_t *cpe)
{
	u16_t vid, did;
	char *check, *check2;

	/* Process a list of PCI device IDs */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_pci_device: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_pci_device: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}
		vid= strtoul(cpe->word, &check, 0x10);
		if (check[0] == '/')
			did= strtoul(check+1, &check2, 0x10);
		if (check[0] != '/' || check2[0] != '\0')
		{
			fatal("do_pci_device: bad ID '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}
		if (rs_start.rss_nr_pci_id >= RS_NR_PCI_DEVICE)
		{
			fatal("do_pci_device: too many device IDs (max %d)",
				RS_NR_PCI_DEVICE);
		}
		rs_start.rss_pci_id[rs_start.rss_nr_pci_id].vid= vid;
		rs_start.rss_pci_id[rs_start.rss_nr_pci_id].did= did;
		rs_start.rss_nr_pci_id++;
	}
}

PRIVATE void do_pci_class(config_t *cpe)
{
	u8_t baseclass, subclass, interface;
	u32_t class_id, mask;
	char *check;

	/* Process a list of PCI device class IDs */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_pci_device: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_pci_device: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}

		baseclass= strtoul(cpe->word, &check, 0x10);
		subclass= 0;
		interface= 0;
		mask= 0xff0000;
		if (check[0] == '/')
		{
			subclass= strtoul(check+1, &check, 0x10);
			mask= 0xffff00;
			if (check[0] == '/')
			{
				interface= strtoul(check+1, &check, 0x10);
				mask= 0xffffff;
			}
		}

		if (check[0] != '\0')
		{
			fatal("do_pci_class: bad class ID '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}
		class_id= (baseclass << 16) | (subclass << 8) | interface;
		if (rs_start.rss_nr_pci_class >= RS_NR_PCI_CLASS)
		{
			fatal("do_pci_class: too many class IDs (max %d)",
				RS_NR_PCI_CLASS);
		}
		rs_start.rss_pci_class[rs_start.rss_nr_pci_class].class=
			class_id;
		rs_start.rss_pci_class[rs_start.rss_nr_pci_class].mask= mask;
		rs_start.rss_nr_pci_class++;
	}
}

PRIVATE void do_pci(config_t *cpe)
{
	int i, call_nr, word, bits_per_word;
	unsigned long mask;

	if (cpe == NULL)
		return;	/* Empty PCI statement */

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_pci: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->flags & CFG_STRING)
	{
		fatal("do_pci: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}

	if (strcmp(cpe->word, KW_DEVICE) == 0)
	{
		do_pci_device(cpe->next);
		return;
	}
	if (strcmp(cpe->word, KW_CLASS) == 0)
	{
		do_pci_class(cpe->next);
		return;
	}
	fatal("do_pci: unexpected word '%s' at %s:%d",
		cpe->word, cpe->file, cpe->line);
}

struct
{
	char *label;
	int call_nr;
} system_tab[]=
{
	{ "EXIT",		SYS_EXIT },
	{ "PRIVCTL",		SYS_PRIVCTL },
	{ "TRACE",		SYS_TRACE },
	{ "KILL",		SYS_KILL },
	{ "UMAP",		SYS_UMAP },
	{ "VIRCOPY",		SYS_VIRCOPY },
	{ "IRQCTL",		SYS_IRQCTL },
	{ "INT86",		SYS_INT86 },
	{ "DEVIO",		SYS_DEVIO },
	{ "SDEVIO",		SYS_SDEVIO },
	{ "VDEVIO",		SYS_VDEVIO },
	{ "SETALARM",		SYS_SETALARM },
	{ "TIMES",		SYS_TIMES },
	{ "GETINFO",		SYS_GETINFO },
	{ "SAFECOPYFROM",	SYS_SAFECOPYFROM },
	{ "SAFECOPYTO",		SYS_SAFECOPYTO },
	{ "SAFEMAP",		SYS_SAFEMAP },
	{ "SAFEREVMAP",		SYS_SAFEREVMAP },
	{ "SAFEUNMAP",		SYS_SAFEUNMAP },
	{ "VSAFECOPY",		SYS_VSAFECOPY },
	{ "SETGRANT",		SYS_SETGRANT },
	{ "READBIOS",		SYS_READBIOS },
	{ "PROFBUF",		SYS_PROFBUF },
	{ "STIME",		SYS_STIME },
	{ "VMCTL",		SYS_VMCTL },
	{ "SYSCTL",		SYS_SYSCTL },
	{ NULL,		0 }
};

PRIVATE void do_ipc(config_t *cpe)
{
	char *list;
	size_t listsize, wordlen;

	list= NULL;
	listsize= 1;
	list= malloc(listsize);
	if (list == NULL)
		fatal("do_ipc: unable to malloc %d bytes", listsize);
	list[0]= '\0';

	/* Process a list of process names that are allowed to be
	 * contacted
	 */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_ipc: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_ipc: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}

		wordlen= strlen(cpe->word);

		listsize += 1 + wordlen;
		list= realloc(list, listsize);
		if (list == NULL)
		{
			fatal("do_ipc: unable to realloc %d bytes",
				listsize);
		}
		strcat(list, " ");
		strcat(list, cpe->word);
	}
#if 0
	printf("do_ipc: got list '%s'\n", list);
#endif

	if (req_ipc)
		fatal("do_ipc: req_ipc is set");
	req_ipc= list;
}

struct
{
	char *label;
	int call_nr;
} vm_table[] =
{
	{ "REMAP",		VM_REMAP },
	{ "UNREMAP",		VM_SHM_UNMAP },
	{ "GETPHYS",		VM_GETPHYS },
	{ "GETREFCNT",		VM_GETREF },
	{ "QUERYEXIT",		VM_QUERY_EXIT },
	{ "INFO",		VM_INFO },
	{ NULL,			0 },
};

PRIVATE void do_vm(config_t *cpe)
{
	int i;

	for (; cpe; cpe = cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_vm: unexpected sublist at %s:%d",
			      cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_vm: unexpected string at %s:%d",
			      cpe->file, cpe->line);
		}

		for (i = 0; vm_table[i].label != NULL; i++)
			if (!strcmp(cpe->word, vm_table[i].label))
				break;
		if (vm_table[i].label == NULL)
			fatal("do_vm: unknown call '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		SET_BIT(rs_start.rss_vm, vm_table[i].call_nr - VM_RQ_BASE);
	}
}

PRIVATE void do_system(config_t *cpe)
{
	int i, call_nr, word, bits_per_word;
	unsigned long mask;

	bits_per_word= sizeof(rs_start.rss_system[0])*8;

	/* Process a list of 'system' calls that are allowed */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_system: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_system: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}

		/* Get call number */
		for (i= 0; system_tab[i].label != NULL; i++)
		{
			if (strcmp(cpe->word, system_tab[i].label) == 0)
				break;
		}
		if (system_tab[i].label == NULL)
		{
			fatal("do_system: unknown call '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}
		call_nr= system_tab[i].call_nr;

		/* Subtract KERNEL_CALL */
		if (call_nr < KERNEL_CALL)
		{
			fatal(
		"do_system: bad call number %d in system tab for '%s'",
				call_nr, system_tab[i].label);
		}
		call_nr -= KERNEL_CALL;

		word= call_nr / bits_per_word;
		mask= (1UL << (call_nr % bits_per_word));

		if (word >= RS_SYS_CALL_MASK_SIZE)
		{
			fatal(
			"RS_SYS_CALL_MASK_SIZE is too small (%d needed)",
				word+1);
		}
		rs_start.rss_system[word] |= mask;
	}
}

PRIVATE void do_control(config_t *cpe)
{
	int nr_control = 0;

	/* Process a list of 'control' labels. */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_control: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_control: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}
		if (nr_control >= RS_NR_CONTROL)
		{
			fatal(
			"do_control: RS_NR_CONTROL is too small (%d needed)",
				nr_control+1);
		}

		rs_start.rss_control[nr_control].l_addr = cpe->word;
		rs_start.rss_control[nr_control].l_len = strlen(cpe->word);
		rs_start.rss_nr_control = ++nr_control;
	}
}

PRIVATE void do_service(config_t *cpe, config_t *config)
{
	config_t *cp;

	/* At this point we expect one sublist that contains the varios
	 * resource allocations
	 */
	if (!(cpe->flags & CFG_SUBLIST))
	{
		fatal("do_service: expected list at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->next != NULL)
	{
		cpe= cpe->next;
		fatal("do_service: expected end of list at %s:%d",
			cpe->file, cpe->line);
	}
	cpe= cpe->list;

	/* Process the list */
	for (cp= cpe; cp; cp= cp->next)
	{
		if (!(cp->flags & CFG_SUBLIST))
		{
			fatal("do_service: expected list at %s:%d",
				cp->file, cp->line);
		}
		cpe= cp->list;
		if ((cpe->flags & CFG_STRING) || (cpe->flags & CFG_SUBLIST))
		{
			fatal("do_service: expected word at %s:%d",
				cpe->file, cpe->line);
		}

		if (strcmp(cpe->word, KW_CLASS) == 0)
		{
			do_class(cpe->next, config);
			continue;
		}
		if (strcmp(cpe->word, KW_UID) == 0)
		{
			do_uid(cpe->next);
			continue;
		}
		if (strcmp(cpe->word, KW_NICE) == 0)
		{
			do_nice(cpe->next);
			continue;
		}
		if (strcmp(cpe->word, KW_IRQ) == 0)
		{
			do_irq(cpe->next);
			continue;
		}
		if (strcmp(cpe->word, KW_IO) == 0)
		{
			do_io(cpe->next);
			continue;
		}
		if (strcmp(cpe->word, KW_PCI) == 0)
		{
			do_pci(cpe->next);
			continue;
		}
		if (strcmp(cpe->word, KW_SYSTEM) == 0)
		{
			do_system(cpe->next);
			continue;
		}
		if (strcmp(cpe->word, KW_IPC) == 0)
		{
			do_ipc(cpe->next);
			continue;
		}
		if (strcmp(cpe->word, KW_VM) == 0)
		{
			do_vm(cpe->next);
			continue;
		}
		if (strcmp(cpe->word, KW_CONTROL) == 0)
		{
			do_control(cpe->next);
			continue;
		}
	}
}

PRIVATE void do_config(char *label, char *filename)
{
	config_t *config, *cp, *cpe;

	config= config_read(filename, 0, NULL);
	if (config == NULL)
	{
		fprintf(stderr, "config_read failed for '%s': %s\n",
			filename, strerror(errno));
		exit(1);
	}

	/* Find an entry for our service */
	for (cp= config; cp; cp= cp->next)
	{
		if (!(cp->flags & CFG_SUBLIST))
		{
			fatal("do_config: expected list at %s:%d",
				cp->file, cp->line);
		}
		cpe= cp->list;
		if ((cpe->flags & CFG_STRING) || (cpe->flags & CFG_SUBLIST))
		{
			fatal("do_config: expected word at %s:%d",
				cpe->file, cpe->line);
		}

		/* At this place we expect the word KW_SERVICE */
		if (strcmp(cpe->word, KW_SERVICE) != 0)
			fatal("do_config: exected word '%S' at %s:%d",
				KW_SERVICE, cpe->file, cpe->line);

		cpe= cpe->next;
		if ((cpe->flags & CFG_STRING) || (cpe->flags & CFG_SUBLIST))
		{
			fatal("do_config: expected word at %s:%d",
				cpe->file, cpe->line);
		}

		/* At this place we expect the name of the service. */
		if (strcmp(cpe->word, label) == 0)
			break;
	}
	if (cp == NULL)
	{
		fprintf(stderr, "service: service '%s' not found in '%s'\n",
			label, filename);
		exit(1);
	}

	cpe= cpe->next;

	do_service(cpe, config);
}

/* Main program. 
 */
PUBLIC int main(int argc, char **argv)
{
  message m;
  int result = EXIT_SUCCESS;
  int request;
  int i;
  char *label, *progname = NULL;
  struct passwd *pw;

  /* Verify and parse the command line arguments. All arguments are checked
   * here. If an error occurs, the problem is reported and exit(2) is called. 
   * all needed parameters to perform the request are extracted and stored
   * global variables. 
   */
  request = parse_arguments(argc, argv);

  if(req_path) {
	/* Obtain binary name. */
	progname = strrchr(req_path, '/');
	assert(progname);	/* an absolute path was required */
	progname++;	/* skip last slash */
  }

  /* Arguments seem fine. Try to perform the request. Only valid requests 
   * should end up here. The default is used for not yet supported requests. 
   */
  switch(request) {
  case RS_UP:
      /* Build space-separated command string to be passed to RS server. */
      strcpy(command, req_path);
      command[strlen(req_path)] = ' ';
      strcpy(command+strlen(req_path)+1, req_args);

      rs_start.rss_cmd= command;
      rs_start.rss_cmdlen= strlen(command);
      rs_start.rss_major= req_major;
      rs_start.rss_period= req_period;
      rs_start.rss_script= req_script;
      if(req_label) {
        rs_start.rss_label.l_addr = req_label;
        rs_start.rss_label.l_len = strlen(req_label);
      } else {
        rs_start.rss_label.l_addr = progname;
        rs_start.rss_label.l_len = strlen(progname);
      }
      if (req_script)
	      rs_start.rss_scriptlen= strlen(req_script);
      else
	      rs_start.rss_scriptlen= 0;

      pw= getpwnam(SERVICE_LOGIN);
      if (pw == NULL)
	fatal("no passwd file entry for '%s'", SERVICE_LOGIN);
      rs_start.rss_uid= pw->pw_uid;

      /* The name of the system service. */
      (label= strrchr(req_path, '/')) ? label++ : (label= req_path);

      if (req_config) {
	assert(progname);
	do_config(progname, req_config);
      }

      if (req_ipc)
      {
	      rs_start.rss_ipc= req_ipc+1;	/* Skip initial space */
	      rs_start.rss_ipclen= strlen(rs_start.rss_ipc);
      }
      else
      {
	      rs_start.rss_ipc= NULL;
	      rs_start.rss_ipclen= 0;
      }

      m.RS_CMD_ADDR = (char *) &rs_start;

      /* Build request message and send the request. */
      if (_syscall(RS_PROC_NR, request, &m) == -1) 
          failure();
      else if(req_printep)
	printf("%d\n", m.RS_ENDPOINT);	
      result = m.m_type;
      break;

  case RS_DOWN:
  case RS_REFRESH:
  case RS_RESTART:
      m.RS_CMD_ADDR = req_label;
      m.RS_CMD_LEN = strlen(req_label);
      if (_syscall(RS_PROC_NR, request, &m) == -1) 
          failure();
      break;
  case RS_SHUTDOWN:
      if (_syscall(RS_PROC_NR, request, &m) == -1) 
          failure();
      break;
  case RS_UPDATE:
      m.RS_CMD_ADDR = req_label;
      m.RS_CMD_LEN = strlen(req_label);
      m.RS_LU_STATE = req_lu_state;
      m.RS_LU_PREPARE_MAXTIME = req_prepare_maxtime;
      if (_syscall(RS_PROC_NR, request, &m) == -1) 
          failure();
      break;
  default:
      print_usage(argv[ARG_NAME], "request is not yet supported");
      result = EGENERIC;
  }
  return(result);
}

