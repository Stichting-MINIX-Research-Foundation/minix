#ifndef _BLOCKDRIVER_MQ_H
#define _BLOCKDRIVER_MQ_H

void mq_init(void);
int mq_enqueue(device_id_t device_id, const message *mess, int
	ipc_status);
int mq_dequeue(device_id_t device_id, message *mess, int *ipc_status);

#endif /* _BLOCKDRIVER_MQ_H */
