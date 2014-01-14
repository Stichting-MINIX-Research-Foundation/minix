#include <minix/mthread.h>
#include "global.h"
#include "proto.h"

#ifdef MTHREAD_STRICT
static struct __mthread_mutex *vm_front, *vm_rear;
static void mthread_mutex_add(mthread_mutex_t *m);
static void mthread_mutex_remove(mthread_mutex_t *m);
#else
# define mthread_mutex_add(m)		((*m)->mm_magic = MTHREAD_INIT_MAGIC)
# define mthread_mutex_remove(m)	((*m)->mm_magic = MTHREAD_NOT_INUSE)
#endif

/*===========================================================================*
 *				mthread_init_valid_mutexes		     *
 *===========================================================================*/
void mthread_init_valid_mutexes(void)
{
#ifdef MTHREAD_STRICT
/* Initialize list of valid mutexes */
  vm_front = vm_rear = NULL;
#endif
}


/*===========================================================================*
 *				mthread_mutex_add			     *
 *===========================================================================*/
#ifdef MTHREAD_STRICT
static void mthread_mutex_add(m) 
mthread_mutex_t *m;
{
/* Add mutex to list of valid, initialized mutexes */

  if (vm_front == NULL) {	/* Empty list */
  	vm_front = *m;
  	(*m)->mm_prev = NULL;
  } else {
  	vm_rear->mm_next = *m;
  	(*m)->mm_prev = vm_rear;
  }

  (*m)->mm_next = NULL;
  vm_rear = *m;
}
#endif

/*===========================================================================*
 *				mthread_mutex_destroy			     *
 *===========================================================================*/
int mthread_mutex_destroy(mutex)
mthread_mutex_t *mutex;
{
/* Invalidate mutex and deallocate resources. */

  mthread_thread_t t;
  mthread_tcb_t *tcb;

  if (mutex == NULL)
  	return(EINVAL);

  if (!mthread_mutex_valid(mutex)) 
  	return(EINVAL);
  else if ((*mutex)->mm_owner != NO_THREAD)
  	return(EBUSY);

  /* Check if this mutex is not associated with a condition */
  for (t = (mthread_thread_t) 0; t < no_threads; t++) {
  	tcb = mthread_find_tcb(t);
	if (tcb->m_state == MS_CONDITION) {
		if (tcb->m_cond != NULL && tcb->m_cond->mc_mutex == *mutex) 
			return(EBUSY);
	}
  }

  /* Not in use; invalidate it */
  mthread_mutex_remove(mutex);	
  free(*mutex);
  *mutex = NULL;

  return(0);
}


/*===========================================================================*
 *				mthread_mutex_init			     *
 *===========================================================================*/
int mthread_mutex_init(mutex, mattr)
mthread_mutex_t *mutex;	/* Mutex that is to be initialized */
mthread_mutexattr_t *mattr;	/* Mutex attribute */
{
/* Initialize the mutex to a known state. Attributes are not supported */

  struct __mthread_mutex *m;

  if (mutex == NULL)
  	return(EAGAIN);
  else if (mattr != NULL)
  	return(ENOSYS);
#ifdef MTHREAD_STRICT
  else if (mthread_mutex_valid(mutex))
  	return(EBUSY);
#endif
  else if ((m = malloc(sizeof(struct __mthread_mutex))) == NULL) 
  	return(ENOMEM);

  mthread_queue_init(&m->mm_queue);
  m->mm_owner = NO_THREAD;
  *mutex = (mthread_mutex_t) m;
  mthread_mutex_add(mutex); /* Validate mutex; mutex now in use */

  return(0);
}

/*===========================================================================*
 *				mthread_mutex_lock			     *
 *===========================================================================*/
