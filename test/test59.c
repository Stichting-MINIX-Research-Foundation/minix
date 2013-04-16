/* Test the mthreads library. When the library is compiled with -DMDEBUG, you
 * have to compile this test with -DMDEBUG as well or it won't link. MDEBUG
 * lets you check the internal integrity of the library. */
#include <stdio.h>
#include <minix/mthread.h>
#include <signal.h>

#define thread_t mthread_thread_t
#define mutex_t mthread_mutex_t
#define cond_t mthread_cond_t
#define once_t mthread_once_t
#define attr_t mthread_attr_t
#define key_t mthread_key_t
#define event_t mthread_event_t
#define rwlock_t mthread_rwlock_t

int max_error = 5;
#include "common.h"


static int count, condition_met;
static int th_a, th_b, th_c, th_d, th_e, th_f, th_g, th_h;
static int mutex_a_step, mutex_b_step, mutex_c_step;
static mutex_t mu[3];
static cond_t condition;
static mutex_t *count_mutex, *condition_mutex;
static once_t once;
static key_t key[MTHREAD_KEYS_MAX+1];
static int values[4];
static int first;
static event_t event;
static int event_a_step, event_b_step;
static rwlock_t rwlock;
static int rwlock_a_step, rwlock_b_step;

#define VERIFY_RWLOCK(a, b, esub, eno) \
	GEN_VERIFY(rwlock_a_step, a, rwlock_b_step, b, esub, eno)

#define VERIFY_EVENT(a, b, esub, eno) \
	GEN_VERIFY(event_a_step, a, event_b_step, b, esub, eno)

#define GEN_VERIFY(acta, a, actb, b, esub, eno)	do { \
	if (acta != a) { \
		printf("Expected %d %d, got: %d %d\n", \
			a, b, acta, actb); \
		err(esub, eno); \
	} else if (actb != b) err(esub, eno); \
					} while(0)

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
#define MAGIC ((signed) 0xb4a3f1c2)

static void destr_a(void *arg);
static void destr_b(void *arg);
static void *thread_a(void *arg);
static void *thread_b(void *arg);
static void *thread_c(void *arg);
static void *thread_d(void *arg);
static void thread_e(void);
static void *thread_f(void *arg);
static void *thread_g(void *arg);
static void *thread_h(void *arg);
static void test_scheduling(void);
static void test_mutex(void);
static void test_condition(void);
static void test_attributes(void);
static void test_keys(void);
static void err(int subtest, int error);

/*===========================================================================*
 *				thread_a				     *
 *===========================================================================*/
static void *thread_a(void *arg) {
  th_a++;
  return(NULL);
}


/*===========================================================================*
 *				thread_b				     *
 *===========================================================================*/
static void *thread_b(void *arg) {
  th_b++;
  if (mthread_once(&once, thread_e) != 0) err(10, 1);
  return(NULL);
}


/*===========================================================================*
 *				thread_c				     *
 *===========================================================================*/
static void *thread_c(void *arg) {
  th_c++;
  return(NULL);
}


/*===========================================================================*
 *				thread_d				     *
 *===========================================================================*/
static void *thread_d(void *arg) {
  th_d++;
  mthread_exit(NULL); /* Thread wants to stop running */
  return(NULL);
}


/*===========================================================================*
 *				thread_e				     *
 *===========================================================================*/
static void thread_e(void) {
  th_e++;
}


/*===========================================================================*
 *				thread_f				     *
 *===========================================================================*/
static void *thread_f(void *arg) {
  if (mthread_mutex_lock(condition_mutex) != 0) err(12, 1);
  th_f++;
  if (mthread_cond_signal(&condition) != 0) err(12, 2);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(12, 3);
  return(NULL);
}


/*===========================================================================*
 *				thread_g				     *
 *===========================================================================*/
static void *thread_g(void *arg) {
  char bigarray[MTHREAD_STACK_MIN + 1];
  if (mthread_mutex_lock(condition_mutex) != 0) err(13, 1);
  memset(bigarray, '\0', MTHREAD_STACK_MIN + 1); /* Actually allocate it */
  th_g++;
  if (mthread_cond_signal(&condition) != 0) err(13, 2);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(13, 3);
  return(NULL);
}


