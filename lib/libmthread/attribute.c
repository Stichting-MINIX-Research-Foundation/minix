#include <minix/mthread.h>
#include "global.h"
#include "proto.h"

PRIVATE struct __mthread_attr *va_front, *va_rear;
FORWARD _PROTOTYPE( void mthread_attr_add, (mthread_attr_t *a)		);
FORWARD _PROTOTYPE( void mthread_attr_remove, (mthread_attr_t *a)	);
FORWARD _PROTOTYPE( int mthread_attr_valid, (mthread_attr_t *a)	);

/*===========================================================================*
 *			mthread_init_valid_attributes			     *
 *===========================================================================*/
PUBLIC void mthread_init_valid_attributes(void)
{
/* Initialize list of valid attributs */
  va_front = va_rear = NULL;
}


/*===========================================================================*
 *				mthread_attr_add			     *
 *===========================================================================*/
PRIVATE void mthread_attr_add(a) 
mthread_attr_t *a;
{
/* Add attribute to list of valid, initialized attributes */

  if (va_front == NULL) {	/* Empty list */
  	va_front = *a;
  	(*a)->prev = NULL;
  } else {
  	va_rear->next = *a;
  	(*a)->prev = va_rear;
  }

  (*a)->next = NULL;
  va_rear = *a;
}


/*===========================================================================*
 *				mthread_attr_destroy			     *
 *===========================================================================*/
PUBLIC int mthread_attr_destroy(attr)
mthread_attr_t *attr;
{
/* Invalidate attribute and deallocate resources. */

  mthread_init();	/* Make sure mthreads is initialized */

  if (attr == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  if (!mthread_attr_valid(attr)) {
  	errno = EINVAL;
  	return(-1);
  }

  /* Valide attribute; invalidate it */
  mthread_attr_remove(attr);	
  free(*attr);
  *attr = NULL;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_init			     *
 *===========================================================================*/
PUBLIC int mthread_attr_init(attr)
mthread_attr_t *attr;	/* Attribute */
{
/* Initialize the attribute to a known state. */
  struct __mthread_attr *a;

  mthread_init();	/* Make sure mthreads is initialized */

  if (attr == NULL) {
  	errno = EAGAIN;
  	return(-1);
  } else if (mthread_attr_valid(attr)) {
  	errno = EBUSY;
  	return(-1);
  }

  if ((a = malloc(sizeof(struct __mthread_attr))) == NULL)
  	return(-1);

  a->a_detachstate = MTHREAD_CREATE_JOINABLE;
  a->a_stackaddr = NULL;
  a->a_stacksize = (size_t) 0;

  *attr = (mthread_attr_t) a;
  mthread_attr_add(attr); /* Validate attribute: attribute now in use */

  return(0);
}

/*===========================================================================*
 *				mthread_attr_getdetachstate			     *
 *===========================================================================*/
PUBLIC int mthread_attr_getdetachstate(attr, detachstate)
mthread_attr_t *attr;
int *detachstate;
{
/* Get detachstate of a thread attribute */
  struct __mthread_attr *a;

  mthread_init();	/* Make sure mthreads is initialized */

  if (attr == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr)) {
  	errno = EINVAL;
  	return(-1);
  }

  *detachstate = a->a_detachstate;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_setdetachstate			     *
 *===========================================================================*/
PUBLIC int mthread_attr_setdetachstate(attr, detachstate)
mthread_attr_t *attr;
int detachstate;
{
/* Set detachstate of a thread attribute */
  struct __mthread_attr *a;

  mthread_init();	/* Make sure mthreads is initialized */

  if (attr == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr)) {
  	errno = EINVAL;
  	return(-1);
  } else if(detachstate != MTHREAD_CREATE_JOINABLE &&
  	    detachstate != MTHREAD_CREATE_DETACHED) {
	errno = EINVAL;
	return(-1);
  }

  a->a_detachstate = detachstate;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_getstack			     *
 *===========================================================================*/
PUBLIC int mthread_attr_getstack(attr, stackaddr, stacksize)
mthread_attr_t *attr;
void **stackaddr;
size_t *stacksize;
{
/* Get stack attribute */
  struct __mthread_attr *a;

  mthread_init();	/* Make sure mthreads is initialized */

  if (attr == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr)) {
  	errno = EINVAL;
  	return(-1);
  } 

  *stackaddr = a->a_stackaddr;
  *stacksize = a->a_stacksize;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_getstacksize		     *
 *===========================================================================*/
PUBLIC int mthread_attr_getstacksize(attr, stacksize)
mthread_attr_t *attr;
size_t *stacksize;
{
/* Get stack size attribute */
  struct __mthread_attr *a;

  mthread_init();	/* Make sure mthreads is initialized */

  if (attr == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr)) {
  	errno = EINVAL;
  	return(-1);
  } 

  *stacksize = a->a_stacksize;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_setstack			     *
 *===========================================================================*/
PUBLIC int mthread_attr_setstack(attr, stackaddr, stacksize)
mthread_attr_t *attr;
void *stackaddr;
size_t stacksize;
{
/* Set stack attribute */
  struct __mthread_attr *a;

  mthread_init();	/* Make sure mthreads is initialized */

  if (attr == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr) || stacksize < MTHREAD_STACK_MIN) {
  	errno = EINVAL;
  	return(-1);
  } 
  /* We don't care about address alignment (POSIX standard). The ucontext
   * system calls will make sure that the provided stack will be aligned (at
   * the cost of some memory if needed).
   */

  a->a_stackaddr = stackaddr;
  a->a_stacksize = stacksize;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_setstacksize			     *
 *===========================================================================*/
PUBLIC int mthread_attr_setstacksize(attr, stacksize)
mthread_attr_t *attr;
size_t stacksize;
{
/* Set stack size attribute */
  struct __mthread_attr *a;

  mthread_init();	/* Make sure mthreads is initialized */

  if (attr == NULL) {
  	errno = EINVAL;
  	return(-1);
  }

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr) || stacksize < MTHREAD_STACK_MIN) {
  	errno = EINVAL;
  	return(-1);
  } 

  a->a_stacksize = stacksize;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_remove			     *
 *===========================================================================*/
PRIVATE void mthread_attr_remove(a)
mthread_attr_t *a;
{
/* Remove attribute from list of valid, initialized attributes */

  if ((*a)->prev == NULL)
  	va_front = (*a)->next;
  else
  	(*a)->prev->next = (*a)->next;

  if ((*a)->next == NULL)
  	va_rear = (*a)->prev;
  else
  	(*a)->next->prev = (*a)->prev;
}


/*===========================================================================*
 *				mthread_attr_valid			     *
 *===========================================================================*/
PRIVATE int mthread_attr_valid(a)
mthread_attr_t *a;
{
/* Check to see if attribute is on the list of valid attributes */
  struct __mthread_attr *loopitem;

  mthread_init();	/* Make sure mthreads is initialized */

  loopitem = va_front;

  while (loopitem != NULL) {
  	if (loopitem == *a) 
  		return(1);

  	loopitem = loopitem->next;
  }

  return(0);
}


/*===========================================================================*
 *				mthread_attr_verify			     *
 *===========================================================================*/
#ifdef MDEBUG
PUBLIC int mthread_attr_verify(void)
{
/* Return true when no attributes are in use */
  struct __mthread_attr *loopitem;

  mthread_init();	/* Make sure mthreads is initialized */

  loopitem = va_front;

  while (loopitem != NULL) {
  	loopitem = loopitem->next;
  	return(0);
  }

  return(1);
}
#endif


