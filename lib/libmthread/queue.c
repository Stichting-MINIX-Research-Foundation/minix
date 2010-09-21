#include <minix/mthread.h>
#include "global.h"

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

  if (mthread_queue_isempty(queue)) {
	queue->head = queue->tail = thread;
  } else {
	threads[queue->tail].m_next = thread;
	queue->tail = thread; /* 'thread' is the new last in line */
  }
}


/*===========================================================================*
 *				mthread_queue_init			     *
 *===========================================================================*/
PUBLIC void mthread_queue_init(queue)
mthread_queue_t *queue;		/* Queue that has to be initialized */
{
/* Initialize queue to a known state */

  queue->head = queue->tail = NO_THREAD;
}


/*===========================================================================*
 *				mthread_queue_isempty			     *
 *===========================================================================*/
PUBLIC int mthread_queue_isempty(queue)
mthread_queue_t *queue;
{
  return(queue->head == NO_THREAD);
}


/*===========================================================================*
 *				mthread_dump_queue			     *
 *===========================================================================*/
PUBLIC void mthread_dump_queue(queue)
mthread_queue_t *queue;
{
  int threshold, count = 0;
  mthread_thread_t t;
  threshold = no_threads;
#ifdef MDEBUG
  printf("Dumping queue: ");
#endif
  if(queue->head != NO_THREAD) {
  	t = queue->head;
#ifdef MDEBUG
	printf("%d ", t);
#endif
	count++;
	t = threads[t].m_next;
	while (t != NO_THREAD) {
#ifdef MDEBUG
		printf("%d ", t);
#endif
		t = threads[t].m_next;
		count++;
		if (count > threshold) break;
	}
  } else {
#ifdef MDEBUG
  	printf("[empty]");
#endif
  }

#ifdef MDEBUG
  printf("\n");
#endif
}


/*===========================================================================*
 *				mthread_queue_remove			     *
 *===========================================================================*/
PUBLIC mthread_thread_t mthread_queue_remove(queue)
mthread_queue_t *queue;		/* Queue we want a thread from */
{
/* Get the first thread in this queue, if there is one. */
  mthread_thread_t thread = queue->head;

  if (thread != NO_THREAD) { /* i.e., this queue is not empty */
	if (queue->head == queue->tail) /* Queue holds only one thread */
		queue->head = queue->tail = NO_THREAD; /*So mark thread empty*/
	else
		/* Second thread in line is the new first */
		queue->head = threads[thread].m_next;
  }

  return(thread);
}