/*===========================================================================*
 *				thread_h				     *
 *===========================================================================*/
static void *thread_h(void *arg) {
  char bigarray[2 * MEG];
  int reply;
  if (mthread_mutex_lock(condition_mutex) != 0) err(14, 1);
  memset(bigarray, '\0', 2 * MEG); /* Actually allocate it */
  th_h++;
  if (mthread_cond_signal(&condition) != 0) err(14, 2);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(14, 3);
  reply = *((int *) arg); 
  mthread_exit((void *) reply);
  return(NULL);
}


/*===========================================================================*
 *				err					     *
 *===========================================================================*/
static void err(int sub, int error) {
  /* As we're running with multiple threads, they might all clobber the
   * subtest variable. This wrapper prevents that from happening. */

  subtest = sub;
  e(error);
}


/*===========================================================================*
 *				test_scheduling				     *
 *===========================================================================*/
static void test_scheduling(void)
{
  unsigned int i;
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
static void *mutex_a(void *arg)
{
  mutex_t *mu = (mutex_t *) arg;

  VERIFY_MUTEX(0, 0, 0, 3, 1);
  if (mthread_mutex_lock(&mu[0]) != 0) err(3, 2);

  /* Trying to acquire lock again should fail with EDEADLK */
  if (mthread_mutex_lock(&mu[0]) != EDEADLK) err(3, 2);
  
#ifdef MTHREAD_STRICT 
  /* Try to acquire lock on uninitialized mutex; should fail with EINVAL */
  /* Note: this check only works when libmthread is compiled with
   * MTHREAD_STRICT turned on. In POSIX this situation is a MAY fail if... */
  if (mthread_mutex_lock(&mu2) != EINVAL) {
  	err(3, 4);
  	mthread_mutex_unlock(&mu2);
  }

  if (mthread_mutex_trylock(&mu2) != EINVAL) {
  	err(3, 6);
  	mthread_mutex_unlock(&mu2);
  }
#endif

  if (mthread_mutex_trylock(&mu[1]) != 0) err(3, 8);
  mutex_a_step = 1;
  mthread_yield();
  VERIFY_MUTEX(1, 0, 0, 3, 9);
  if (mthread_mutex_trylock(&mu[2]) != EBUSY) err(3, 10);
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
  return(NULL);
}


/*===========================================================================*
 *				mutex_b					     *
 *===========================================================================*/
static void *mutex_b(void *arg)
{
  mutex_t *mu = (mutex_t *) arg;

  /* At this point mutex_a thread should have acquired a lock on mu[0]. We
   * should not be able to unlock it on behalf of that thread.
   */

  VERIFY_MUTEX(1, 0, 0, 4, 1);
  if (mthread_mutex_unlock(&mu[0]) != EPERM) err(4, 2);

  /* Probing mu[0] to lock it should tell us it's locked */
  if (mthread_mutex_trylock(&mu[0]) != EBUSY) err(4, 4);

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
  return(NULL);
}


/*===========================================================================*
 *				mutex_c					     *
 *===========================================================================*/
static void *mutex_c(void *arg)
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
  return(NULL);
}


/*===========================================================================*
 *				test_mutex				     *
 *===========================================================================*/
static void test_mutex(void)
{
  unsigned int i;
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
static void *cond_a(void *arg)
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
#ifdef MTHREAD_STRICT
  /* Condition c is not initialized, so whatever we do with it should fail. */
  if (mthread_cond_wait(&c, condition_mutex) == 0) err(6, 8);
  if (mthread_cond_wait(NULL, condition_mutex) == 0) err(6, 9);
  if (mthread_cond_signal(&c) == 0) err(6, 10);
  if (mthread_mutex_unlock(condition_mutex) != 0) err(6, 11);

  /* Try again with an unlocked mutex */
  if (mthread_cond_wait(&c, condition_mutex) == 0) err(6, 12);
  if (mthread_cond_signal(&c) == 0) err(6, 13);
#endif

  /* And again with an unlocked mutex, but initialized c */
  if (mthread_cond_init(&c, NULL) != 0) err(6, 14);
  if (mthread_cond_wait(&c, condition_mutex) == 0) err(6, 15);
  if (mthread_cond_signal(&c) != 0) err(6, 16);/*c.f., 6.10 this should work!*/
  if (mthread_cond_destroy(&c) != 0) err(6, 17);
  return(NULL);
}


