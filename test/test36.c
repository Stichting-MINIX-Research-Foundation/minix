/* test36: pathconf() fpathconf()	Author: Jan-mark Wams */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>

#define MAX_ERROR	4
#define ITERATIONS     10

#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)	if (stat(a,b) != 0) printf("Can't stat %s\n", a)

#include "common.c"

int superuser;

void test36a(void);
void test36b(void);
void test36c(void);
void test36d(void);
int not_provided_option(int _option);
int provided_option(int _option, int _minimum_value);
int variating_option(int _option, int _minimum_value);

char *testdirs[] = {
	    "/",
	    "/etc",
	    "/tmp",
	    "/usr",
	    "/usr/bin",
	    ".",
	    NULL
};

char *testfiles[] = {
	     "/",
	     "/etc",
	     "/etc/passwd",
	     "/tmp",
	     "/dev/tty",
	     "/usr",
	     "/usr/bin",
	     ".",
	     NULL
};

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  sync();
  start(36);
  if (argc == 2) m = atoi(argv[1]);
  superuser = (geteuid() == 0);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test36a();
	if (m & 0002) test36b();
	if (m & 0004) test36c();
	if (m & 0010) test36d();
  }
  quit();
  return 1;
}

void test36a()
{				/* Test normal operation. */
  subtest = 1;
  System("rm -rf ../DIR_36/*");

#ifdef _POSIX_CHOWN_RESTRICTED
# if _POSIX_CHOWN_RESTRICTED - 0 == -1
  if (not_provided_option(_PC_CHOWN_RESTRICTED) != 0) e(1);
# else
  if (provided_option(_PC_CHOWN_RESTRICTED, 0) != 0) e(2);
# endif
#else
  if (variating_option(_PC_CHOWN_RESTRICTED, 0) != 0) e(3);
#endif

#ifdef _POSIX_NO_TRUNC
# if _POSIX_NO_TRUNC - 0 == -1
  if (not_provided_option(_PC_NO_TRUNC) != 0) e(4);
# else
  if (provided_option(_PC_NO_TRUNC, 0) != 0) e(5);
# endif
#else
  if (variating_option(_PC_NO_TRUNC, 0) != 0) e(6);
#endif

#ifdef _POSIX_VDISABLE
 {
	int _posix_vdisable = _POSIX_VDISABLE;
	if(_posix_vdisable == -1) {
		if (not_provided_option(_PC_VDISABLE) != 0) e(7);
	} else {
		if (provided_option(_PC_VDISABLE, 0) != 0) e(8);
	}
 }
#else
  if (variating_option(_PC_VDISABLE, 0) != 0) e(9);
#endif

}

void test36b()
{
  subtest = 2;
  System("rm -rf ../DIR_36/*");
}

void test36c()
{
  subtest = 3;
  System("rm -rf ../DIR_36/*");
}

void test36d()
{
  subtest = 4;
  System("rm -rf ../DIR_36/*");
}

int not_provided_option(option)
int option;
{
  char **p;

  for (p = testfiles; *p != (char *) NULL; p++) {
	if (pathconf(*p, option) != -1) return printf("*p == %s\n", *p), 1;
  }
  return 0;
}

int provided_option(option, minimum)
int option, minimum;
{
  char **p;

  /* These three options are only defined on directorys. */
  if (option == _PC_NO_TRUNC
      || option == _PC_NAME_MAX
      || option == _PC_PATH_MAX)
	p = testdirs;
  else
	p = testfiles;

  for (; *p != NULL; p++) {
	if (pathconf(*p, option) < minimum)
		return printf("*p == %s\n", *p), 1;
  }
  return 0;
}

int variating_option(option, minimum)
int option, minimum;
{
  char **p;

  /* These three options are only defined on directorys. */
  if (option == _PC_NO_TRUNC
      || option == _PC_NAME_MAX
      || option == _PC_PATH_MAX)
	p = testdirs;
  else
	p = testfiles;

  for (; *p != NULL; p++) {
	if (pathconf(*p, option) < minimum)
		return printf("*p == %s\n", *p), 1;
  }
  return 0;
}
