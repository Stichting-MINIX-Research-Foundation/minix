/* Test the mthreads library. When the library is compiled with -DMDEBUG, you
 * have to compile this test with -DMDEBUG as well or it won't link. MDEBUG
 * lets you check the internal integrity of the library. */
#include <stdio.h>
#include <minix/mthread.h>

#define thread_t mthread_thread_t
#define mutex_t mthread_mutex_t
#define cond_t mthread_cond_t
#define once_t mthread_once_t
#define attr_t mthread_attr_t

#define MAX_ERROR 5
#include "common.c"

PUBLIC int errct;
PRIVATE int count, condition_met;
PRIVATE int th_a, th_b, th_c, th_d, th_e, th_f, th_g, th_h;
PRIVATE int mutex_a_step, mutex_b_step, mutex_c_step;
PRIVATE mutex_t mu[3];
PRIVATE cond_t condition;
PRIVATE mutex_t *count_mutex, *condition_mutex;
PRIVATE once_t once;
#define VERIFY_MUTEX(a,b,c,esub,eno)	do { \
	if (mutex_a_step != a) { \
		printf("Expected %d %d %d, got: %d %d %d\n", \
			a, b, c, mutex_a_step, mutex_b_step, mutex_c_step); \
		err(esub, eno); \
	} else if (mutex_b_step != b) err(esub, eno); \
	else if (mutex_c_step != c) err(esub, eno); \
					} while(0)
#define ROUNDS 14
#define THRESH1 3
#define THRESH2 8
#define MEG 1024*1024
#define MAGIC 0xb4a3f1c2

FORWARD _PROTOTYPE( void thread_a, (void *arg)				);
FORWARD _PROTOTYPE( void thread_b, (void *arg)				);
FORWARD _PROTOTYPE( void thread_c, (void *arg)				);
FORWARD _PROTOTYPE( void thread_d, (void *arg)				);
FORWARD _PROTOTYPE( void thread_e, (void)				);
FORWARD _PROTOTYPE( void thread_f, (void *arg)				);
FORWARD _PROTOTYPE( void thread_g, (void *arg)				);
FORWARD _PROTOTYPE( void thread_h, (void *arg)				);
FORWARD _PROTOTYPE( void test_scheduling, (void)			);
FORWARD _PROTOTYPE( void test_mutex, (void)				);
FORWARD _PROTOTYPE( void test_condition, (void)				);
FORWARD _PROTOTYPE( void test_attributes, (void)			);
FORWARD _PROTOTYPE( void err, (int subtest, int error)			);

/*===========================================================================*
 *				thread_a				     *
 *===========================================================================*/
PRIVATE void thread_a(void *arg) {
  th_a++;
}


/*===========================================================================*
 *				thread_b				     *
 *===========================================================================*/
PRIVATE void thread_b(void *arg) {
  th_b++;
  if (mthread_once(&once, thread_e) != 0) err(10, 1);
}


/*===========================================================================*
 *				thread_c				     *
 *===========================================================================*/
PRIVATE void thread_c(void *arg) {
  th_c++;
}


/*===========================================================================*
 *				thread_d				     *
 *===========================================================================*/
PRIVATE void thread_d(void *arg) {
  th_d++;
  mthread_exit(NULL); /* Thread wants to stop running */
}


/*===========================================================================*
 *				thread_e				     *
 *===========================================================================*/
PRIVATE void thread_e(void) {
  th_e++;
}


/*===========================================================================*
 *				thread_f				     *
 *===========================================================================*/
PRIVATE void thread_f(void *arg) {
  if (mthread_mutex_lock(condition_mutex) != 0) err(12, 1);
  th_f++;
  if (mthread_cond_signal(&condition) != 0) err(12, 2);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(12, 3);
}


/*===========================================================================*
 *				thread_g				     *
 *===========================================================================*/
PRIVATE void thread_g(void *arg) {
  char bigarray[MTHREAD_STACK_MIN + 1];
  if (mthread_mutex_lock(condition_mutex) != 0) err(13, 1);
  memset(bigarray, '\0', MTHREAD_STACK_MIN + 1); /* Actually allocate it */
  th_g++;
  if (mthread_cond_signal(&condition) != 0) err(13, 2);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(13, 3);
}


/*===========================================================================*
 *				thread_h				     *
 *===========================================================================*/