/*===========================================================================*
 *					cond_b				     *
 *===========================================================================*/
static void *cond_b(void *arg)
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

  return(NULL);
}

/*===========================================================================*
 *				cond_broadcast				     *
 *===========================================================================*/
static void *cond_broadcast(void *arg)
{
  if (mthread_mutex_lock(condition_mutex) != 0) err(9, 1);

  while(!condition_met) 
  	if (mthread_cond_wait(&condition, condition_mutex) != 0) err(9, 2);

  if (mthread_mutex_unlock(condition_mutex) != 0) err(9, 3);

  if (mthread_mutex_lock(count_mutex) != 0) err(9, 4);
  count++;
  if (mthread_mutex_unlock(count_mutex) != 0) err(9, 5);
  return(NULL);
}

/*===========================================================================*
 *				test_condition				     *
 *===========================================================================*/
static void test_condition(void)
{
#define NTHREADS 10
  int i;
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

#ifdef MTHREAD_STRICT
  /* Let's try to destroy it again. Should fails as it's uninitialized. */
  /* Note: this only works when libmthread is compiled with MTHREAD_STRICT. In
   * POSIX this situation is a MAY fail if... */
  if (mthread_cond_destroy(&condition) == 0) err(8, 10); 
#endif

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

#ifdef MTHREAD_STRICT 
  /* Again, destroying the condition variable twice shouldn't work */
  /* See previous note about MTHREAD_STRICT */
  if (mthread_cond_destroy(&condition) == 0) err(8, 23); 
#endif

#ifdef MDEBUG
  mthread_verify();
#endif
}

/*===========================================================================*
 *				test_attributes				     *
 *===========================================================================*/
static void test_attributes(void)
{
  attr_t tattr;
  thread_t tid;
  int detachstate = -1, status = 0;
  unsigned int i, no_ints, stack_untouched = 1;
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
  free(newstackaddr);

  /* Try to allocate too small a stack; it should fail and the attribute 
   * values should remain as is.
   */
  newstacksize = MTHREAD_STACK_MIN - 1;
  stackaddr = NULL;
  stacksize = 0;
  if (mthread_attr_init(&tattr) != 0) err(11, 17);
  if ((newstackaddr = malloc(newstacksize)) == NULL) err(11, 18);
  if (mthread_attr_setstack(&tattr, newstackaddr, newstacksize) != EINVAL)
  	err(11, 19);
  if (mthread_attr_getstack(&tattr, &stackaddr, &stacksize) != 0) err(11, 21);
  if (stackaddr == newstackaddr) err(11, 22);
  if (stacksize == newstacksize) err(11, 23);
  if (mthread_attr_destroy(&tattr) != 0) err(11, 24);
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
   * downwards.
   */
#if defined(__i386__) || defined(__arm__)
  if (stackp[0] != MAGIC) err(11, 66); /* End of the stack */
  for (i = no_ints - 1 - 16; i < no_ints; i++)
  	if (stackp[i] != MAGIC) stack_untouched = 0;
  if (stack_untouched) err(11, 67); /* Beginning of the stack */
  if (stackp[no_ints / 2] != 0) err(11, 68);/*Zero half way through the stack*/
#else
#error "Unsupported chip for this test"
#endif

  if (mthread_join(tid, (void *) &status) != 0) err(11, 69);
  if ((size_t) status != stacksize) err(11, 70);
  if (mthread_attr_destroy(&tattr) != 0) err(11, 71); 
  if (mthread_mutex_destroy(condition_mutex) != 0) err(11, 72);
  if (mthread_cond_destroy(&condition) != 0) err(11, 73);
  free(stackaddr);

#ifdef MDEBUG
  mthread_verify();
#endif
}

