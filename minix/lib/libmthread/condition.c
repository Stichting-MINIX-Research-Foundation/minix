#include <stdint.h>
#include <minix/mthread.h>
#include "global.h"
#include "proto.h"

#ifdef MTHREAD_STRICT
static struct __mthread_cond *vc_front, *vc_rear;
static void mthread_cond_add(mthread_cond_t *c);
static void mthread_cond_remove(mthread_cond_t *c);
static int mthread_cond_valid(mthread_cond_t *c);
#else
# define mthread_cond_add(c)		((*c)->mc_magic = MTHREAD_INIT_MAGIC)
# define mthread_cond_remove(c)		((*c)->mc_magic = MTHREAD_NOT_INUSE)
# define mthread_cond_valid(c)		((*c)->mc_magic == MTHREAD_INIT_MAGIC)
#endif
#define MAIN_COND mainthread.m_cond

/* Is 'tcb' waiting on condition 'c' and not signaled yet?  A thread in
 * mthread_cond_wait() is suspended (MS_CONDITION); one in
 * mthread_cond_timedwait() stays runnable and polls m_signaled. */
static int mthread_cond_waiting(mthread_tcb_t *tcb, struct __mthread_cond *c)
{
  if (tcb->m_cond != c) return(0);
  if (tcb->m_state == MS_CONDITION) return(1);
  return(tcb->m_timedwait && !tcb->m_signaled);
}

/* Wake a waiter found by mthread_cond_waiting(). */
static void mthread_cond_wake(mthread_thread_t t, mthread_tcb_t *tcb)
{
  if (tcb->m_state == MS_CONDITION)
	mthread_unsuspend(t);
  else
	tcb->m_signaled = 1;
}

/*===========================================================================*
 *				mthread_init_valid_conditions		     *
 *===========================================================================*/
void mthread_init_valid_conditions(void)
{
#ifdef MTHREAD_STRICT
/* Initialize condition variable list */
  vc_front = vc_rear = NULL;
#endif
}


/*===========================================================================*
 *				mthread_cond_add			     *
 *===========================================================================*/
#ifdef MTHREAD_STRICT
static void mthread_cond_add(c) 
mthread_cond_t *c;
{
/* Add condition to list of valid, initialized conditions */

  if (vc_front == NULL) {	/* Empty list */
  	vc_front = *c;
  	(*c)->mc_prev = NULL;
  } else {
  	vc_rear->mc_next = *c;
  	(*c)->mc_prev = vc_rear;
  }

  (*c)->mc_next = NULL;
  vc_rear = *c;
}
#endif

/*===========================================================================*
 *				mthread_cond_broadcast			     *
 *===========================================================================*/
int mthread_cond_broadcast(cond)
mthread_cond_t *cond;
{
/* Signal all threads waiting for condition 'cond'. */
  mthread_thread_t t;
  mthread_tcb_t *tcb;

  if (cond == NULL) 
  	return(EINVAL);
  else if (!mthread_cond_valid(cond))
  	return(EINVAL);

  tcb = mthread_find_tcb(MAIN_THREAD);
  if (mthread_cond_waiting(tcb, *cond))
  	mthread_cond_wake(MAIN_THREAD, tcb);

  for (t = (mthread_thread_t) 0; t < no_threads; t++) {
  	tcb = mthread_find_tcb(t);
	if (mthread_cond_waiting(tcb, *cond))
		mthread_cond_wake(t, tcb);
  }

  return(0);
}


/*===========================================================================*
 *				mthread_cond_destroy			     *
 *===========================================================================*/
int mthread_cond_destroy(cond)
mthread_cond_t *cond;
{
/* Destroy a condition variable. Make sure it's not in use */
  mthread_thread_t t;
  mthread_tcb_t *tcb;

  if (cond == NULL)
  	return(EINVAL);
  else if (!mthread_cond_valid(cond))
  	return(EINVAL);

  /* Is another thread currently using this condition variable? */
  tcb = mthread_find_tcb(MAIN_THREAD);
  if ((tcb->m_state == MS_CONDITION || tcb->m_timedwait) &&
      tcb->m_cond == *cond)
  	return(EBUSY);

  for (t = (mthread_thread_t) 0; t < no_threads; t++) {
  	tcb = mthread_find_tcb(t);
	if ((tcb->m_state == MS_CONDITION || tcb->m_timedwait) &&
	    tcb->m_cond == *cond)
		return(EBUSY);
  }

  /* Not in use; invalidate it. */
  mthread_cond_remove(cond);
  free(*cond);
  *cond = NULL;

  return(0);
}


/*===========================================================================*
 *				mthread_cond_init			     *
 *===========================================================================*/
