#ifndef DDEKIT_SRC_MSG_QUEUE_H
#define DDEKIT_SRC_MSG_QUEUE_H

#include <ddekit/ddekit.h>
#include <ddekit/thread.h> 
#include <minix/ipc.h>

struct ddekit_minix_msg_q; 

_PROTOTYPE( void ddekit_minix_queue_msg, (message *m));

_PROTOTYPE( void ddekit_minix_rcv, 
            (struct ddekit_minix_msg_q * mq, message *m));

_PROTOTYPE( struct ddekit_minix_msg_q *ddekit_minix_create_msg_q, 
            (unsigned from, unsigned to));

_PROTOTYPE( void ddekit_minix_destroy_msg_q,
            (struct ddekit_minix_msg_q *mq));


#endif /* DDEKIT_SRC_MSG_QUEUE_H */