PRIVATE void thread_h(void *arg) {
  char bigarray[2 * MEG];
  int reply;
  if (mthread_mutex_lock(condition_mutex) != 0) err(14, 1);
  memset(bigarray, '\0', 2 * MEG); /* Actually allocate it */
  th_h++;
  if (mthread_cond_signal(&condition) != 0) err(14, 2);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(14, 3);
  reply = *((int *) arg); 
  mthread_exit((void *) reply);
}


/*===========================================================================*
 *				err					     *
 *===========================================================================*/
PRIVATE void err(int sub, int error) {
  /* As we're running with multiple threads, they might all clobber the
   * subtest variable. This wrapper prevents that from happening. */

  subtest = sub;
  e(error);
}


/*===========================================================================*
 *				test_scheduling				     *
 *===========================================================================*/
PRIVATE void test_scheduling(void)
{
  int i;
  thread_t t[7];

#ifdef MDEBUG
  mthread_verify();
#endif
  th_a = th_b = th_c = th_d = th_e = 0;

  if (mthread_create(&t[0], NULL, thread_a, NULL) != 0) err(1, 1);
  if (mthread_create(&t[1], NULL, thread_a, NULL) != 0) err(1, 2);
  if (mthread_create(&t[2], NULL, thread_a, NULL) != 0) err(1, 3);
  if (mthread_create(&t[3], NULL, thread_d, NULL) != 0) err(1, 4);
  if (mthread_once(&once, thread_e) != 0) err(1, 5);

  mthread_yield();

  if (mthread_create(&t[4], NULL, thread_c, NULL) != 0) err(1, 6);
  mthread_yield();
  if (mthread_create(&t[5], NULL, thread_b, NULL) != 0) err(1, 7);
  if (mthread_create(&t[6], NULL, thread_a, NULL) != 0) err(1, 8);
  mthread_yield();
  mthread_yield();
  if (mthread_once(&once, thread_e) != 0) err(1, 9);
  if (mthread_once(&once, thread_e) != 0) err(1, 10);

  if (th_a != 4) err(1, 11);
  if (th_b != 1) err(1, 12);
  if (th_c != 1) err(1, 13);
  if (th_d != 1) err(1, 14);
  if (th_e != 1) err(1, 15);

  for (i = 0; i < (sizeof(t) / sizeof(thread_t)); i++) {
	if (mthread_join(t[i], NULL) != 0) err(1, 16);
	if (mthread_join(t[i], NULL) == 0) err(1, 17); /*Shouldn't work twice*/
  }

#ifdef MDEBUG
  mthread_verify();
#endif
  if (mthread_create(NULL, NULL, NULL, NULL) == 0) err(1, 18);
  mthread_yield();

#ifdef MDEBUG
  mthread_verify();
#endif
  if (mthread_create(&t[6], NULL, NULL, NULL) == 0) err(1, 19);
  mthread_yield();
#ifdef MDEBUG
  mthread_verify();
#endif
  if (mthread_join(0xc0ffee, NULL) == 0) err(1, 20);
  mthread_yield();
  mthread_yield();

#ifdef MDEBUG
  mthread_verify();
#endif
}


/*===========================================================================*
 *				mutex_a					     *
 *===========================================================================*/
PRIVATE void mutex_a(void *arg)
{
  mutex_t *mu = (mutex_t *) arg;
  mutex_t mu2;

  VERIFY_MUTEX(0, 0, 0, 3, 1);
  if (mthread_mutex_lock(&mu[0]) != 0) err(3, 2);

  /* Trying to acquire lock again should fail with EDEADLK */
  if (mthread_mutex_lock(&mu[0]) != -1) err(3, 2);
  if (errno != EDEADLK) err(3, 3);

  /* Try to acquire lock on uninitialized mutex; should fail with EINVAL */
  if (mthread_mutex_lock(&mu2) != -1) {
  	err(3, 4);
  	mthread_mutex_unlock(&mu2);
  }
  if (errno != EINVAL) err(3, 5); 
  errno = 0;
  if (mthread_mutex_trylock(&mu2) != -1) {
  	err(3, 6);
  	mthread_mutex_unlock(&mu2);
  }
  if (errno != EINVAL) err(3, 7); 

  if (mthread_mutex_trylock(&mu[1]) != 0) err(3, 8);
  mutex_a_step = 1;
  mthread_yield();
  VERIFY_MUTEX(1, 0, 0, 3, 9);
  errno = 0;
  if (mthread_mutex_trylock(&mu[2]) != -1) err(3, 10);
  if (errno != EBUSY) err(3, 11);
  if (mthread_mutex_lock(&mu[2]) != 0) err(3, 12); /* Transfer control to main
  						    * loop.
  						    */
  VERIFY_MUTEX(1, 0, 0, 3, 13);

  if (mthread_mutex_unlock(&mu[0]) != 0) err(3, 14);
  mutex_a_step = 2;
  mthread_yield();

  VERIFY_MUTEX(2, 1, 0, 3, 15);
  if (mthread_mutex_unlock(&mu[1]) != 0) err(3, 16);
  mutex_a_step = 3;

  /* Try with faulty memory locations */
  if (mthread_mutex_lock(NULL) == 0) err(3, 17);
  if (mthread_mutex_trylock(NULL) == 0) err(3, 18);
  if (mthread_mutex_unlock(NULL) == 0) err(3, 19);

  if (mthread_mutex_unlock(&mu[2]) != 0) err(3, 20);
}