int mthread_mutex_lock(mutex)
mthread_mutex_t *mutex;	/* Mutex that is to be locked */
{
/* Try to lock this mutex. If already locked, append the current thread to
 * FIFO queue associated with this mutex and suspend the thread. */

  struct __mthread_mutex *m;

  if (mutex == NULL)
  	return(EINVAL);

  m = (struct __mthread_mutex *) *mutex;
  if (!mthread_mutex_valid(&m)) 
  	return(EINVAL);
  else if (m->mm_owner == NO_THREAD) { /* Not locked */
	m->mm_owner = current_thread;
  } else if (m->mm_owner == current_thread) {
  	return(EDEADLK);
  } else {
	mthread_queue_add(&m->mm_queue, current_thread);
	mthread_suspend(MS_MUTEX);
  }

  /* When we get here we acquired the lock. */
  return(0);
}


/*===========================================================================*
 *				mthread_mutex_remove			     *
 *===========================================================================*/
#ifdef MTHREAD_STRICT
static void mthread_mutex_remove(m)
mthread_mutex_t *m;
{
/* Remove mutex from list of valid, initialized mutexes */

  if ((*m)->mm_prev == NULL)
  	vm_front = (*m)->mm_next;
  else
  	(*m)->mm_prev->mm_next = (*m)->mm_next;

  if ((*m)->mm_next == NULL)
  	vm_rear = (*m)->mm_prev;
  else
  	(*m)->mm_next->mm_prev = (*m)->mm_prev;
}
#endif

/*===========================================================================*
 *				mthread_mutex_trylock			     *
 *===========================================================================*/
int mthread_mutex_trylock(mutex)
mthread_mutex_t *mutex;	/* Mutex that is to be locked */
{
/* Try to lock this mutex and return OK. If already locked, return error. */

  struct __mthread_mutex *m;

  if (mutex == NULL) 
  	return(EINVAL);

  m = (struct __mthread_mutex *) *mutex;
  if (!mthread_mutex_valid(&m))
  	return(EINVAL);
  else if (m->mm_owner == current_thread)
	return(EDEADLK);
  else if (m->mm_owner == NO_THREAD) {
	m->mm_owner = current_thread;
	return(0);
  } 

  return(EBUSY);
}


/*===========================================================================*
 *				mthread_mutex_unlock			     *
 *===========================================================================*/
int mthread_mutex_unlock(mutex)
mthread_mutex_t *mutex;	/* Mutex that is to be unlocked */
{
/* Unlock a previously locked mutex. If there is a pending lock for this mutex 
 * by another thread, mark that thread runnable. */

  struct __mthread_mutex *m;

  if (mutex == NULL) 
	return(EINVAL);

  m = (struct __mthread_mutex *) *mutex;
  if (!mthread_mutex_valid(&m))
	return(EINVAL);
  else if (m->mm_owner != current_thread) 
  	return(EPERM);	/* Can't unlock a mutex locked by another thread. */

  m->mm_owner = mthread_queue_remove(&m->mm_queue);
  if (m->mm_owner != NO_THREAD) mthread_unsuspend(m->mm_owner);
  return(0);
}


/*===========================================================================*
 *				mthread_mutex_valid			     *
 *===========================================================================*/
#ifdef MTHREAD_STRICT
int mthread_mutex_valid(m)
mthread_mutex_t *m;
{
/* Check to see if mutex is on the list of valid mutexes */
  struct __mthread_mutex *loopitem;

  loopitem = vm_front;

  while (loopitem != NULL) {
	if (loopitem == *m)
		return(1);

	loopitem = loopitem->mm_next;
  }

  return(0);
}
#endif

/*===========================================================================*
 *				mthread_mutex_verify			     *
 *===========================================================================*/
#ifdef MDEBUG
int mthread_mutex_verify(void)
{
  /* Return true when no mutexes are in use */
  int r = 1;
  struct __mthread_mutex *loopitem;

#ifdef MTHREAD_STRICT
  loopitem = vm_front;

  while (loopitem != NULL) {
  	printf("mutex corruption: owner: %d\n", loopitem->mm_owner);
	loopitem = loopitem->mm_next;
  	r = 0;
  }
#endif

  return(r);
}
#endif
