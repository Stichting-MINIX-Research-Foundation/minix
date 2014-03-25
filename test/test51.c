/* Test51.c
 *
 * Test getcontext, setcontext, makecontext, and swapcontext system calls.
 *
 * Part of this test is somewhat based on the GNU GCC ucontext test set.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ucontext.h>
#include <math.h>
#include <fenv.h>

#include <sys/signal.h>

/* We can't use a global subtest variable reliably, because threads might
   change the value when we reenter a thread (i.e., report wrong subtest
   number). */
#define err(st, en) do { subtest = st; e(en); } while(0)

void do_calcs(void);
void do_child(void);
void do_parent(void);
void func1(int a, int b, int c, int d, int e, int f, int g, int h, int
	i, int j, int k, int l, int m, int n, int o, int p, int q, int r, int s,
	int t, int u, int v, int w, int x, int y, int z, int aa, int bb);
void func2(void);
void just_exit(void);
void test_brk(void);
void verify_main_reenter(void);

int max_error = 5;
#include "common.h"

#define SSIZE 32768
#define ROUNDS 10
#define SWAPS 10


int subtest;
ucontext_t ctx[3];
int entered_func1, entered_func2, reentered_main, entered_overflow;

static char st_stack[SSIZE];
static volatile int shift, global;

void do_calcs(void)
{
  float a, b, c, d, e;
  float foo, bar;
  int i;

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
  if(fabs(foo - (a * b)) > 0.0001) err(8, 1);
  if(fabs(bar - (c * d)) > 0.0001) err(8, 2); 
}

void do_child(void)
{
  int s;
  s = 1;

  /* Initialize FPU context and verify it's set to round to nearest. */
  if (fegetround() != FE_TONEAREST) err(9, 1);

  /* Now we change the rounding to something else, and this should be preserved
     between context swaps. */
  if (fesetround(FE_DOWNWARD) != 0) err(9, 2);

  while(s < SWAPS) {
	s++;
	if (swapcontext(&ctx[2], &ctx[1]) == -1) err(9, 2);
	do_calcs();
	if (fegetround() != FE_DOWNWARD) err(9, 4);
  }
  quit();
}

void do_parent(void)
{
  ucontext_t dummy;
  int s;
  s = 1;

  /* Initialize FPU context and verify it's set to round to nearest. */
  if (fegetround() != FE_TONEAREST) err(10, 1);

  /* Now we change the rounding to something else, and this should be preserved
     between context swaps. */
  if (fesetround(FE_UPWARD) != 0) err(10, 2);

  /* Quick check to make sure that getcontext does not reset the FPU state. */
  getcontext(&dummy);

  if (fegetround() != FE_UPWARD) err(10, 3);

  while(s < SWAPS) {
	do_calcs();
	if (fegetround() != FE_UPWARD) err(10, 4);
	s++;
	if (swapcontext(&ctx[1], &ctx[2]) == -1) err(10, 5);
  }
  /* Returning to main thread through uc_link */
}

static void fail(void)
{
  /* Shouldn't get here */
  err(5, 1);
}

void func1(int a, int b, int c, int d, int e, int f, int g,
	   int h, int i, int j, int k, int l, int m, int n,
	   int o, int p, int q, int r, int s, int t, int u,
	   int v, int w, int x, int y, int z, int aa, int bb)
{
  if ( a != (0x0000001 << shift) ||  b != (0x0000004 << shift) ||
       c != (0x0000010 << shift) ||  d != (0x0000040 << shift) ||
       e != (0x0000100 << shift) ||  f != (0x0000400 << shift) ||
       g != (0x0001000 << shift) ||  h != (0x0004000 << shift) ||
       i != (0x0010000 << shift) ||  j != (0x0040000 << shift) ||
       k != (0x0100000 << shift) ||  l != (0x0400000 << shift) ||
       m != (0x1000000 << shift) ||  n != (0x4000000 << shift) ||
       o != (0x0000002 << shift) ||  p != (0x0000008 << shift) ||
       q != (0x0000020 << shift) ||  r != (0x0000080 << shift) ||
       s != (0x0000200 << shift) ||  t != (0x0000800 << shift) ||
       u != (0x0002000 << shift) ||  v != (0x0008000 << shift) ||
       w != (0x0020000 << shift) ||  x != (0x0080000 << shift) ||
       y != (0x0200000 << shift) ||  z != (0x0800000 << shift) ||
      aa != (0x2000000 << shift) || bb != (0x8000000 << shift) ) {
	err(2, 1);
  } 

  if (shift && swapcontext (&ctx[1], &ctx[2]) != 0) err(2, 2);
  shift++;
  entered_func1++;
}