/*===========================================================================*
 *				destr_a					     *
 *===========================================================================*/
static void destr_a(void *value)
{
  int num;

  num = (int) value;

  /* This destructor must be called once for all of the values 1..4. */
  if (num <= 0 || num > 4) err(15, 1);

  if (values[num - 1] != 1) err(15, 2);

  values[num - 1] = 2;
}

/*===========================================================================*
 *				destr_b					     *
 *===========================================================================*/
static void destr_b(void *value)
{
  /* This destructor must never trigger. */
  err(16, 1);
}

/*===========================================================================*
 *				key_a					     *
 *===========================================================================*/
static void *key_a(void *arg)
{
  int i;

  if (!first) mthread_yield();

  /* Each new threads gets NULL-initialized values. */
  for (i = 0; i < 5; i++)
	if (mthread_getspecific(key[i]) != NULL) err(17, 1);

  /* Make sure that the local values persist despite other threads' actions. */
  for (i = 1; i < 5; i++)
	if (mthread_setspecific(key[i], (void *) i) != 0) err(17, 2);

  mthread_yield();

  for (i = 1; i < 5; i++)
	if (mthread_getspecific(key[i]) != (void *) i) err(17, 3);

  mthread_yield();

  /* The other thread has deleted this key by now. */
  if (mthread_setspecific(key[3], NULL) != EINVAL) err(17, 4);

  /* If a key's value is set to NULL, its destructor must not be called. */
  if (mthread_setspecific(key[4], NULL) != 0) err(17, 5);
  return(NULL);
}

/*===========================================================================*
 *				key_b					     *
 *===========================================================================*/
static void *key_b(void *arg)
{
  int i;

  first = 1;
  mthread_yield();

  /* Each new threads gets NULL-initialized values. */
  for (i = 0; i < 5; i++)
	if (mthread_getspecific(key[i]) != NULL) err(18, 1);

  for (i = 0; i < 4; i++)
	if (mthread_setspecific(key[i], (void *) (i + 2)) != 0) err(18, 2);

  mthread_yield();

  /* Deleting a key will not cause a call its destructor at any point. */
  if (mthread_key_delete(key[3]) != 0) err(18, 3);

  mthread_exit(NULL);
  return(NULL);
}

/*===========================================================================*
 *				key_c					     *
 *===========================================================================*/
static void *key_c(void *arg)
{
  /* The only thing that this thread should do, is set a value. */
  if (mthread_setspecific(key[0], (void *) mthread_self()) != 0) err(19, 1);

  mthread_yield();

  if (!mthread_equal((thread_t) mthread_getspecific(key[0]), mthread_self()))
	err(19, 2);
  return(NULL);
}

/*===========================================================================*
 *				test_keys				     *
 *===========================================================================*/
