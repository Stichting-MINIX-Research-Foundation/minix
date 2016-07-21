#ifndef MINIX_SOCKEVENT_PROC_H
#define MINIX_SOCKEVENT_PROC_H

struct sockevent_proc {
	struct sockevent_proc *spr_next;	/* next on sock or free list */
	unsigned char spr_event;		/* event for call (SEV_) */
	unsigned char spr_timer;		/* suspended call has timer? */
	struct sockdriver_call spr_call;	/* call structure */
	endpoint_t spr_endpt;			/* user endpoint */
	struct sockdriver_packed_data spr_data;	/* regular data, packed */
	size_t spr_datalen;			/* length of regular data */
	size_t spr_dataoff;			/* offset into regular data */
	struct sockdriver_packed_data spr_ctl;	/* control data, packed */
	socklen_t spr_ctllen;			/* length of control data */
	socklen_t spr_ctloff;			/* offset into control data */
	int spr_flags;				/* send/recv flags (MSG_) */
	int spr_rflags;				/* recv result flags (MSG_) */
	clock_t spr_time;			/* timeout time for call */
};

void sockevent_proc_init(void);
struct sockevent_proc *sockevent_proc_alloc(void);
void sockevent_proc_free(struct sockevent_proc *);

#endif /* !MINIX_SOCKEVENT_PROC_H */
