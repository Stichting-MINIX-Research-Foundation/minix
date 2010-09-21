#include <minix/mthread.h>
#include "proto.h"
#include "global.h"

PRIVATE struct __mthread_mutex *vm_front, *vm_rear;
FORWARD _PROTOTYPE( void mthread_mutex_add, (mthread_mutex_t *m)	);
FORWARD _PROTOTYPE( void mthread_mutex_remove, (mthread_mutex_t *m)	);

/*===========================================================================*
 *				mthread_init_valid_mutexes			     *
 *===========================================================================*/
PUBLIC void mthread_init_valid_mutexes(void)
{
/* Initialize list of valid mutexes */
  vm_front = vm_rear = NULL;
}


/*===========================================================================*
 *				mthread_mutex_add			     *
 *===========================================================================*/
PRIVATE void mthread_mutex_add(m) 
mthread_mutex_t *m;
{
/* Add mutex to list of valid, initialized mutexes */

  if (vm_front == NULL) {	/* Empty list */
  	vm_front = *m;
  	(*m)->prev = NULL;
  } else {
  	vm_rear->next = *m;
  	(*m)->prev = vm_rear;
  }

  (*m)->next = NULL;
  vm_rear = *m;
}


/*===========================================================================*
 *				mthread_mutex_destroy			     *
 *===========================================================================*/
PUBLIC int mthread_mutex_destroy(mutex)
mthread_mutex_t *mutex;
{
/* Invalidate mutex and deallocate resources. */

  int i;

  mthread_init();	/* Make sure mthreads is initialized */

  if (mutex == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  if (!mthread_mutex_valid(mutex)) {
  	errno = EINVAL;
  	return(-1);
  } else if ((*mutex)->owner != NO_THREAD) {
  	errno = EBUSY;
  	return(-1);
  }

  /* Check if this mutex is not associated with a condition */
  for (i = 0; i < no_threads; i++) {
	if (threads[i].m_state == CONDITION) {
		if (threads[i].m_cond != NULL &&
		    threads[i].m_cond->mutex == *mutex) {
			errno = EBUSY;
			return(-1);
		}
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
PUBLIC int mthread_mutex_init(mutex, mattr)
mthread_mutex_t *mutex;	/* Mutex that is to be initialized */
mthread_mutexattr_t *mattr;	/* Mutex attribute */
{
/* Initialize the mutex to a known state. Attributes are not supported */

  struct __mthread_mutex *m;

  mthread_init();	/* Make sure mthreads is initialized */

  if (mutex == NULL) {
  	errno = EAGAIN;
  	return(-1);
  } else if (mattr != NULL) {
  	errno = ENOSYS;
  	return(-1);
  } else if (mthread_mutex_valid(mutex)) {
  	errno = EBUSY;
  	return(-1);
  }

  if ((m = malloc(sizeof(struct __mthread_mutex))) == NULL)
  	return(-1);

  mthread_queue_init( &(m->queue) );
  m->owner = NO_THREAD;
  *mutex = (mthread_mutex_t) m;
  mthread_mutex_add(mutex); /* Validate mutex; mutex now in use */

  return(0);
}

/*===========================================================================*
 *				mthread_mutex_lock			     *
 *===========================================================================*/
PUBLIC int mthread_mutex_lock(mutex)
mthread_mutex_t *mutex;	/* Mutex that is to be locked */
{
/* Try to lock this mutex. If already locked, append the current thread to
 * FIFO queue associated with this mutex and suspend the thread. */

  struct __mthread_mutex *m;

  mthread_init();	/* Make sure mthreads is initialized */

  if (mutex == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  m = (struct __mthread_mutex *) *mutex;
  if (!mthread_mutex_valid(&m)) {
  	errno = EINVAL;
  	return(-1);
  } else if (m->owner == NO_THREAD) { /* Not locked */
	m->owner = current_thread;
  } else if (m->owner == current_thread) {
  	errno = EDEADLK;
  	return(-1);
  } else {
	mthread_queue_add( &(m->queue), current_thread);
	mthread_suspend(MUTEX);
  }

  /* When we get here we acquired the lock. */
  return(0);
}


/*===========================================================================*
 *				mthread_mutex_remove			     *
 *===========================================================================*/
PRIVATE void mthread_mutex_remove(m)
mthread_mutex_t *m;
{
/* Remove mutex from list of valid, initialized mutexes */

  if ((*m)->prev == NULL)
  	vm_front = (*m)->next;
  else
  	(*m)->prev->next = (*m)->next;

  if ((*m)->next == NULL)
  	vm_rear = (*m)->prev;
  else
  	(*m)->next->prev = (*m)->prev;
}


/*===========================================================================*
 *				mthread_mutex_trylock			     *
 *===========================================================================*/
PUBLIC int mthread_mutex_trylock(mutex)
mthread_mutex_t *mutex;	/* Mutex that is to be locked */
{
/* Try to lock this mutex and return OK. If already locked, return error. */

  struct __mthread_mutex *m;

  mthread_init();	/* Make sure mthreads is initialized */

  if (mutex == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  m = (struct __mthread_mutex *) *mutex;
  if (!mthread_mutex_valid(&m)) {
  	errno = EINVAL;
  	return(-1);
  } else if (m->owner == NO_THREAD) {
	m->owner = current_thread;
	return(0);
  } 

  errno = EBUSY;
  return(-1);
}


/*===========================================================================*
 *				mthread_mutex_unlock			     *
 *===========================================================================*/
PUBLIC int mthread_mutex_unlock(mutex)
mthread_mutex_t *mutex;	/* Mutex that is to be unlocked */
{
/* Unlock a previously locked mutex. If there is a pending lock for this mutex 
 * by another thread, mark that thread runnable. */

  struct __mthread_mutex *m;

  mthread_init();	/* Make sure mthreads is initialized */

  if (mutex == NULL) { 
	errno = EINVAL;
	return(-1);
  }

  m = (struct __mthread_mutex *) *mutex;
  if (!mthread_mutex_valid(&m)) {
	errno = EINVAL;
	return(-1);
  } else if (m->owner != current_thread) {
  	errno = EPERM;
  	return(-1); /* Can't unlock a mutex locked by another thread. */
  }

  m->owner = mthread_queue_remove( &(m->queue) );
  if (m->owner != NO_THREAD) mthread_unsuspend(m->owner);
  return(0);
}


/*===========================================================================*
 *				mthread_mutex_valid			     *
 *===========================================================================*/
PUBLIC int mthread_mutex_valid(m)
mthread_mutex_t *m;
{
/* Check to see if mutex is on the list of valid mutexes */
  struct __mthread_mutex *loopitem;

  mthread_init();	/* Make sure mthreads is initialized */

  loopitem = vm_front;

  while (loopitem != NULL) {
  	if (loopitem == *m) 
  		return(1);

  	loopitem = loopitem->next;
  }

  return(0);
}


/*===========================================================================*
 *				mthread_mutex_verify			     *
 *===========================================================================*/
#ifdef MDEBUG
PUBLIC int mthread_mutex_verify(void)
{
  /* Return true when no mutexes are in use */
  int r = 1;
  struct __mthread_mutex *loopitem;

  mthread_init();	/* Make sure mthreads is initialized */

  loopitem = vm_front;

  while (loopitem != NULL) {
  	loopitem = loopitem->next;
  	r = 0;
  }

  return(r);
}
#endif


