/*
inet/generic/event.c

Created:	April 1995 by Philip Homburg <philip@cs.vu.nl>

Implementation of an event queue.

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "assert.h"
#include "event.h"

THIS_FILE

event_t *ev_head;
static event_t *ev_tail;

void ev_init(ev)
event_t *ev;
{
	ev->ev_func= 0;
	ev->ev_next= NULL;
}

void ev_enqueue(ev, func, ev_arg)
event_t *ev;
ev_func_t func;
ev_arg_t ev_arg;
{
	assert(ev->ev_func == 0);
	ev->ev_func= func;
	ev->ev_arg= ev_arg;
	ev->ev_next= NULL;
	if (ev_head == NULL)
		ev_head= ev;
	else
		ev_tail->ev_next= ev;
	ev_tail= ev;
}

void ev_process()
{
	ev_func_t func;
	event_t *curr;

	while (ev_head)
	{
		curr= ev_head;
		ev_head= curr->ev_next;
		func= curr->ev_func;
		curr->ev_func= 0;

		assert(func != 0);
		func(curr, curr->ev_arg);
	}
}

int ev_in_queue(ev)
event_t *ev;
{
	return ev->ev_func != 0;
}


/*
 * $PchId: event.c,v 1.4 1995/11/21 06:45:27 philip Exp $
 */