int mthread_cond_init(cond, cattr)
mthread_cond_t *cond;
mthread_condattr_t *cattr;
{
/* Initialize condition variable to a known state. cattr is ignored */
  struct __mthread_cond *c;

  if (cond == NULL) 
	return(EINVAL);

#ifdef MTHREAD_STRICT
  else if (mthread_cond_valid(cond)) 
	/* Already initialized */
  	return(EBUSY);
#endif
  else if ((c = malloc(sizeof(struct __mthread_cond))) == NULL) 
  	return(ENOMEM);

  c->mc_mutex = NULL;
  /* The attribute is NULL (default clock) or the clock id + 1, see
   * pthread_condattr_setclock(). */
  if (cattr != NULL && *cattr != NULL)
	c->mc_clock = (clockid_t) ((uintptr_t) *cattr - 1);
  else
	c->mc_clock = CLOCK_REALTIME;
  *cond = (mthread_cond_t) c;
  mthread_cond_add(cond);

  return(0);
}


/*===========================================================================*
 *				mthread_cond_remove			     *
 *===========================================================================*/
#ifdef MTHREAD_STRICT
static void mthread_cond_remove(c)
mthread_cond_t *c;
{
/* Remove condition from list of valid, initialized conditions */

  if ((*c)->mc_prev == NULL)
  	vc_front = (*c)->mc_next;
  else
  	(*c)->mc_prev->mc_next = (*c)->mc_next;

  if ((*c)->mc_next == NULL)
  	vc_rear = (*c)->mc_prev;
  else
  	(*c)->mc_next->mc_prev = (*c)->mc_prev;

}
#endif

/*===========================================================================*
 *				mthread_cond_signal			     *
 *===========================================================================*/
int mthread_cond_signal(cond)
mthread_cond_t *cond;
{
/* Signal a thread that condition 'cond' was met. Just a single thread. */
  mthread_thread_t t;
  mthread_tcb_t *tcb;

  if (cond == NULL)
	return(EINVAL);
  else if (!mthread_cond_valid(cond))
	return(EINVAL);

  tcb = mthread_find_tcb(MAIN_THREAD);
  if (mthread_cond_waiting(tcb, *cond)) {
  	mthread_cond_wake(MAIN_THREAD, tcb);
	return(0);
  }

  for (t = (mthread_thread_t) 0; t < no_threads; t++) {
  	tcb = mthread_find_tcb(t);
	if (mthread_cond_waiting(tcb, *cond)) {
		mthread_cond_wake(t, tcb);
		break;
	}
  }

  return(0);
}


/*===========================================================================*
 *				mthread_cond_valid			     *
 *===========================================================================*/
#ifdef MTHREAD_STRICT
static int mthread_cond_valid(c)
mthread_cond_t *c;
{
/* Check to see if cond is on the list of valid conditions */
  struct __mthread_cond *loopitem;

  loopitem = vc_front;

  while (loopitem != NULL) {
  	if (loopitem == *c)
  		return(1);

  	loopitem = loopitem->mc_next;
  }

  return(0);
}
#endif

/*===========================================================================*
 *				mthread_cond_verify			     *
 *===========================================================================*/
#ifdef MDEBUG
int mthread_cond_verify(void)
{
/* Return true in case no condition variables are in use. */

  return(vc_front == NULL);
}
#endif


/*===========================================================================*
 *				mthread_cond_wait			     *
 *===========================================================================*/
int mthread_cond_wait(cond, mutex)
mthread_cond_t *cond;
mthread_mutex_t *mutex;
{
/* Wait for a condition to be signaled */
  mthread_tcb_t *tcb;
  struct __mthread_cond *c;
  struct __mthread_mutex *m;
  unsigned int count;

  if (cond == NULL || mutex == NULL)
	return(EINVAL);
  
  c = (struct __mthread_cond *) *cond;
  m = (struct __mthread_mutex *) *mutex;

  if (!mthread_cond_valid(cond) || !mthread_mutex_valid(mutex)) 
	return(EINVAL);

  c->mc_mutex = m;	/* Remember we're using this mutex in a cond_wait */
  count = m->mm_count;	/* release a recursive mutex completely */
  m->mm_count = 0;
  if (mthread_mutex_unlock(mutex) != 0) { /* Fails when we're not the owner */
	m->mm_count = count;
  	return(-1);
  }

  tcb = mthread_find_tcb(current_thread);
  tcb->m_cond = c; /* Register condition variable. */
  mthread_suspend(MS_CONDITION);

  /* When execution returns here, the condition was met. Lock mutex again. */
  c->mc_mutex = NULL;				/* Forget about this mutex */
  tcb->m_cond = NULL;				/* ... and condition var */
  if (mthread_mutex_lock(mutex) != 0)
  	return(-1);
  m->mm_count = count;

  return(0);
}

/* pthread compatibility layer. */
__weak_alias(pthread_cond_init, mthread_cond_init)