void func2(void)
{
  if (swapcontext(&ctx[2], &ctx[1]) != 0) err(3, 1);
  entered_func2++;
}

void just_exit(void)
{
  if (errct == 0) printf("ok\n");
  _exit(1);
}

void test_brk(void)
{
  char *big_stack;

  big_stack = malloc(16 * 1024 * 1024); /* 16 MB */
  /* If this fails, it is likely brk system call failed due stack/data segments
     collision detection. Unless the system really is low on memory, this is an
     error. */
  if (big_stack == NULL) err(7, 1);
}

void verify_main_reenter(void)
{
  if (reentered_main == 0) err(4, 1);
}

int set_context_test_value;
void set_context_test_thread1(void){
	set_context_test_value |= 0x1;
	setcontext(&ctx[2]);
	err(1, 24);

}

void set_context_test_thread2(void){
	set_context_test_value |= 0x1 << 1;
	setcontext(&ctx[0]);
	err(1, 23);
}

int main(void)
{
  start(51);

  atexit(verify_main_reenter);

  /* Save current context in ctx[0] */
  if (getcontext(&ctx[0]) != 0) {
	/* Don't verify reentering main, not going to happen */
	atexit(just_exit); 
	err(1, 1);
  }

  ctx[1] = ctx[0];
  ctx[1].uc_stack.ss_sp = st_stack;
  ctx[1].uc_stack.ss_size = SSIZE;
  ctx[1].uc_link = &ctx[0]; /* When done running, return here */

  /* ctx[1] is going to run func1 and then return here (uc_link). */
  /* We'll see later on whether makecontext worked. */
  makecontext(&ctx[1], (void (*) (void)) func1, 28,
  	      (0x0000001 << shift), (0x0000004 << shift), 
	      (0x0000010 << shift), (0x0000040 << shift),
	      (0x0000100 << shift), (0x0000400 << shift),
	      (0x0001000 << shift), (0x0004000 << shift),
	      (0x0010000 << shift), (0x0040000 << shift),
	      (0x0100000 << shift), (0x0400000 << shift),
	      (0x1000000 << shift), (0x4000000 << shift),
	      (0x0000002 << shift), (0x0000008 << shift),
	      (0x0000020 << shift), (0x0000080 << shift),
	      (0x0000200 << shift), (0x0000800 << shift),
	      (0x0002000 << shift), (0x0008000 << shift),
	      (0x0020000 << shift), (0x0080000 << shift),
	      (0x0200000 << shift), (0x0800000 << shift),
	      (0x2000000 << shift), (0x8000000 << shift));

  if (++global == 1) {
	/* First time we're here. Let's run ctx[1] and return to ctx[0] when
	 * we're done. Note that we return to above the 'makecontext' call. */
	if (setcontext(&ctx[1]) != 0) err(1, 2);
  }
  if (global != 2) {
	/* When ++global was 1 we let ctx[1] run and returned to ctx[0], so
	   above ++global is executed again and should've become 2. */
	err(1, 3);
  }

  /* Setup ctx[2] to run func2 */
  if (getcontext(&ctx[2]) != 0) err(1, 4);
  ctx[2].uc_stack.ss_sp = malloc(SSIZE);
  ctx[2].uc_stack.ss_size = SSIZE;
  ctx[2].uc_link = &ctx[1];
  makecontext(&ctx[2], (void (*) (void)) func2, 0);

  /* Now things become tricky. ctx[2] is set up such that when it finishes 
     running, and starts ctx[1] again. However, func1 swaps back to func2. Then,
     when func2 has finished running, we continue with ctx[1] and, finally, we
     return to ctx[0]. */
  if (swapcontext(&ctx[0], &ctx[2]) != 0) err(1, 5); /* makecontext failed? */
  reentered_main = 1;

  /* The call graph is as follows:
   *
   *                       ########
   *             /--------># main #
   *             7    /----########----\
   *             |    |    ^           |
   *             |    1    2           3
   *             |    V    |           V
   *          #########----/           #########
   *          # func1 #<-------4-------# func2 #
   *          #########--------5------>#########
   *                 ^                  |
   *                 |                  |
   *                 \---------6--------/
   *
   * Main calls func1, func1 increases entered_func1, and returns to main. Main
   * calls func2, swaps to func1, swaps to func2, which increases entered_func2,
   * continues with func1, which increases entered_func1 again, continues to
   * main, where reentered_main is set to 1. In effect, entered_func1 == 2,
   * entered_func2 == 1, reentered_main == 1. Verify that. */

  if (entered_func1 != 2) err(1, 6);
  if (entered_func2 != 1) err(1, 7);
  /* reentered_main == 1 is verified upon exit */
  
  /* Try to allocate too small a stack */
  free(ctx[2].uc_stack.ss_sp); /* Deallocate stack space first */
  if (getcontext(&ctx[2]) != 0) err(1, 8);
  ctx[2].uc_stack.ss_sp = malloc(MINSIGSTKSZ-1);
  ctx[2].uc_stack.ss_size = MINSIGSTKSZ-1;
  ctx[2].uc_link = &ctx[0];
  makecontext(&ctx[2], (void (*) (void)) fail, 0);
  /* Because makecontext is void, we can only detect an error by trying to use
     the invalid context */
  if (swapcontext(&ctx[0], &ctx[2]) == 0) err(1, 9);

  /* Try to allocate a huge stack to force the usage of brk/sbrk system call
     to enlarge the data segment. Because we are fiddling with the stack
     pointer, the OS might think the stack segment and data segment have
     collided and kill us. This is wrong and therefore the following should
     work. */
  free(ctx[2].uc_stack.ss_sp); /* Deallocate stack space first */
  if (getcontext(&ctx[2]) != 0) err(1, 14);
  ctx[2].uc_stack.ss_sp = malloc(8 * 1024 * 1024); /* 8 MB */
  ctx[2].uc_stack.ss_size = 8 * 1024 * 1024;
  ctx[2].uc_link = &ctx[0];
  makecontext(&ctx[2], (void (*) (void)) test_brk, 0);
  if (swapcontext(&ctx[0], &ctx[2]) != 0) err(1, 15);

  ctx[1].uc_link = &ctx[0];
  ctx[2].uc_link = NULL;
  makecontext(&ctx[1], (void (*) (void)) do_parent, 0);
  makecontext(&ctx[2], (void (*) (void)) do_child, 0);
  if (swapcontext(&ctx[0], &ctx[2]) == -1) err(1, 16);

  /* test setcontext  first do some cleanup */

  free(ctx[1].uc_stack.ss_sp);
  free(ctx[2].uc_stack.ss_sp);

  memset(&ctx[0],0,sizeof(ucontext_t));
  memset(&ctx[1],0,sizeof(ucontext_t));
  memset(&ctx[2],0,sizeof(ucontext_t));


  /* create 3 new contexts */
  volatile int cb =1;

  /* control will be returned here */
  set_context_test_value  = 0;
  if (getcontext(&ctx[0]) != 0) err(1, 17);
  if (set_context_test_value != 0x3) err(1, 20);
  if (cb == 1) {
    cb =0;
    if (getcontext(&ctx[1]) != 0) err(1, 18);
    if (getcontext(&ctx[2]) != 0) err(1, 19);

    // allocate new stacks */
    ctx[1].uc_stack.ss_sp = malloc(SSIZE);
    ctx[1].uc_stack.ss_size = SSIZE;
    ctx[2].uc_stack.ss_sp = malloc(SSIZE);
    ctx[2].uc_stack.ss_size = SSIZE;

    makecontext(&ctx[1],set_context_test_thread1,0);
    makecontext(&ctx[2],set_context_test_thread2,0);
    if( setcontext(&ctx[1]) != 0){
      err(1, 21);
    }
    err(1, 22);
  }

  quit();
  return(-1);
}

