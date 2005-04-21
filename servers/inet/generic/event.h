/*
inet/generic/event.h

Created:	April 1995 by Philip Homburg <philip@cs.vu.nl>

Header file for an event mechanism.

Copyright 1995 Philip Homburg
*/

#ifndef INET__GENERIC__EVENT_H
#define INET__GENERIC__EVENT_H

struct event;

typedef union ev_arg
{
	int ev_int;
	void *ev_ptr;
} ev_arg_t;

typedef void (*ev_func_t) ARGS(( struct event *ev, union ev_arg eva ));

typedef struct event
{
	ev_func_t ev_func;
	ev_arg_t ev_arg;
	struct event *ev_next;
} event_t;

extern event_t *ev_head;

void ev_init ARGS(( event_t *ev ));
void ev_enqueue ARGS(( event_t *ev, ev_func_t func, ev_arg_t ev_arg ));
void ev_process ARGS(( void ));
int ev_in_queue ARGS(( event_t *ev ));

#endif /* INET__GENERIC__EVENT_H */

/*
 * $PchId: event.h,v 1.4 1995/11/21 06:45:27 philip Exp $
 */
