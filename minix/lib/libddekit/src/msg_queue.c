#include "common.h"

#include <ddekit/memory.h>
#include <ddekit/minix/msg_queue.h>
#include <ddekit/panic.h>
#include <ddekit/semaphore.h>

#define MESSAGE_QUEUE_SIZE 16

#ifdef DDEBUG_LEVEL_MSG_Q
#undef DDEBUG
#define DDEBUG DDEBUG_LEVEL_MSG_Q
#endif

#include "debug.h"

struct ddekit_minix_msg_q {
	
	unsigned from, to;

	message messages[MESSAGE_QUEUE_SIZE];
	int ipc_status[MESSAGE_QUEUE_SIZE];
	ddekit_sem_t *msg_w_sem, *msg_r_sem;
	int msg_r_pos, msg_w_pos;
	
	struct ddekit_minix_msg_q *next;
};

static struct ddekit_minix_msg_q * _list = NULL;
static void _ddekit_minix_queue_msg
                   (struct ddekit_minix_msg_q *mq, message *m, int ipc_status);

/*****************************************************************************
 *      ddekit_minix_create_msg_q                                            *
 ****************************************************************************/
struct ddekit_minix_msg_q *
ddekit_minix_create_msg_q(unsigned from, unsigned to)
{
	struct ddekit_minix_msg_q *mq =  (struct ddekit_minix_msg_q *)
	    ddekit_simple_malloc(sizeof(struct ddekit_minix_msg_q));

	mq->from = from;
	mq->to   = to;
	mq->msg_w_pos = 0;
	mq->msg_r_pos = 0;

	mq->msg_r_sem = ddekit_sem_init(0);
	mq->msg_w_sem = ddekit_sem_init(MESSAGE_QUEUE_SIZE);

	/* TODO: check for overlapping message ranges */
	mq->next = _list;
	_list     = mq;

	DDEBUG_MSG_VERBOSE("created msg_q from %x to %x\n", from , to);

	return mq;
}

/*****************************************************************************
 *      ddekit_minix_deregister_msg_q                                        *
 ****************************************************************************/
void ddekit_minix_deregister_msg_q(struct ddekit_minix_msg_q *mq)
{
	struct ddekit_minix_msg_q *prev =_list, *it;

	for (it = _list->next; it != NULL ; it = it->next) {
		if (it == mq) {
			prev->next = it->next;
			break;
		}
		prev=it;
	}

	ddekit_sem_deinit(mq->msg_r_sem);
	ddekit_sem_deinit(mq->msg_w_sem);

	ddekit_simple_free(mq);

	DDEBUG_MSG_VERBOSE("destroyed msg_q from \n");
}

/*****************************************************************************
 *     _ddekit_minix_queue_msg                                               *
 ****************************************************************************/
static void
_ddekit_minix_queue_msg (
	struct ddekit_minix_msg_q *mq,
	message *m,
	int ipc_status
)
{
	int full;
	full = ddekit_sem_down_try(mq->msg_w_sem);

	if (full) {
		/* Our message queue is full... */
		int result;
		DDEBUG_MSG_WARN("Receive queue is full. Dropping request.\n");

		/* XXX should reply to the sender with EIO or so, but for that
		 * we would need to look at the request and find a suitable
		 * reply code..
		 */
	} else {
		/* queue the message */
		memcpy(&mq->messages[mq->msg_w_pos], m, sizeof(message));
		mq->ipc_status[mq->msg_w_pos] = ipc_status;
		if (++mq->msg_w_pos == MESSAGE_QUEUE_SIZE) {
			mq->msg_w_pos = 0;
		}
		DDEBUG_MSG_VERBOSE("ddekit_minix_queue_msg: queueing msg %x\n",
		                    m->m_type);
		ddekit_sem_up(mq->msg_r_sem);
	}
}

/*****************************************************************************
 *       ddekit_minix_queue_msg                                              *
 ****************************************************************************/
void ddekit_minix_queue_msg(message *m, int ipc_status)
{
	struct ddekit_minix_msg_q *it, *mq = NULL;

	for (it = _list; it !=NULL ; it = it->next) {
		if (m->m_type >= it->from && m->m_type <= it->to) {
			mq = it;
			break;
		}
	}
	if (mq == NULL) {
		DDEBUG_MSG_VERBOSE("no q for msgtype %x\n", m->m_type);
		return;
	}
	_ddekit_minix_queue_msg(mq, m, ipc_status);
}

/*****************************************************************************
 *        ddekit_minix_rcv                                                   *
 ****************************************************************************/
void ddekit_minix_rcv (
	struct ddekit_minix_msg_q *mq,
	message *m,
	int *ipc_status
)
{
	DDEBUG_MSG_VERBOSE("waiting for message");

	ddekit_sem_down(mq->msg_r_sem);

	memcpy(m, &mq->messages[mq->msg_r_pos], sizeof(message));
	*ipc_status = mq->ipc_status[mq->msg_r_pos];
	if (++mq->msg_r_pos == MESSAGE_QUEUE_SIZE) {
		mq->msg_r_pos = 0;
	}

	DDEBUG_MSG_VERBOSE("unqueing message");

	ddekit_sem_up(mq->msg_w_sem);
}