/*===========================================================================*
 *				mutex_b					     *
 *===========================================================================*/
PRIVATE void mutex_b(void *arg)
{
  mutex_t *mu = (mutex_t *) arg;

  /* At this point mutex_a thread should have acquired a lock on mu[0]. We
   * should not be able to unlock it on behalf of that thread.
   */

  VERIFY_MUTEX(1, 0, 0, 4, 1);
  if (mthread_mutex_unlock(&mu[0]) != -1) err(4, 2);
  if (errno != EPERM) err(4, 3);

  /* Probing mu[0] to lock it should tell us it's locked */
  if (mthread_mutex_trylock(&mu[0]) == 0) err(4, 4);
  if (errno != EBUSY) err(4, 5);

  if (mthread_mutex_lock(&mu[0]) != 0) err(4, 5);
  mutex_b_step = 1;
  VERIFY_MUTEX(2, 1, 0, 4, 6);
  if (mthread_mutex_lock(&mu[1]) != 0) err(4, 6);
  mutex_b_step = 2;
  VERIFY_MUTEX(3, 2, 2, 4, 7);
  mthread_yield();
  VERIFY_MUTEX(3, 2, 2, 4, 8);

  if (mthread_mutex_unlock(&mu[0]) != 0) err(4, 7);
  mutex_b_step = 3;
  mthread_yield();

  if (mthread_mutex_unlock(&mu[1]) != 0) err(4, 8);
  mutex_b_step = 4;
}


/*===========================================================================*
 *				mutex_c					     *
 *===========================================================================*/
PRIVATE void mutex_c(void *arg)
{
  mutex_t *mu = (mutex_t *) arg;

  VERIFY_MUTEX(1, 0, 0, 5, 1);
  if (mthread_mutex_lock(&mu[1]) != 0) err(5, 2);
  mutex_c_step = 1;
  VERIFY_MUTEX(3, 1, 1, 5, 3);
  mthread_yield();
  VERIFY_MUTEX(3, 1, 1, 5, 4);

  if (mthread_mutex_unlock(&mu[1]) != 0) err(5, 5);
  mutex_c_step = 2;
  if (mthread_mutex_lock(&mu[0]) != 0) err(5, 6);
  mutex_c_step = 3;
  VERIFY_MUTEX(3, 3, 3, 5, 7);
  mthread_yield();
  VERIFY_MUTEX(3, 4, 3, 5, 8);

  if (mthread_mutex_unlock(&mu[0]) != 0) err(5, 9);
  mutex_c_step = 4;
}


/*===========================================================================*
 *				test_mutex				     *
 *===========================================================================*/
