/* Utility routines for Minix tests.
 * This is designed to be #includ'ed near the top of test programs.  It is
 * self-contained except for max_error.
 */

#include <errno.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/statvfs.h>
#include <sys/syslimits.h>
#include <sys/wait.h>

#include "common.h"

int common_test_nr = -1, errct = 0, subtest;
int quietflag = 1, bigflag = 0;

/* provide a default max_error symbol as Max_error with a value
 * of 5. The test program can override it wit its own max_error
 * symbol if it wants that this code will then use instead.
 */
__weak_alias(max_error,Max_error);
int Max_error = 5;
extern int max_error;

void start(test_nr)
int test_nr;
{
  char buf[64];
  int i;

  /* if this variable is set, specify to tests we are running
   * in 'overnight' mode
   */
  bigflag = !!getenv(BIGVARNAME);

  common_test_nr = test_nr;
  printf("Test %2d ", test_nr);
  fflush(stdout);		/* since stdout is probably line buffered */
  sync();
  rm_rf_dir(test_nr);
  sprintf(buf, "mkdir DIR_%02d", test_nr);
  if (system(buf) != 0) {
	e(666);
	quit();
  }
  sprintf(buf, "DIR_%02d", test_nr);
  if (chdir(buf) != 0) {
	e(6666);
	quit();
  }

  for (i = 3; i < OPEN_MAX; ++i) {
	/* Close all files except stdin, stdout, and stderr */
	(void) close(i);
  }
}

int does_fs_truncate(void)
{
  struct statvfs stvfs;
  int does_truncate = 0;
  char cwd[PATH_MAX];		/* Storage for path to current working dir */

  if (realpath(".", cwd) == NULL) e(7777);	/* Get current working dir */
  if (statvfs(cwd, &stvfs) != 0) e(7778);	/* Get FS information */
  /* Depending on how an FS handles too long file names, we have to adjust our
   * error checking. If an FS does not truncate file names, it should generate
   * an ENAMETOOLONG error when we provide too long a file name.
   */
  if (!(stvfs.f_flag & ST_NOTRUNC)) does_truncate = 1;
 
  return(does_truncate); 
}

int name_max(char *path)
{
  struct statvfs stvfs;

  if (statvfs(path, &stvfs) != 0) e(7779);
  return(stvfs.f_namemax);
}


void rm_rf_dir(test_nr)
int test_nr;
{
  char buf[128];

  sprintf(buf, "rm -rf DIR_%02d >/dev/null 2>&1", test_nr);
  if (system_p(buf) != 0) printf("Warning: system(\"%s\") failed\n", buf);
}

void rm_rf_ppdir(test_nr)
int test_nr;
{
/* Attempt to remove everything in the test directory (== the current dir). */

  char buf[128];

  sprintf(buf, "chmod 777 ../DIR_%02d/* ../DIR_%02d/*/* >/dev/null 2>&1",
	  test_nr, test_nr);
  (void) system(buf);
  sprintf(buf, "rm -rf ../DIR_%02d >/dev/null 2>&1", test_nr);
  if (system(buf) != 0) printf("Warning: system(\"%s\") failed\n", buf);
}

void e_f(char *file, int line, int n)
{
  int err_number;
  err_number = errno;	/* Store before printf can clobber it */
  if (errct == 0) printf("\n");	/* finish header */
  printf("%s:%d: Subtest %d,  error %d,  errno %d: %s\n",
	file, line, subtest, n, err_number, strerror(err_number));
  if (++errct > max_error) {
	printf("Too many errors; test aborted\n");
	cleanup();
	exit(1);
  }
  errno = err_number;	
}

void cleanup()
{
  if (chdir("..") == 0 && common_test_nr != -1) rm_rf_dir(common_test_nr);
}

void fail_printf(const char *file, const char *func, int line,
	const char *fmt, ...) {
	va_list ap;
	char buf[1024];
	size_t len;

	len = snprintf(buf, sizeof(buf), "[%s:%s:%d] ", file, func, line);

	va_start(ap, fmt);
	len += vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
	va_end(ap);

	snprintf(buf + len, sizeof(buf) - len, " errno=%d error=%s",
		errno, strerror(errno));

	em(line, buf);
}

void quit()
{
  cleanup();
  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}

void
printprogress(char *msg, int i, int max)
{
        int use_i = i + 1;
        static time_t start_time, prev_time;
        static int prev_i;
        time_t now;

	if(quietflag) return;

        time(&now);
        if(prev_i >= i) start_time = now;

        if(now > start_time && prev_time < now) {
                double i_per_sec = i / (now - start_time);
                int remain_secs;

                remain_secs = (int)((max-i) / i_per_sec);

                fprintf(stderr, "%-35s  %7d/%7d  %3d%%  ETA %3ds\r", msg,
                      use_i, (max), use_i*100/(max), remain_secs);
                fflush(stderr);
        }

        if(use_i >= max) {
                fprintf(stderr, "%-35s  done                                      \n", msg);
        }

        prev_i = i;
        prev_time = now;
}

void getmem(uint32_t *total, uint32_t *free, uint32_t *cached)
{
        uint32_t pagesize, largest;
        FILE *f = fopen("/proc/meminfo", "r");
        if(!f) return;
        if(fscanf(f, "%u %u %u %u %u", &pagesize, total, free,
                &largest, cached) != 5) {
		fprintf(stderr, "fscanf of meminfo failed\n");
		exit(1);
	}
        fclose(f);
}

static int get_setting(const char *name, int def)
{
	const char *value;

	value = getenv(name);
	if (!value || !*value) return def;
	if (strcmp(value, "yes") == 0) return 1;
	if (strcmp(value, "no") == 0) return 0;

	fprintf(stderr, "warning: invalid $%s value: %s\n", name, value);
	return 0;
}

int get_setting_use_network(void)
{
	/* set $USENETWORK to "yes" or "no" to indicate whether
	 * an internet connection is to be expected; defaults to "no"
	 */
	return get_setting("USENETWORK", 0);
}

int
system_p(const char *command)
{
	extern char **environ;
	pid_t pid;
	int pstat;
	const char *argp[] = {"sh", "-c", "-p", NULL, NULL};
	argp[3] = command;

	switch(pid = vfork()) {
	case -1:			/* error */
		return -1;
	case 0:				/* child */
		execve(_PATH_BSHELL, __UNCONST(argp), environ);
		_exit(127);
	}

	while (waitpid(pid, &pstat, 0) == -1) {
		if (errno != EINTR) {
			pstat = -1;
			break;
		}
	}

	return (pstat);
}

