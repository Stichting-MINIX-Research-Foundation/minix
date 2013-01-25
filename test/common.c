/* Utility routines for Minix tests.
 * This is designed to be #includ'ed near the top of test programs.  It is
 * self-contained except for MAX_ERRORS.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/statvfs.h>

int common_test_nr = -1, errct = 0, subtest;

#define e(errn) e_f(__FILE__, __LINE__, (errn))

void cleanup(void);
int does_fs_truncate(void);
void e_f(char *file, int lineno, int n);
int name_max(char *path);
void quit(void);
void rm_rf_dir(int test_nr);
void rm_rf_ppdir(int test_nr);
void start(int test_nr);

void start(test_nr)
int test_nr;
{
  char buf[64];
  int i;

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
  if (system(buf) != 0) printf("Warning: system(\"%s\") failed\n", buf);
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
	file, line, subtest, n, errno, strerror(errno));
  if (++errct > MAX_ERROR) {
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
