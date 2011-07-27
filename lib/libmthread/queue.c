#include <minix/mthread.h>
#include "global.h"
#include "proto.h"

/*===========================================================================*
 *				mthread_queue_add			     *
 *===========================================================================*/
PUBLIC void mthread_queue_add(queue, thread)
mthread_queue_t *queue;		/* Queue we want thread to append to */
mthread_thread_t thread;
{
/* Append a thread to the tail of the queue. As a process can be present on
 * only one queue at the same time, we can use the threads array's 'next'
 * pointer to point to the next thread on the queue.
 */
  mthread_tcb_t *last;

  if (!isokthreadid(thread))
  	mthread_panic("Can't append invalid thread ID to a queue");

  last = mthread_find_tcb(thread);

  if (mthread_queue_isempty(queue)) {
  	queue->mq_head = queue->mq_tail = last;
  } else  {
	queue->mq_tail->m_next = last;
	queue->mq_tail = last;	/* 'last' is the new last in line */
  }
}


/*===========================================================================*
 *				mthread_queue_init			     *
 *===========================================================================*/
PUBLIC void mthread_queue_init(queue)
mthread_queue_t *queue;		/* Queue that has to be initialized */
{
/* Initialize queue to a known state */

  queue->mq_head = queue->mq_tail = NULL;
}


/*===========================================================================*
 *				mthread_queue_isempty			     *
 *===========================================================================*/
PUBLIC int mthread_queue_isempty(queue)
mthread_queue_t *queue;
{
  return(queue->mq_head == NULL);
}


/*===========================================================================*
 *				mthread_dump_queue			     *
 *===========================================================================*/
#ifdef MDEBUG
PUBLIC void mthread_dump_queue(queue)
mthread_queue_t *queue;
{
  int threshold, count = 0;
  mthread_tcb_t *t;
  mthread_thread_t tid;
  threshold = no_threads;
  printf("Dumping queue: ");

  if(queue->mq_head != NULL) {
  	t = queue->mq_head;
	if (t == &mainthread) tid = MAIN_THREAD;
	else tid = t->m_tid;
	printf("%d ", tid);
	count++;
	t = t->m_next; 
	while (t != NULL) {
		if (t == &mainthread) tid = MAIN_THREAD;
		else tid = t->m_tid;
		printf("%d ", tid);
		t = t->m_next; 
		count++;
		if (count > threshold) break;
	}
  } else {
  	printf("[empty]");
  }

  printf("\n");
}
#endif

/*===========================================================================*
 *				mthread_queue_remove			     *
 *===========================================================================*/
PUBLIC mthread_thread_t mthread_queue_remove(queue)
mthread_queue_t *queue;		/* Queue we want a thread from */
{
/* Get the first thread in this queue, if there is one. */
  mthread_thread_t thread;
  mthread_tcb_t *tcb;

  /* Calculate thread id from queue head */
  if (queue->mq_head == NULL) thread = NO_THREAD;
  else if (queue->mq_head == &mainthread) thread = MAIN_THREAD;
  else thread = (queue->mq_head->m_tid);

  if (thread != NO_THREAD) { /* i.e., this queue is not empty */
  	tcb = queue->mq_head;
	if (queue->mq_head == queue->mq_tail) {
		/* Queue holds only one thread */
		queue->mq_head = queue->mq_tail = NULL; /* So mark thread empty */
	} else {
		/* Second thread in line is the new first */
		queue->mq_head = queue->mq_head->m_next;
	}

	tcb->m_next = NULL; /* This thread is no longer part of a queue */
  }

  return(thread);
}

