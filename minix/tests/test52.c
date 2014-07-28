#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>

#define ROUNDS 20
#define SWAPS 40
int max_error = 5;
#include "common.h"



int pipefdc[2];
int pipefdp[2];
int subtest = 0;
int child_is_dead = 0;

void dead_child(int n);
void do_child(void);
void do_parent(void);
void do_calcs(void);
void err(int n);
void quit(void);

void err(int n)
{
  e(n);
}

void do_calcs(void)
{
  float a, b, c, d, e;
  float foo, bar;
  int i;
  subtest = 3;

  a = 1.1;
  b = 2.2;
  c = 3.3;
  d = 4.4;
  e = 5.5;

  foo = a * b; /* 2.42 */
  bar = c * d; /* 14.52 */

  i = 0;
  while(i < ROUNDS) {
	foo += c; /* 5.72 */
	foo *= d; /* 25.168 */
	foo /= e; /* 4.5760 */
	bar -= a; /* 13.42 */
	bar *= b; /* 29.524 */
	bar /= e; /* 5.3680 */

	/* Undo */
	foo *= e;
	foo /= d;
	foo -= c;

	bar *= e;
	bar /= b;
	bar += a;

	i++;
  }

  if (fabs(foo - (a * b)) > 0.0001) err(1);
  if (fabs(bar - (c * d)) > 0.0001) err(2);
}

void dead_child(int n)
{
  int status;
  subtest = 4;

  (void) n; /* Avoid warning about unused parameter */

  if (wait(&status) == -1) err(1);

  if (!WIFEXITED(status)) {
	err(2);
	quit();
  } else {
	errct += WEXITSTATUS(status);
	child_is_dead = 1;
  }
}

void do_child(void)
{
  char buf[2];
  int s;

  s = 0;
  close(pipefdp[0]);
  close(pipefdc[1]);

  while(s < SWAPS) {
	do_calcs();

	/* Wake up parent */
	write(pipefdp[1], buf, 1);
	
	/* Wait for parent to wake me up */
	read(pipefdc[0], buf, 1); 
	
	s++;
  }
  exit(0);
}

void do_parent(void)
{
  int s;
  char buf[2];
  struct sigaction sa;
  subtest = 2;

  sa.sa_handler = dead_child;
  sa.sa_flags = 0;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) err(1);

  s = 0;
  close(pipefdp[1]);
  close(pipefdc[0]);

  while(s < SWAPS) {
	/* Wait for child to wake me up */	
	read(pipefdp[0], buf, 1);

	do_calcs();

	/* Wake up child */
	write(pipefdc[1], buf, 1);
	s++;
  }

  while(child_is_dead == 0) { fflush(stdout); } /* Busy wait */

  quit();
}

int main(void)
{
  pid_t r;
  subtest = 1;

  start(52);

  if (pipe(pipefdc) == -1) err(1);
  if (pipe(pipefdp) == -1) err(2);

  r = fork();
  if(r < 0) {
	err(3);
  } else if(r == 0) {
	/* Child */
	do_child();
  } else {
	/* Parent */
	do_parent();
  }

  return(0); /* Never reached */

}

