/* Test 9 setjmp with register variables.	Author: Ceriel Jacobs */

#include <sys/types.h>
#include <setjmp.h>
#include <signal.h>

int max_error = 4;
#include "common.h"



char *tmpa;

int main(int argc, char *argv []);
void test9a(void);
void test9b(void);
void test9c(void);
void test9d(void);
void test9e(void);
void test9f(void);
char *addr(void);
void garbage(void);
void level1(void);
void level2(void);
void dolev(void);
void catch(int s);
void hard(void);

int main(argc, argv)
int argc;
char *argv[];
{
  jmp_buf envm;
  int i, j, m = 0xFFFF;

  start(9);
  if (argc == 2) m = atoi(argv[1]);
  for (j = 0; j < 100; j++) {
	if (m & 00001) test9a();
	if (m & 00002) test9b();
	if (m & 00004) test9c();
	if (m & 00010) test9d();
	if (m & 00020) test9e();
	if (m & 00040) test9f();
  }
  if (errct) quit();
  i = 1;
  if (setjmp(envm) == 0) {
	i = 2;
	longjmp(envm, 1);
  } else {
	if (i == 2) {
		/* Correct */
	} else if (i == 1) {
		printf("WARNING: The setjmp/longjmp of this machine restore register variables\n\
to the value they had at the time of the Setjmp\n");
	} else {
		printf("Aha, I just found one last error\n");
		return 1;
	}
  }
  quit();
  return(-1);			/* impossible */
}

void test9a()
{
  register int p;

  subtest = 1;
  p = 200;
  garbage();
  if (p != 200) e(1);
}

void test9b()
{
  register int p, q;

  subtest = 2;
  p = 200;
  q = 300;
  garbage();
  if (p != 200) e(1);
  if (q != 300) e(2);
}

void test9c()
{
  register int p, q, r;

  subtest = 3;
  p = 200;
  q = 300;
  r = 400;
  garbage();
  if (p != 200) e(1);
  if (q != 300) e(2);
  if (r != 400) e(3);
}

char buf[512];

void test9d()
{
  register char *p;

  subtest = 4;
  p = &buf[100];
  garbage();
  if (p != &buf[100]) e(1);
}

void test9e()
{
  register char *p, *q;

  subtest = 5;
  p = &buf[100];
  q = &buf[200];
  garbage();
  if (p != &buf[100]) e(1);
  if (q != &buf[200]) e(2);
}

void test9f()
{
  register char *p, *q, *r;

  subtest = 6;
  p = &buf[100];
  q = &buf[200];
  r = &buf[300];
  garbage();
  if (p != &buf[100]) e(1);
  if (q != &buf[200]) e(2);
  if (r != &buf[300]) e(3);
}

jmp_buf env;

/*	return address of local variable.
  This way we can check that the stack is not polluted.
*/
char *
 addr()
{
  char a, *ret;

  ret = &a;
  return(ret);
}

void garbage()
{
  register int i, j, k;
  register char *p, *q, *r;
  char *a = NULL;

  p = &buf[300];
  q = &buf[400];
  r = &buf[500];
  i = 10;
  j = 20;
  k = 30;
  switch (setjmp(env)) {
      case 0:
	a = addr();
#ifdef __GNUC__
	/*
	 * to defeat the smartness of the GNU C optimizer we pretend we
	 * use 'a'. Otherwise the optimizer will not detect the looping
	 * effectuated by setjmp/longjmp, so that it thinks it can get
	 * rid of the assignment to 'a'.
	 */
	srand((unsigned)&a);
#endif
	longjmp(env, 1);
	break;
      case 1:
	if (i != 10) e(11);
	if (j != 20) e(12);
	if (k != 30) e(13);
	if (p != &buf[300]) e(14);
	if (q != &buf[400]) e(15);
	if (r != &buf[500]) e(16);
	tmpa = addr();
	if (a != tmpa) e(17);
	level1();
	break;
      case 2:
	if (i != 10) e(21);
	if (j != 20) e(22);
	if (k != 30) e(23);
	if (p != &buf[300]) e(24);
	if (q != &buf[400]) e(25);
	if (r != &buf[500]) e(26);
	tmpa = addr();
	if (a != tmpa) e(27);
	level2();
	break;
      case 3:
	if (i != 10) e(31);
	if (j != 20) e(32);
	if (k != 30) e(33);
	if (p != &buf[300]) e(34);
	if (q != &buf[400]) e(35);
	if (r != &buf[500]) e(36);
	tmpa = addr();
	if (a != tmpa) e(37);
	hard();
      case 4:
	if (i != 10) e(41);
	if (j != 20) e(42);
	if (k != 30) e(43);
	if (p != &buf[300]) e(44);
	if (q != &buf[400]) e(45);
	if (r != &buf[500]) e(46);
	tmpa = addr();
	if (a != tmpa) e(47);
	return;
	break;
      default:	e(100);
  }
  e(200);
}

void level1()
{
  register char *p;
  register int i;

  i = 1000;
  p = &buf[10];
  i = 200;
  p = &buf[20];
 
#ifdef __GNUC__
	/*
	 * to defeat the smartness of the GNU C optimizer we pretend we
	 * use 'a'. Otherwise the optimizer will not detect the looping
	 * effectuated by setjmp/longjmp, so that it thinks it can get
	 * rid of the assignment to 'a'.
	 */
  srand(i);
  srand((int)*p);
#endif

  longjmp(env, 2);
}

void level2()
{
  register char *p;
  register int i;

  i = 0200;
  p = &buf[2];
  *p = i;
  dolev();
}

void dolev()
{
  register char *p;
  register int i;

  i = 010;
  p = &buf[3];
  *p = i;
  longjmp(env, 3);
}

void catch(s)
int s;
{
  longjmp(env, 4);
}

void hard()
{
  register char *p;

  signal(SIGHUP, catch);
  for (p = buf; p <= &buf[511]; p++) *p = 025;
  kill(getpid(), SIGHUP);
}
