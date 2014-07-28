/* test 6 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int max_error = 3;
#include "common.h"


int subtest = 1;
int zilch[5000];


int main(int argc, char *argv []);
void test6a(void);
void test6b(void);

int main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  if (argc == 2) m = atoi(argv[1]);

  start(6);

  for (i = 0; i < 70; i++) {
	if (m & 00001) test6a();
	if (m & 00002) test6b();
  }

  quit();
  return(-1);			/* impossible */
}

void test6a()
{
/* Test sbrk() and brk(). */

  char *addr, *addr2, *addr3;
  int i, del, click, click2;

  subtest = 1;
  addr = sbrk(0);
  addr = sbrk(0);		/* force break to a click boundary */
  for (i = 0; i < 10; i++) sbrk(7 * i);
  for (i = 0; i < 10; i++) sbrk(-7 * i);
  if (sbrk(0) != addr) e(1);
  sbrk(30);
  if (brk(addr) != 0) e(2);
  if (sbrk(0) != addr) e(3);

  del = 0;
  do {
	del++;
	brk(addr + del);
	addr2 = sbrk(0);
  } while (addr2 == addr);
  click = addr2 - addr;
  sbrk(-1);
  if (sbrk(0) != addr) e(4);
  brk(addr);
  if (sbrk(0) != addr) e(5);

  del = 0;
  do {
	del++;
	brk(addr - del);
	addr3 = sbrk(0);
  } while (addr3 == addr);
  click2 = addr - addr3;
  sbrk(1);
  if (sbrk(0) != addr) e(6);
  brk(addr);
  if (sbrk(0) != addr) e(8);
  if (click != click2) e(9);

  brk(addr + 2 * click);
  if (sbrk(0) != addr + 2 * click) e(10);
  sbrk(3 * click);
  if (sbrk(0) != addr + 5 * click) e(11);
  sbrk(-5 * click);
  if (sbrk(0) != addr) e(12);
}

void test6b()
{
  int i, err;

  subtest = 2;
  signal(SIGQUIT, SIG_IGN);
  err = 0;
  for (i = 0; i < 5000; i++)
	if (zilch[i] != 0) err++;
  if (err > 0) e(1);
  kill(getpid(), SIGQUIT);
}