static void test_keys(void)
{
  thread_t t[24];
  int i, j;

  /* Make sure that we can create exactly MTHREAD_KEYS_MAX keys. */
  memset(key, 0, sizeof(key));

  for (i = 0; i < MTHREAD_KEYS_MAX; i++) {
	if (mthread_key_create(&key[i], NULL) != 0) err(20, 1);

	for (j = 0; j < i - 1; j++)
		if (key[i] == key[j]) err(20, 2);
  }

  if (mthread_key_create(&key[i], NULL) != EAGAIN) err(20, 3);

  for (i = 3; i < MTHREAD_KEYS_MAX; i++)
	if (mthread_key_delete(key[i]) != 0) err(20, 4);

  /* Test basic good and bad value assignment and retrieval. */
  if (mthread_setspecific(key[0], (void *) 1) != 0) err(20, 5);
  if (mthread_setspecific(key[1], (void *) 2) != 0) err(20, 6);
  if (mthread_setspecific(key[2], (void *) 3) != 0) err(20, 7);
  if (mthread_setspecific(key[1], NULL) != 0) err(20, 8);
  if (mthread_getspecific(key[0]) != (void *) 1) err(20, 9);
  if (mthread_getspecific(key[1]) != NULL) err(20, 10);
  if (mthread_getspecific(key[2]) != (void *) 3) err(20, 11);
  if (mthread_setspecific(key[3], (void *) 4) != EINVAL) err(20, 12);
  if (mthread_setspecific(key[3], NULL) != EINVAL) err(20, 13);

  if (mthread_key_delete(key[1]) != 0) err(20, 14);
  if (mthread_key_delete(key[2]) != 0) err(20, 15);

  /* Test thread locality and destructors. */
  if (mthread_key_create(&key[1], destr_a) != 0) err(20, 16);
  if (mthread_key_create(&key[2], destr_a) != 0) err(20, 17);
  if (mthread_key_create(&key[3], destr_b) != 0) err(20, 18);
  if (mthread_key_create(&key[4], destr_b) != 0) err(20, 19);

  if (mthread_getspecific(key[2]) != NULL) err(20, 20);

  for (i = 0; i < 4; i++)
	values[i] = 1;
  first = 0;

  if (mthread_create(&t[0], NULL, key_a, NULL) != 0) err(20, 21);
  if (mthread_create(&t[1], NULL, key_b, NULL) != 0) err(20, 22);

  for (i = 0; i < 2; i++) 
	if (mthread_join(t[i], NULL) != 0) err(20, 23);

  /* The destructors must have changed all these values now. */
  for (i = 0; i < 4; i++)
	if (values[i] != 2) err(20, 24);

  /* The original values must not have changed. */
  if (mthread_getspecific(key[0]) != (void *) 1) err(20, 25);

  /* Deleting a deleted key should not cause any problems either. */
  if (mthread_key_delete(key[3]) != EINVAL) err(20, 26);

  /* Make sure everything still works when using a larger number of threads.
   * This should trigger reallocation code within libmthread's key handling.
   */
  for (i = 0; i < 24; i++)
	if (mthread_create(&t[i], NULL, key_c, NULL) != 0) err(20, 27);

  for (i = 0; i < 24; i++) 
	if (mthread_join(t[i], NULL) != 0) err(20, 28);
}

/*===========================================================================*
 *				event_a					     *
 *===========================================================================*/
static void *event_a(void *arg)
{
  VERIFY_EVENT(0, 0, 21, 1);

  /* Wait for main thread to signal us */
  if (mthread_event_wait(&event) != 0) err(21, 2);

  /* Mark state transition and wakeup thread b */
  event_a_step = 1;
  if (mthread_event_fire(&event) != 0) err(21, 3);
  mthread_yield();
  VERIFY_EVENT(1, 1, 21, 4);

  /* Wait for main thread to signal again with fireall  */
  if (mthread_event_wait(&event) != 0) err(21, 5);

  /* Marks state transition and exit */
  event_a_step = 2;
  return(NULL);
}

/*===========================================================================*
 *				event_b					     *
 *===========================================================================*/
static void *event_b(void *arg)
{
  VERIFY_EVENT(0, 0, 22, 1);

  /* Wait for thread a to signal us */
  if (mthread_event_wait(&event) != 0) err(22, 2);
  VERIFY_EVENT(1, 0, 22, 3);

  /* Mark state transition and wait again, this time for main thread */
  event_b_step = 1;
  if (mthread_event_wait(&event) != 0) err(21, 5);

  /* Marks state transition and exit */
  event_b_step = 2;
  return(NULL);
}

/*===========================================================================*
 *				test_event				     *
 *===========================================================================*/
static void test_event(void)
{
  thread_t t[2];
  int i;

  if (mthread_event_init(&event) != 0) err(23, 1);

  /* Try with faulty memory locations */
  if (mthread_event_wait(NULL) == 0) err(23, 2);
  if (mthread_event_fire(NULL) == 0) err(23, 3);

  /* create threads */
  if (mthread_create(&t[0], NULL, event_a, NULL) != 0) err(23, 4);
  if (mthread_create(&t[1], NULL, event_b, NULL) != 0) err(23, 5);

  /* wait for them to block on event */
  mthread_yield_all();
  VERIFY_EVENT(0, 0, 23, 6);

  /* Fire event to wakeup thread a */
  if (mthread_event_fire(&event) != 0) err(23, 7);
  mthread_yield_all();
  VERIFY_EVENT(1, 1, 23, 6);

  /* Fire all to wakeup both a and b */
  if (mthread_event_fire_all(&event) != 0) err(23, 7);
  mthread_yield_all();
  VERIFY_EVENT(2, 2, 23, 8);

  /* We are done here */
  for (i = 0; i < 2; i++)
	if (mthread_join(t[i], NULL) != 0) err(23, 9);

  if (mthread_event_destroy(&event) != 0) err(23, 10);
}