PRIVATE void test_mutex(void)
{
  int i;
  thread_t t[3];
#ifdef MDEBUG
  mthread_verify();
#endif
  if (mthread_mutex_init(&mu[0], NULL) != 0) err(2, 1);
  if (mthread_mutex_init(&mu[1], NULL) != 0) err(2, 2);
  if (mthread_mutex_init(&mu[2], NULL) != 0) err(2, 3);

  if (mthread_create(&t[0], NULL, mutex_a, (void *) mu) != 0) err(2, 3);
  if (mthread_create(&t[1], NULL, mutex_b, (void *) mu) != 0) err(2, 4);
  if (mthread_create(&t[2], NULL, mutex_c, (void *) mu) != 0) err(2, 5);

  if (mthread_mutex_lock(&mu[2]) != 0) err(2, 6);

  mthread_yield_all(); /* Should result in a RUNNABLE mutex_a, and a blocked
  			* on mutex mutex_b and mutex_c.
  			*/ 

  VERIFY_MUTEX(1, 0, 0, 2, 7); /* err(2, 7) */
  if (mthread_mutex_unlock(&mu[2]) != 0) err(2, 8);

  mthread_yield();	/* Should schedule mutex_a to release the lock on the
			 * mu[0] mutex. Consequently allowing mutex_b and mutex_c
  			* to acquire locks on the mutexes and exit.
  			*/
  VERIFY_MUTEX(2, 0, 0, 2, 9);

  for (i = 0; i < (sizeof(t) / sizeof(thread_t)); i++) 
	if (mthread_join(t[i], NULL) != 0) err(2, 10);

  if (mthread_mutex_destroy(&mu[0]) != 0) err(2, 11);
  if (mthread_mutex_destroy(&mu[1]) != 0) err(2, 12);
  if (mthread_mutex_destroy(&mu[2]) != 0) err(2, 13);

#ifdef MDEBUG
  mthread_verify();
#endif
}


/*===========================================================================*
 *					cond_a				     *
 *===========================================================================*/
PRIVATE void cond_a(void *arg)
{
  cond_t c;
  int did_count = 0;
  while(1) {
  	if (mthread_mutex_lock(condition_mutex) != 0) err(6, 1);
  	while (count >= THRESH1 && count <= THRESH2) {
  		if (mthread_cond_wait(&condition, condition_mutex) != 0)
  			err(6, 2);
  	}
  	if (mthread_mutex_unlock(condition_mutex) != 0) err(6, 3);

	mthread_yield(); 

  	if (mthread_mutex_lock(count_mutex) != 0) err(6, 4);
  	count++;
  	did_count++;
  	if (mthread_mutex_unlock(count_mutex) != 0) err(6, 5);

  	if (count >= ROUNDS) break;
  }
  if (!(did_count <= count - (THRESH2 - THRESH1 + 1))) err(6, 6);

  /* Try faulty addresses */
  if (mthread_mutex_lock(condition_mutex) != 0) err(6, 7);
  /* Condition c is not initialized, so whatever we do with it should fail. */
  if (mthread_cond_wait(&c, condition_mutex) == 0) err(6, 8);
  if (mthread_cond_wait(NULL, condition_mutex) == 0) err(6, 9);
  if (mthread_cond_signal(&c) == 0) err(6, 10);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(6, 11);

  /* Try again with an unlocked mutex */
  if (mthread_cond_wait(&c, condition_mutex) == 0) err(6, 12);
  if (mthread_cond_signal(&c) == 0) err(6, 13);

  /* And again with an unlocked mutex, but initialized c */
  if (mthread_cond_init(&c, NULL) != 0) err(6, 14);
  if (mthread_cond_wait(&c, condition_mutex) == 0) err(6, 15);
  if (mthread_cond_signal(&c) != 0) err(6, 16);/*c.f., 6.10 this should work!*/
  if (mthread_cond_destroy(&c) != 0) err(6, 17);
}


/*===========================================================================*
 *					cond_b				     *
 *===========================================================================*/
PRIVATE void cond_b(void *arg)
{
  int did_count = 0;
  while(1) {
  	if (mthread_mutex_lock(condition_mutex) != 0) err(7, 1);
  	if (count < THRESH1 || count > THRESH2) 
  		if (mthread_cond_signal(&condition) != 0) err(7, 2);
  	if (mthread_mutex_unlock(condition_mutex) != 0) err(7, 3);

	mthread_yield();

  	if (mthread_mutex_lock(count_mutex) != 0) err(7, 4);
  	count++;
  	did_count++;
  	if (mthread_mutex_unlock(count_mutex) != 0) err(7, 5);

  	if (count >= ROUNDS) break;
  }

  if (!(did_count >= count - (THRESH2 - THRESH1 + 1))) err(7, 6);

}

/*===========================================================================*
 *				cond_broadcast				     *
 *===========================================================================*/
PRIVATE void cond_broadcast(void *arg)
{
  int rounds = 0;
  if (mthread_mutex_lock(condition_mutex) != 0) err(9, 1);

  while(!condition_met) 
  	if (mthread_cond_wait(&condition, condition_mutex) != 0) err(9, 2);

  if (mthread_mutex_unlock(condition_mutex) != 0) err(9, 3);

  if (mthread_mutex_lock(count_mutex) != 0) err(9, 4);
  count++;
  if (mthread_mutex_unlock(count_mutex) != 0) err(9, 5);
}

