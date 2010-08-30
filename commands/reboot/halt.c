/* halt / reboot - halt or reboot system (depends on name)

   halt   - calling reboot() with RBT_HALT
   reboot - calling reboot() with RBT_REBOOT

   author: Edvard Tuinder   v892231@si.hhs.NL

   This program calls the library function reboot(2) which performs
   the system-call do_reboot. 

 */

#define _POSIX_SOURCE	1
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

void write_log _ARGS(( void ));
void usage _ARGS(( void ));
int main _ARGS(( int argc, char *argv[] ));

char *prog;
char *reboot_code = "delay; boot";

void
usage()
{
  fprintf(stderr, "Usage: %s [-hrRfd] [-x reboot-code]\n", prog);
  exit(1);
}

int
main(argc,argv)
int argc;
char **argv;
{
  int flag = -1;		/* default action unknown */
  int fast = 0;			/* fast halt/reboot, don't bother being nice. */
  int i;
  struct stat dummy;
  char *monitor_code = "";
  pid_t pid;

  if ((prog = strrchr(argv[0],'/')) == NULL) prog = argv[0]; else prog++;

  if (strcmp(prog, "halt") == 0) flag = RBT_HALT;
  if (strcmp(prog, "reboot") == 0) flag = RBT_REBOOT;

  i = 1;
  while (i < argc && argv[i][0] == '-') {
    char *opt = argv[i++] + 1;

    if (*opt == '-' && opt[1] == 0) break;	/* -- */

    while (*opt != 0) switch (*opt++) {
      case 'h': flag = RBT_HALT; 	break;
      case 'r': flag = RBT_REBOOT; 	break;
      case 'R': flag = RBT_RESET; 	break;
      case 'd': flag = RBT_DEFAULT; 	break;
      case 'f': fast = 1; break;
      case 'x':
	flag = RBT_MONITOR;
	if (*opt == 0) {
	  if (i == argc) usage();
	  opt = argv[i++];
	}
	monitor_code = opt;
	opt = "";
	break;
      default:
	usage();
    }
  }

  if (i != argc) usage();

  if (flag == -1) {
    fprintf(stderr, "Don't know what to do when named '%s'\n", prog);
    exit(1);
  }

  if (flag == RBT_REBOOT) {
	flag = RBT_MONITOR;		/* set monitor code for reboot */
	monitor_code = reboot_code;
  }

  if (stat("/usr/bin", &dummy) < 0) {
    /* It seems that /usr isn't present, let's assume "-f." */
    fast = 1;
  }

  signal(SIGHUP, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  /* Skip this part for fast shut down. */
  if (! fast) {
    /* Run the shutdown scripts. */
    switch ((pid = fork())) {
      case -1:
	fprintf(stderr, "%s: can't fork(): %s\n", prog, strerror(errno));
	exit(1);
      case 0:
	execl("/bin/sh", "sh", "/etc/rc", "down", (char *) NULL);
	fprintf(stderr, "%s: can't execute: /bin/sh: %s\n",
	  prog, strerror(errno));
	exit(1);
      default:
	while (waitpid(pid, NULL, 0) != pid) {}
    }
  }

  /* Tell init to stop spawning getty's. */
  kill(1, SIGTERM);

  /* Extra sync for the case where SIGTERM causes deadlock */
  sync();

  /* Give everybody a chance to die peacefully. */
  printf("Sending SIGTERM to all processes ...\n");
  kill(-1, SIGTERM);
  sleep(1);

  write_log();

  sync();

  reboot(flag, monitor_code, strlen(monitor_code));
  fprintf(stderr, "%s: reboot(): %s\n", prog, strerror(errno));
  return 1;
}
