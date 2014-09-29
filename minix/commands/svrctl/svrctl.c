#define _MINIX_SYSTEM 1

#include <sys/svrctl.h>
#include <sys/types.h>
#include <ctype.h>
#include <lib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void);

#define VFS			"vfs"
#define PM			"pm"
#define SET			"set"
#define GET			"get"

static char *bin_name;

int main (int argc, char *argv[])
{
  unsigned long param;
  endpoint_t proc_e = NONE;
  struct sysgetenv sysgetenv;
  char *to_whom, *operation, *what, *value;
  unsigned i;

  bin_name = argv[0];
  if (argc < 4 || argc > 5) usage();
  if (geteuid() != 0) {
	fprintf(stderr, "You have to be root to run this utility\n");
	exit(EXIT_FAILURE);
  }

  /* Make some parameters lower case to ease comparing */
  to_whom = argv[1];
  operation = argv[2];
  what = argv[3];
  for (i = 0; i < strlen(to_whom); ++i)   to_whom[i] = tolower(to_whom[i]);
  for (i = 0; i < strlen(operation); ++i) operation[i] = tolower(operation[i]);
  for (i = 0; i < strlen(what); ++i)      what[i] = tolower(what[i]);

  if (!strncmp(to_whom, VFS, strlen(VFS)+1)) proc_e = VFS_PROC_NR;
  else if (!strncmp(to_whom, PM, strlen(PM)+1)) proc_e = PM_PROC_NR;
  else usage();

  sysgetenv.key = what;
  sysgetenv.keylen = strlen(what) + 1;

  if (!strncmp(operation, SET, strlen(SET)+1)) {
	if (argc != 5) usage();
	value = argv[4];
	sysgetenv.val = value;
	sysgetenv.vallen = strlen(value) + 1;

	if (proc_e == VFS_PROC_NR)
		param = VFSSETPARAM;
	else if (proc_e == PM_PROC_NR)
		param = PMSETPARAM;
	else
		usage();

	if (svrctl(param, &sysgetenv) != 0) {
		if (errno == ESRCH)
			fprintf(stderr, "invalid parameter: %s\n", what);
		else if (errno == EINVAL)
			fprintf(stderr, "invalid value: %s\n", value);
		else
			perror("");
		exit(EXIT_FAILURE);
	}
	return(EXIT_SUCCESS);
  } else if (!strncmp(operation, GET, strlen(GET)+1)) {
	char get_param_buffer[4096];

	memset(get_param_buffer, '\0', sizeof(get_param_buffer));
	sysgetenv.val = get_param_buffer;
	sysgetenv.vallen = sizeof(get_param_buffer) - 1;

	if (proc_e == VFS_PROC_NR)
		param = VFSGETPARAM;
	else if (proc_e == PM_PROC_NR)
		param = PMGETPARAM;
	else
		usage();

	if (svrctl(param, &sysgetenv) != 0) {
		if (errno == ESRCH)
			fprintf(stderr, "invalid parameter: %s\n", what);
		else
			perror("");
		return(EXIT_FAILURE);
	} else {
		if (sysgetenv.vallen > 0) {
			get_param_buffer[sysgetenv.vallen] = '\0';
			printf("%s\n", get_param_buffer);
		}
	}
	return(EXIT_SUCCESS);
  } else
	usage();

  return(EXIT_FAILURE);
}

static void usage()
{
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "  %s <vfs|pm> set <request> <value>\n", bin_name);
  fprintf(stderr, "  %s <vfs|pm> get <request>\n", bin_name);
  exit(EXIT_FAILURE);
}