/*===========================================================================*
 *				test_condition				     *
 *===========================================================================*/
PRIVATE void test_condition(void)
{
#define NTHREADS 10
  int i, r;
  thread_t t[2], s[NTHREADS];
  count_mutex = &mu[0];
  condition_mutex = &mu[1];

  /* Test simple condition variable behavior: Two threads increase a counter.
   * At some point one thread waits for a condition and the other thread
   * signals the condition. Consequently, one thread increased the counter a
   * few times less than other thread. Although the difference is 'random', 
   * there is a guaranteed minimum difference that we can measure.
   */

#ifdef MDEBUG
  mthread_verify();
#endif

  if (mthread_mutex_init(count_mutex, NULL) != 0) err(8, 1);
  if (mthread_mutex_init(condition_mutex, NULL) != 0) err(8, 2);
  if (mthread_cond_init(&condition, NULL) != 0) err(8, 3);
  count = 0;

  if (mthread_create(&t[0], NULL, cond_a, NULL) != 0) err(8, 4);
  if (mthread_create(&t[1], NULL, cond_b, NULL) != 0) err(8, 5);

  for (i = 0; i < (sizeof(t) / sizeof(thread_t)); i++) 
	if (mthread_join(t[i], NULL) != 0) err(8, 6);

  if (mthread_mutex_destroy(count_mutex) != 0) err(8, 7);
  if (mthread_mutex_destroy(condition_mutex) != 0) err(8, 8);
  if (mthread_cond_destroy(&condition) != 0) err(8, 9);

  /* Let's try to destroy it again. Should fails as it's uninitialized. */
  if (mthread_cond_destroy(&condition) == 0) err(8, 10); 

#ifdef MDEBUG
  mthread_verify();
#endif

  /* Test signal broadcasting: spawn N threads that will increase a counter
   * after a condition has been signaled. The counter must equal N. */
  if (mthread_mutex_init(count_mutex, NULL) != 0) err(8, 11);
  if (mthread_mutex_init(condition_mutex, NULL) != 0) err(8, 12);
  if (mthread_cond_init(&condition, NULL) != 0) err(8, 13);
  condition_met = count = 0;

  for (i = 0; i < NTHREADS; i++) 
	if (mthread_create(&s[i], NULL, cond_broadcast, NULL) != 0) err(8, 14);

  /* Allow other threads to block on the condition variable. If we don't yield,
   * the threads will only start running when we call mthread_join below. In
   * that case the while loop in cond_broadcast will never evaluate to true.
   */
  mthread_yield();

  if (mthread_mutex_lock(condition_mutex) != 0) err(8, 15);
  condition_met = 1;
  if (mthread_cond_broadcast(&condition) != 0) err(8, 16);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(8, 17);

  for (i = 0; i < (sizeof(s) / sizeof(thread_t)); i++) 
	if (mthread_join(s[i], NULL) != 0) err(8, 18);

  if (count != NTHREADS) err(8, 19);
  if (mthread_mutex_destroy(count_mutex) != 0) err(8, 20);
  if (mthread_mutex_destroy(condition_mutex) != 0) err(8, 21);
  if (mthread_cond_destroy(&condition) != 0) err(8, 22); 

  /* Again, destroying the condition variable twice shouldn't work */
  if (mthread_cond_destroy(&condition) == 0) err(8, 23); 

#ifdef MDEBUG
  mthread_verify();
#endif
}

/*===========================================================================*
 *				test_attributes				     *
 *===========================================================================*/
