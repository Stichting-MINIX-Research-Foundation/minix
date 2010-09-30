#include <minix/mthread.h>
#include "global.h"
#include "proto.h"

PRIVATE struct __mthread_cond *vc_front, *vc_rear;
FORWARD _PROTOTYPE( void mthread_cond_add, (mthread_cond_t *c)		);
FORWARD _PROTOTYPE( void mthread_cond_remove, (mthread_cond_t *c)	);
FORWARD _PROTOTYPE( int mthread_cond_valid, (mthread_cond_t *c)	);
#define MAIN_COND mainthread.m_cond

/*===========================================================================*
 *				mthread_init_valid_conditions		     *
 *===========================================================================*/
PUBLIC void mthread_init_valid_conditions(void)
{
/* Initialize condition variable list */
  vc_front = vc_rear = NULL;
}


/*===========================================================================*
 *				mthread_cond_add			     *
 *===========================================================================*/
PRIVATE void mthread_cond_add(c) 
mthread_cond_t *c;
{
/* Add condition to list of valid, initialized conditions */

  if (vc_front == NULL) {	/* Empty list */
  	vc_front = *c;
  	(*c)->prev = NULL;
  } else {
  	vc_rear->next = *c;
  	(*c)->prev = vc_rear;
  }

  (*c)->next = NULL;
  vc_rear = *c;
}


/*===========================================================================*
 *				mthread_cond_broadcast			     *
 *===========================================================================*/
