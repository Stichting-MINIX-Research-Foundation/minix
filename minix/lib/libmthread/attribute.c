#include <minix/mthread.h>
#include "global.h"
#include "proto.h"

static struct __mthread_attr *va_front, *va_rear;
static void mthread_attr_add(mthread_attr_t *a);
static void mthread_attr_remove(mthread_attr_t *a);
static int mthread_attr_valid(mthread_attr_t *a);

/*===========================================================================*
 *			mthread_init_valid_attributes			     *
 *===========================================================================*/
void mthread_init_valid_attributes(void)
{
/* Initialize list of valid attributs */
  va_front = va_rear = NULL;
}


/*===========================================================================*
 *				mthread_attr_add			     *
 *===========================================================================*/
static void mthread_attr_add(a) 
mthread_attr_t *a;
{
/* Add attribute to list of valid, initialized attributes */

  if (va_front == NULL) {	/* Empty list */
  	va_front = *a;
  	(*a)->ma_prev = NULL;
  } else {
  	va_rear->ma_next = *a;
  	(*a)->ma_prev = va_rear;
  }

  (*a)->ma_next = NULL;
  va_rear = *a;
}


/*===========================================================================*
 *				mthread_attr_destroy			     *
 *===========================================================================*/
int mthread_attr_destroy(attr)
mthread_attr_t *attr;
{
/* Invalidate attribute and deallocate resources. */

  if (attr == NULL)
  	return(EINVAL);

  if (!mthread_attr_valid(attr)) 
  	return(EINVAL);

  /* Valide attribute; invalidate it */
  mthread_attr_remove(attr);	
  free(*attr);
  *attr = NULL;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_init			     *
 *===========================================================================*/
int mthread_attr_init(attr)
mthread_attr_t *attr;	/* Attribute */
{
/* Initialize the attribute to a known state. */
  struct __mthread_attr *a;

  if (attr == NULL) 
  	return(EAGAIN);
  else if (mthread_attr_valid(attr)) 
  	return(EBUSY);

  if ((a = malloc(sizeof(struct __mthread_attr))) == NULL)
  	return(-1);

  a->ma_detachstate = MTHREAD_CREATE_JOINABLE;
  a->ma_stackaddr = NULL;
  a->ma_stacksize = (size_t) 0;

  *attr = (mthread_attr_t) a;
  mthread_attr_add(attr); /* Validate attribute: attribute now in use */

  return(0);
}

/*===========================================================================*
 *				mthread_attr_getdetachstate			     *
 *===========================================================================*/
int mthread_attr_getdetachstate(attr, detachstate)
mthread_attr_t *attr;
int *detachstate;
{
/* Get detachstate of a thread attribute */
  struct __mthread_attr *a;

  if (attr == NULL) 
  	return(EINVAL);

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr)) 
  	return(EINVAL);

  *detachstate = a->ma_detachstate;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_setdetachstate			     *
 *===========================================================================*/
int mthread_attr_setdetachstate(attr, detachstate)
mthread_attr_t *attr;
int detachstate;
{
/* Set detachstate of a thread attribute */
  struct __mthread_attr *a;

  if (attr == NULL) 
  	return(EINVAL);

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr)) 
  	return(EINVAL);
  else if(detachstate != MTHREAD_CREATE_JOINABLE &&
  	  detachstate != MTHREAD_CREATE_DETACHED) 
	return(EINVAL);

  a->ma_detachstate = detachstate;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_getstack			     *
 *===========================================================================*/
int mthread_attr_getstack(attr, stackaddr, stacksize)
mthread_attr_t *attr;
void **stackaddr;
size_t *stacksize;
{
/* Get stack attribute */
  struct __mthread_attr *a;

  if (attr == NULL) 
  	return(EINVAL);

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr))
  	return(EINVAL);

  *stackaddr = a->ma_stackaddr;
  *stacksize = a->ma_stacksize;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_getstacksize		     *
 *===========================================================================*/
int mthread_attr_getstacksize(attr, stacksize)
mthread_attr_t *attr;
size_t *stacksize;
{
/* Get stack size attribute */
  struct __mthread_attr *a;

  if (attr == NULL)
  	return(EINVAL);

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr))
  	return(EINVAL);

  *stacksize = a->ma_stacksize;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_setstack			     *
 *===========================================================================*/
int mthread_attr_setstack(attr, stackaddr, stacksize)
mthread_attr_t *attr;
void *stackaddr;
size_t stacksize;
{
/* Set stack attribute */
  struct __mthread_attr *a;

  if (attr == NULL) 
  	return(EINVAL);

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr) || stacksize < MTHREAD_STACK_MIN) 
  	return(EINVAL);
 
  /* We don't care about address alignment (POSIX standard). The ucontext
   * system calls will make sure that the provided stack will be aligned (at
   * the cost of some memory if needed).
   */

  a->ma_stackaddr = stackaddr;
  a->ma_stacksize = stacksize;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_setstacksize			     *
 *===========================================================================*/
int mthread_attr_setstacksize(attr, stacksize)
mthread_attr_t *attr;
size_t stacksize;
{
/* Set stack size attribute */
  struct __mthread_attr *a;

  if (attr == NULL)
  	return(EINVAL);

  a = (struct __mthread_attr *) *attr;
  if (!mthread_attr_valid(attr) || stacksize < MTHREAD_STACK_MIN) 
	return(EINVAL);

  a->ma_stacksize = stacksize;

  return(0);
}


/*===========================================================================*
 *				mthread_attr_remove			     *
 *===========================================================================*/
static void mthread_attr_remove(a)
mthread_attr_t *a;
{
/* Remove attribute from list of valid, initialized attributes */

  if ((*a)->ma_prev == NULL)
  	va_front = (*a)->ma_next;
  else
  	(*a)->ma_prev->ma_next = (*a)->ma_next;

  if ((*a)->ma_next == NULL)
  	va_rear = (*a)->ma_prev;
  else
  	(*a)->ma_next->ma_prev = (*a)->ma_prev;
}


/*===========================================================================*
 *				mthread_attr_valid			     *
 *===========================================================================*/
static int mthread_attr_valid(a)
mthread_attr_t *a;
{
/* Check to see if attribute is on the list of valid attributes */
  struct __mthread_attr *loopitem;

  loopitem = va_front;

  while (loopitem != NULL) {
  	if (loopitem == *a) 
  		return(1);

  	loopitem = loopitem->ma_next;
  }

  return(0);
}


/*===========================================================================*
 *				mthread_attr_verify			     *
 *===========================================================================*/
#ifdef MDEBUG
int mthread_attr_verify(void)
{
/* Return true when no attributes are in use */
  struct __mthread_attr *loopitem;

  loopitem = va_front;

  while (loopitem != NULL) {
  	loopitem = loopitem->ma_next;
  	return(0);
  }

  return(1);
}
#endif

/* pthread compatibility layer. */
__weak_alias(pthread_attr_destroy, mthread_attr_destroy)
__weak_alias(pthread_attr_getdetachstate, mthread_attr_getdetachstate)
__weak_alias(pthread_attr_getstack, mthread_attr_getstack)
__weak_alias(pthread_attr_getstacksize, mthread_attr_getstacksize)
__weak_alias(pthread_attr_init, mthread_attr_init)
__weak_alias(pthread_attr_setdetachstate, mthread_attr_setdetachstate)
__weak_alias(pthread_attr_setstack, mthread_attr_setstack)
__weak_alias(pthread_attr_setstacksize, mthread_attr_setstacksize)