PRIVATE void test_attributes(void)
{
  attr_t tattr;
  thread_t tid;
  int detachstate = -1, status = 0;
  int i, no_ints, stack_untouched = 1;
  void *stackaddr, *newstackaddr;
  int *stackp;
  size_t stacksize, newstacksize;

#ifdef MDEBUG
  mthread_verify();
#endif

  /* Initialize thread attribute and try to read the default values */
  if (mthread_attr_init(&tattr) != 0) err(11, 1);
  if (mthread_attr_getdetachstate(&tattr, &detachstate) != 0) err(11, 2);
  if (detachstate != MTHREAD_CREATE_JOINABLE) err(11, 3);
  if (mthread_attr_getstack(&tattr, &stackaddr, &stacksize) != 0) err(11, 4);
  if (stackaddr != NULL) err(11, 5);
  if (stacksize != (size_t) 0) err(11, 6);

  /* Modify the attribute ... */
  /* Try bogus detach state value */
  if (mthread_attr_setdetachstate(&tattr, 0xc0ffee) == 0) err(11, 7);
  if (mthread_attr_setdetachstate(&tattr, MTHREAD_CREATE_DETACHED) != 0)
  	err(11, 8);
  newstacksize = (size_t) MEG;
  if ((newstackaddr = malloc(newstacksize)) == NULL) err(11, 9);
  if (mthread_attr_setstack(&tattr, newstackaddr, newstacksize) != 0)
  	err(11, 10);
   /* ... and read back the new values. */
  if (mthread_attr_getdetachstate(&tattr, &detachstate) != 0) err(11, 11);
  if (detachstate != MTHREAD_CREATE_DETACHED) err(11, 12);
  if (mthread_attr_getstack(&tattr, &stackaddr, &stacksize) != 0) err(11, 13);
  if (stackaddr != newstackaddr) err(11, 14);
  if (stacksize != newstacksize) err(11, 15);
  if (mthread_attr_destroy(&tattr) != 0) err(11, 16);
  /* Freeing the stack. Note that this is only possible because it wasn't
   * actually used yet by a thread. If it was, mthread would clean it up after
   * usage and this free would do something undefined. */
  free(newstackaddr);

  /* Try to allocate too small a stack; it should fail and the attribute 
   * values should remain as is.
   */
  newstacksize = MTHREAD_STACK_MIN - 1;
  stackaddr = NULL;
  stacksize = 0;
  if (mthread_attr_init(&tattr) != 0) err(11, 17);
  if ((newstackaddr = malloc(newstacksize)) == NULL) err(11, 18);
  if (mthread_attr_setstack(&tattr, newstackaddr, newstacksize) == 0)
  	err(11, 19);
  if (errno != EINVAL) err(11, 20);
  if (mthread_attr_getstack(&tattr, &stackaddr, &stacksize) != 0) err(11, 21);
  if (stackaddr == newstackaddr) err(11, 22);
  if (stacksize == newstacksize) err(11, 23);
  if (mthread_attr_destroy(&tattr) != 0) err(11, 24);
  /* Again, freeing because we can. Shouldn't do it if it was actually used. */
  free(newstackaddr);

  /* Tell attribute to let the system allocate a stack for the thread and only
   * dictate how big that stack should be (2 megabyte, not actually allocated
   * yet).
   */
  if (mthread_attr_init(&tattr) != 0) err(11, 25);
  if (mthread_attr_setstack(&tattr, NULL /* System allocated */, 2*MEG) != 0)
  	err(11, 26);
  if (mthread_attr_getstack(&tattr, &stackaddr, &stacksize) != 0) err(11, 27);
  if (stackaddr != NULL) err(11, 28);
  if (stacksize != 2*MEG) err(11, 29);

  /* Use set/getstacksize to set and retrieve new stack sizes */
  stacksize = 0;
  if (mthread_attr_getstacksize(&tattr, &stacksize) != 0) err(11, 30);
  if (stacksize != 2*MEG) err(11, 31);
  newstacksize = MEG;
  if (mthread_attr_setstacksize(&tattr, newstacksize) != 0) err(11, 32);
  if (mthread_attr_getstacksize(&tattr, &stacksize) != 0) err(11, 33);
  if (stacksize != newstacksize) err(11, 34);
  if (mthread_attr_destroy(&tattr) != 0) err(11, 35);

  /* Perform same tests, but also actually use them in a thread */
  if (mthread_attr_init(&tattr) != 0) err(11, 36);
  if (mthread_attr_setdetachstate(&tattr, MTHREAD_CREATE_DETACHED) != 0)
  	err(11, 37);
  condition_mutex = &mu[0];
  if (mthread_mutex_init(condition_mutex, NULL) != 0) err(11, 38);
  if (mthread_cond_init(&condition, NULL) != 0) err(11, 39);
  if (mthread_mutex_lock(condition_mutex) != 0) err(11, 40);
  if (mthread_create(&tid, &tattr, thread_f, NULL) != 0) err(11, 41);
  /* Wait for thread_f to finish */
  if (mthread_cond_wait(&condition, condition_mutex) != 0) err(11, 42);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(11, 43);
  if (th_f != 1) err(11, 44);
  /* Joining a detached thread should fail */
  if (mthread_join(tid, NULL) == 0) err(11, 45);
  if (mthread_attr_destroy(&tattr) != 0) err(11, 46);

  /* Try telling the attribute how large the stack should be */
  if (mthread_attr_init(&tattr) != 0) err(11, 47);
  if (mthread_attr_setstack(&tattr, NULL, 2 * MTHREAD_STACK_MIN) != 0)
  	err(11, 48);
  if (mthread_mutex_lock(condition_mutex) != 0) err(11, 49);
  if (mthread_create(&tid, &tattr, thread_g, NULL) != 0) err(11, 50);
  /* Wait for thread_g to finish */
  if (mthread_cond_wait(&condition, condition_mutex) != 0) err(11, 51);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(11, 52);
  if (th_g != 1) err(11, 53);
  if (mthread_attr_setdetachstate(&tattr, MTHREAD_CREATE_DETACHED) != 0)
  	err(11, 54); /* Shouldn't affect the join below, as thread is already
  		      * running as joinable. If this attribute should be 
  		      * modified after thread creation, use mthread_detach().
  		      */
  if (mthread_join(tid, NULL) != 0) err(11, 55);
  if (mthread_attr_destroy(&tattr) != 0) err(11, 56);

  /* Try telling the attribute how large the stack should be and where it is
   * located.
   */
  if (mthread_attr_init(&tattr) != 0) err(11, 57);
  stacksize = 3 * MEG;
  /* Make sure this test is meaningful. We have to verify that we actually
   * use a custom stack. So we're going to allocate an array on the stack in
   * thread_h that should at least be bigger than the default stack size
   * allocated by the system.
   */
  if (2 * MEG <= MTHREAD_STACK_MIN) err(11, 58);
  if ((stackaddr = malloc(stacksize)) == NULL) err(11, 59);
  /* Fill stack with pattern. We assume that the beginning of the stack
   * should be overwritten with something and that the end should remain
   * untouched. The thread will zero-fill around two-thirds of the stack with
   * zeroes, so we can check if that's true. 
   */
  stackp = stackaddr;
  no_ints = stacksize / sizeof(int);
  for (i = 0; i < no_ints ; i++) 
	stackp[i] = MAGIC;
  if (mthread_attr_setstack(&tattr, stackaddr, stacksize) != 0) err(11, 60);
  if (mthread_mutex_lock(condition_mutex) != 0) err(11, 61);
  if (mthread_create(&tid, &tattr, thread_h, (void *) &stacksize) != 0)	
  	err(11, 62);
  /* Wait for thread h to finish */
  if (mthread_cond_wait(&condition, condition_mutex) != 0) err(11, 63);
  if (th_h != 1) err(11, 64);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(11, 65);

  /* Verify stack hypothesis; we assume a stack is used from the top and grows
   * downwards. At this point the stack should still exist, because we haven't
   * 'joined' yet. After joining, the stack is cleaned up and this test becomes
   * useless. */
#if (_MINIX_CHIP == _CHIP_INTEL)
  if (stackp[0] != MAGIC) err(11, 66); /* End of the stack */
  for (i = no_ints - 1 - 16; i < no_ints; i++)
  	if (stackp[i] != MAGIC) stack_untouched = 0;
  if (stack_untouched) err(11, 67); /* Beginning of the stack */
  if (stackp[no_ints / 2] != 0) err(11, 68);/*Zero half way through the stack*/
#else
#error "Unsupported chip for this test"
#endif

  if (mthread_join(tid, (void *) &status) != 0) err(11, 69);
  if (status != stacksize) err(11, 70);
  if (mthread_attr_destroy(&tattr) != 0) err(11, 71); 
  if (mthread_mutex_destroy(condition_mutex) != 0) err(11, 72);
  if (mthread_cond_destroy(&condition) != 0) err(11, 73);

#ifdef MDEBUG
  mthread_verify();
#endif
}

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(void)
{
  errct = 0;
  th_a = th_b = th_c = th_d = th_e = th_f = th_g = th_h = 0;
  mutex_a_step = mutex_b_step = mutex_c_step = 0;
  once = MTHREAD_ONCE_INIT;

  start(59);
  mthread_init(); 
  test_scheduling();
  test_mutex();
  test_condition();
  test_attributes();
  quit();
}