PUBLIC int mthread_cond_broadcast(cond)
mthread_cond_t *cond;
{
/* Signal all threads waiting for condition 'cond'. */
  mthread_thread_t t;
  mthread_tcb_t *tcb;

  mthread_init();	/* Make sure libmthread is initialized */

  if(cond == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  if (!mthread_cond_valid(cond)) {
  	errno = EINVAL;
  	return(-1);
  }

  tcb = mthread_find_tcb(MAIN_THREAD);
  if (tcb->m_state == MS_CONDITION && tcb->m_cond == *cond)
  	mthread_unsuspend(MAIN_THREAD);

  for (t = (mthread_thread_t) 0; t < no_threads; t++) {
  	tcb = mthread_find_tcb(t);
	if (tcb->m_state == MS_CONDITION && tcb->m_cond == *cond) 
		mthread_unsuspend(t);
  }

  return(0);
}


/*===========================================================================*
 *				mthread_cond_destroy			     *
 *===========================================================================*/
PUBLIC int mthread_cond_destroy(cond)
mthread_cond_t *cond;
{
/* Destroy a condition variable. Make sure it's not in use */
  mthread_thread_t t;
  mthread_tcb_t *tcb;

  mthread_init();	/* Make sure libmthread is initialized */

  if (cond == NULL) { 
  	errno = EINVAL;
  	return(-1);
  } 

  if (!mthread_cond_valid(cond)) {
  	errno = EINVAL;
  	return(-1);
  }

  /* Is another thread currently using this condition variable? */
  tcb = mthread_find_tcb(MAIN_THREAD);
  if (tcb->m_state == MS_CONDITION && tcb->m_cond == *cond) {
  	errno = EBUSY;
  	return(-1);
  }

  for (t = (mthread_thread_t) 0; t < no_threads; t++) {
  	tcb = mthread_find_tcb(t);
	if(tcb->m_state == MS_CONDITION && tcb->m_cond == *cond){
		errno = EBUSY;
		return(-1);
	}
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
PUBLIC int mthread_cond_init(cond, cattr)
mthread_cond_t *cond;
mthread_condattr_t *cattr;
{
/* Initialize condition variable to a known state. cattr is ignored */
  struct __mthread_cond *c;

  mthread_init();	/* Make sure libmthread is initialized */

  if (cond == NULL) {
	errno = EINVAL;
	return(-1); 
  } else if (cattr != NULL) {
  	errno = ENOSYS;
  	return(-1);
  }

  if (mthread_cond_valid(cond)) {
	/* Already initialized */
  	errno = EBUSY;
  	return(-1);
  } 

  if ((c = malloc(sizeof(struct __mthread_cond))) == NULL)
  	return(-1);

  c->mutex = NULL;
  *cond = (mthread_cond_t) c;
  mthread_cond_add(cond);

  return(0);
}


/*===========================================================================*
 *				mthread_cond_remove			     *
 *===========================================================================*/
PRIVATE void mthread_cond_remove(c)
mthread_cond_t *c;
{
/* Remove condition from list of valid, initialized conditions */

  if ((*c)->prev == NULL)
  	vc_front = (*c)->next;
  else
  	(*c)->prev->next = (*c)->next;

  if ((*c)->next == NULL)
  	vc_rear = (*c)->prev;
  else
  	(*c)->next->prev = (*c)->prev;

}


/*===========================================================================*
 *				mthread_cond_signal			     *
 *===========================================================================*/
PUBLIC int mthread_cond_signal(cond)
mthread_cond_t *cond;
{
/* Signal a thread that condition 'cond' was met. Just a single thread. */
  mthread_thread_t t;
  mthread_tcb_t *tcb;

  mthread_init();	/* Make sure libmthread is initialized */

  if(cond == NULL) {
	errno = EINVAL;
	return(-1);
  }

  if (!mthread_cond_valid(cond)) {
	errno = EINVAL;
	return(-1);
  }

  tcb = mthread_find_tcb(MAIN_THREAD);
  if (tcb->m_state == MS_CONDITION && tcb->m_cond == *cond)
  	mthread_unsuspend(MAIN_THREAD);

  for (t = (mthread_thread_t) 0; t < no_threads; t++) {
  	tcb = mthread_find_tcb(t);
	if(tcb->m_state == MS_CONDITION && tcb->m_cond == *cond){
		mthread_unsuspend(t);
		break;
	}
  }

  return(0);
}


/*===========================================================================*
 *				mthread_cond_valid			     *
 *===========================================================================*/
PRIVATE int mthread_cond_valid(c)
mthread_cond_t *c;
{
/* Check to see if cond is on the list of valid conditions */
  struct __mthread_cond *loopitem;

  loopitem = vc_front;

  while (loopitem != NULL) {
  	if (loopitem == *c)
  		return(1);

  	loopitem = loopitem->next;
  }

  return(0);
}


/*===========================================================================*
 *				mthread_cond_verify			     *
 *===========================================================================*/
#ifdef MDEBUG
PUBLIC int mthread_cond_verify(void)
{
/* Return true in case no condition variables are in use. */

  mthread_init();	/* Make sure libmthread is initialized */

  return(vc_front == NULL);
}
#endif


/*===========================================================================*
 *				mthread_cond_wait			     *
 *===========================================================================*/
PUBLIC int mthread_cond_wait(cond, mutex)
mthread_cond_t *cond;
mthread_mutex_t *mutex;
{
/* Wait for a condition to be signaled */
  mthread_tcb_t *tcb;
  struct __mthread_cond *c;
  struct __mthread_mutex *m;

  mthread_init();	/* Make sure libmthread is initialized */

  if (cond == NULL || mutex == NULL) {
	errno = EINVAL;
	return(-1);
  }
  
  c = (struct __mthread_cond *) *cond;
  m = (struct __mthread_mutex *) *mutex;

  if (!mthread_cond_valid(cond) || !mthread_mutex_valid(mutex)) {
	errno = EINVAL;
	return(-1);
  }

  c->mutex = m;	/* Remember we're using this mutex in a cond_wait */
  if (mthread_mutex_unlock(mutex) != 0) /* Fails when we're not the owner */
  	return(-1);

  tcb = mthread_find_tcb(current_thread);
  tcb->m_cond = c; /* Register condition variable. */
  mthread_suspend(MS_CONDITION);

  /* When execution returns here, the condition was met. Lock mutex again. */
  c->mutex = NULL;				/* Forget about this mutex */
  tcb->m_cond = NULL;				/* ... and condition var */
  if (mthread_mutex_lock(mutex) != 0)
  	return(-1);

  return(0);
}