/*===========================================================================*
 *				rwlock_a				     *
 *===========================================================================*/
static void *rwlock_a(void *arg)
{
  /* acquire read lock */
  VERIFY_RWLOCK(0, 0, 24, 1);
  if (mthread_rwlock_rdlock(&rwlock) != 0) err(24, 2);
  rwlock_a_step = 1;
  mthread_yield();

  /* release read lock */
  VERIFY_RWLOCK(1, 1, 24, 3);
  if (mthread_rwlock_unlock(&rwlock) != 0) err(24, 4);
  rwlock_a_step = 2;

  /* get write lock */
  if (mthread_rwlock_wrlock(&rwlock) != 0) err(24, 5);
  rwlock_a_step = 3;
  VERIFY_RWLOCK(3, 2, 24, 6);

  /* release write lock */
  if (mthread_rwlock_unlock(&rwlock) != 0) err(24, 7);
  mthread_yield();

  VERIFY_RWLOCK(3, 3, 24, 8);

  return(NULL);
}

/*===========================================================================*
 *				rwlock_b				     *
 *===========================================================================*/
static void *rwlock_b(void *arg)
{
  /* Step 1: acquire the read lock */
  VERIFY_RWLOCK(1, 0, 25, 1);
  if (mthread_rwlock_rdlock(&rwlock) != 0) err(25, 2);
  rwlock_b_step = 1;
  mthread_yield();

  /* We return back with first thread blocked on wrlock */
  VERIFY_RWLOCK(2, 1, 25, 3);
  rwlock_b_step = 2;

  /* Release read lock and acquire write lock */
  if (mthread_rwlock_unlock(&rwlock) != 0) err(25, 4);
  if (mthread_rwlock_wrlock(&rwlock) != 0) err(25, 5);
  rwlock_b_step = 3;

  VERIFY_RWLOCK(3, 3, 25, 6);
  if (mthread_rwlock_unlock(&rwlock) != 0) err(25, 6);

  return(NULL);
}

/*===========================================================================*
 *				test_rwlock				     *
 *===========================================================================*/
static void test_rwlock(void)
{
  thread_t t[2];
  int i;

  if (mthread_rwlock_init(&rwlock) != 0) err(26, 1);

  /* Try with faulty memory locations */
  if (mthread_rwlock_rdlock(NULL) == 0) err(26, 2);
  if (mthread_rwlock_wrlock(NULL) == 0) err(26, 3);
  if (mthread_rwlock_unlock(NULL) == 0) err(26, 4);

  /* Create the threads and start testing */
  if (mthread_create(&t[0], NULL, rwlock_a, NULL) != 0) err(26, 5);
  if (mthread_create(&t[1], NULL, rwlock_b, NULL) != 0) err(26, 6);

  mthread_yield_all();

  for (i = 0; i < 2; i++)
	if (mthread_join(t[i], NULL) != 0) err(26, 7);

  if (mthread_rwlock_destroy(&rwlock) != 0) err(26, 8);
}


/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(void)
{
  errct = 0;
  th_a = th_b = th_c = th_d = th_e = th_f = th_g = th_h = 0;
  mutex_a_step = mutex_b_step = mutex_c_step = 0;
  event_a_step = event_b_step = 0;
  rwlock_a_step = rwlock_b_step = 0;
  once = MTHREAD_ONCE_INIT;

  start(59);
  mthread_init(); 
  test_scheduling();
  test_mutex();
  test_event();
  test_rwlock();
  test_condition();
  test_attributes();
  test_keys();
  quit();
  return(0);	/* Not reachable */
}

