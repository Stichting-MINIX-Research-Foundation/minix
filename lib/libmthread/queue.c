#include <minix/mthread.h>
#include "global.h"
#include "proto.h"

/*===========================================================================*
 *				mthread_queue_add			     *
 *===========================================================================*/
void mthread_queue_add(queue, thread)
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
void mthread_queue_init(queue)
mthread_queue_t *queue;		/* Queue that has to be initialized */
{
/* Initialize queue to a known state */

  queue->mq_head = queue->mq_tail = NULL;
}


/*===========================================================================*
 *				mthread_queue_isempty			     *
 *===========================================================================*/
int mthread_queue_isempty(queue)
mthread_queue_t *queue;
{
  return(queue->mq_head == NULL);
}


/*===========================================================================*
 *				mthread_dump_queue			     *
 *===========================================================================*/
#ifdef MDEBUG
void mthread_dump_queue(queue)
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
mthread_thread_t mthread_queue_remove(queue)
mthread_queue_t *queue;		/* Queue we want a thread from */
{
/* Get the first thread in this queue, if there is one. */
  mthread_thread_t thread;
  mthread_tcb_t *tcb, *random_tcb, *prev;
  int count = 0, offset_id = 0, picked_random = 0;

  tcb = queue->mq_head;

  if (MTHREAD_RND_SCHED) {
	/* Count items on queue */
	random_tcb = queue->mq_head;
	if (random_tcb != NULL) {
		do {
			count++;
			random_tcb = random_tcb->m_next;
		} while (random_tcb != NULL);
	}

	if (count > 1) {
		picked_random = 1;

		/* Get random offset */
		offset_id = random() % count;

		/* Find offset in queue */
		random_tcb = queue->mq_head;
		prev = random_tcb;
		while (--offset_id > 0) {
			prev = random_tcb;
			random_tcb = random_tcb->m_next;
		}

		/* Stitch head and tail together */
		prev->m_next = random_tcb->m_next;

		/* Fix head and tail */
		if (queue->mq_head == random_tcb)
			queue->mq_head = random_tcb->m_next;
		if (queue->mq_tail == random_tcb)
			queue->mq_tail = prev;

		tcb = random_tcb;
	}
  }

  /* Retrieve thread id from tcb */
  if (tcb == NULL) thread = NO_THREAD;
  else if (tcb == &mainthread) thread = MAIN_THREAD;
  else thread = (tcb->m_tid);

  /* If we didn't pick a random thread and queue is not empty... */
  if (!picked_random && thread != NO_THREAD) {
  	tcb = queue->mq_head;
	if (queue->mq_head == queue->mq_tail) {
		/* Queue holds only one thread */
		queue->mq_head = queue->mq_tail = NULL; /* So mark thread empty */
	} else {
		/* Second thread in line is the new first */
		queue->mq_head = queue->mq_head->m_next;
	}
  }

  if (tcb != NULL)
	tcb->m_next = NULL; /* This thread is no longer part of a queue */

  return(thread);
}

