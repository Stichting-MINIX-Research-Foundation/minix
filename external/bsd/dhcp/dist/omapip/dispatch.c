/*	$NetBSD: dispatch.c,v 1.4 2014/07/12 12:09:37 spz Exp $	*/
/* dispatch.c

   I/O dispatcher. */

/*
 * Copyright (c) 2004,2007-2009,2013-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 *
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: dispatch.c,v 1.4 2014/07/12 12:09:37 spz Exp $");

#include "dhcpd.h"

#include <omapip/omapip_p.h>
#include <sys/time.h>

static omapi_io_object_t omapi_io_states;
struct timeval cur_tv;

struct eventqueue *rw_queue_empty;

OMAPI_OBJECT_ALLOC (omapi_io,
		    omapi_io_object_t, omapi_type_io_object)
OMAPI_OBJECT_ALLOC (omapi_waiter,
		    omapi_waiter_object_t, omapi_type_waiter)

void
register_eventhandler(struct eventqueue **queue, void (*handler)(void *))
{
	struct eventqueue *t, *q;

	/* traverse to end of list */
	t = NULL;
	for (q = *queue ; q ; q = q->next) {
		if (q->handler == handler)
			return; /* handler already registered */
		t = q;
	}
		
	q = ((struct eventqueue *)dmalloc(sizeof(struct eventqueue), MDL));
	if (!q)
		log_fatal("register_eventhandler: no memory!");
	memset(q, 0, sizeof *q);
	if (t)
		t->next = q;
	else 
		*queue	= q;
	q->handler = handler;
	return;
}

void
unregister_eventhandler(struct eventqueue **queue, void (*handler)(void *))
{
	struct eventqueue *t, *q;
	
	/* traverse to end of list */
	t= NULL;
	for (q = *queue ; q ; q = q->next) {
		if (q->handler == handler) {
			if (t)
				t->next = q->next;
			else
				*queue = q->next;
			dfree(q, MDL); /* Don't access q after this!*/
			break;
		}
		t = q;
	}
	return;
}

void
trigger_event(struct eventqueue **queue)
{
	struct eventqueue *q;

	for (q=*queue ; q ; q=q->next) {
		if (q->handler) 
			(*q->handler)(NULL);
	}
}

/*
 * Callback routine to connect the omapi I/O object and socket with
 * the isc socket code.  The isc socket code will call this routine
 * which will then call the correct local routine to process the bytes.
 * 
 * Currently we are always willing to read more data, this should be modified
 * so that on connections we don't read more if we already have enough.
 *
 * If we have more bytes to write we ask the library to call us when
 * we can write more.  If we indicate we don't have more to write we need
 * to poke the library via isc_socket_fdwatchpoke.
 */

/*
 * sockdelete indicates if we are deleting the socket or leaving it in place
 * 1 is delete, 0 is leave in place
 */
#define SOCKDELETE 1
static int
omapi_iscsock_cb(isc_task_t   *task,
		 isc_socket_t *socket,
		 void         *cbarg,
		 int           flags)
{
	omapi_io_object_t *obj;
	isc_result_t status;

	/* Get the current time... */
	gettimeofday (&cur_tv, (struct timezone *)0);

	/* isc socket stuff */
#if SOCKDELETE
	/*
	 * walk through the io states list, if our object is on there
	 * service it.  if not ignore it.
	 */
	for (obj = omapi_io_states.next;
	     (obj != NULL) && (obj->next != NULL);
	     obj = obj->next) {
		if (obj == cbarg)
			break;
	}
	if (obj == NULL) {
		return(0);
	}
#else
	/* Not much to be done if we have the wrong type of object. */
	if (((omapi_object_t *)cbarg) -> type != omapi_type_io_object) {
		log_fatal ("Incorrect object type, must be of type io_object");
	}
	obj = (omapi_io_object_t *)cbarg;

	/*
	 * If the object is marked as closed don't try and process
	 * anything just indicate that we don't want any more.
	 *
	 * This should be a temporary fix until we arrange to properly
	 * close the socket.
	 */
	if (obj->closed == ISC_TRUE) {
		return(0);
	}
#endif	  

	if ((flags == ISC_SOCKFDWATCH_READ) &&
	    (obj->reader != NULL) &&
	    (obj->inner != NULL)) {
		status = obj->reader(obj->inner);
		/* 
		 * If we are shutting down (basically tried to
		 * read and got no bytes) we don't need to try
		 * again.
		 */
		if (status == ISC_R_SHUTTINGDOWN)
			return (0);
		/* Otherwise We always ask for more when reading */
		return (1);
	} else if ((flags == ISC_SOCKFDWATCH_WRITE) &&
		 (obj->writer != NULL) &&
		 (obj->inner != NULL)) {
		status = obj->writer(obj->inner);
		/* If the writer has more to write they should return
		 * ISC_R_INPROGRESS */
		if (status == ISC_R_INPROGRESS) {
			return (1);
		}
	}

	/*
	 * We get here if we either had an error (inconsistent
	 * structures etc) or no more to write, tell the socket
	 * lib we don't have more to do right now.
	 */
	return (0);
}

/* Register an I/O handle so that we can do asynchronous I/O on it. */

isc_result_t omapi_register_io_object (omapi_object_t *h,
				       int (*readfd) (omapi_object_t *),
				       int (*writefd) (omapi_object_t *),
				       isc_result_t (*reader)
						(omapi_object_t *),
				       isc_result_t (*writer)
						(omapi_object_t *),
				       isc_result_t (*reaper)
						(omapi_object_t *))
{
	isc_result_t status;
	omapi_io_object_t *obj, *p;
	int fd_flags = 0, fd = 0;

	/* omapi_io_states is a static object.   If its reference count
	   is zero, this is the first I/O handle to be registered, so
	   we need to initialize it.   Because there is no inner or outer
	   pointer on this object, and we're setting its refcnt to 1, it
	   will never be freed. */
	if (!omapi_io_states.refcnt) {
		omapi_io_states.refcnt = 1;
		omapi_io_states.type = omapi_type_io_object;
	}
		
	obj = (omapi_io_object_t *)0;
	status = omapi_io_allocate (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	obj->closed = ISC_FALSE;  /* mark as open */

	status = omapi_object_reference (&obj -> inner, h, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_io_dereference (&obj, MDL);
		return status;
	}

	status = omapi_object_reference (&h -> outer,
					 (omapi_object_t *)obj, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_io_dereference (&obj, MDL);
		return status;
	}

	/*
	 * Attach the I/O object to the isc socket library via the 
	 * fdwatch function.  This allows the socket library to watch
	 * over a socket that we built.  If there are both a read and
	 * a write socket we asssume they are the same socket.
	 */

	if (readfd) {
		fd_flags |= ISC_SOCKFDWATCH_READ;
		fd = readfd(h);
	}

	if (writefd) {
		fd_flags |= ISC_SOCKFDWATCH_WRITE;
		fd = writefd(h);
	}

	if (fd_flags != 0) {
		status = isc_socket_fdwatchcreate(dhcp_gbl_ctx.socketmgr,
						  fd, fd_flags,
						  omapi_iscsock_cb,
						  obj,
						  dhcp_gbl_ctx.task,
						  &obj->fd);
		if (status != ISC_R_SUCCESS) {
			log_error("Unable to register fd with library %s",
				   isc_result_totext(status));

			/*sar*/
			/* is this the cleanup we need? */
			omapi_object_dereference(&h->outer, MDL);
			omapi_io_dereference (&obj, MDL);
			return (status);
		}
	}


	/* Find the last I/O state, if there are any. */
	for (p = omapi_io_states.next;
	     p && p -> next; p = p -> next)
		;
	if (p)
		omapi_io_reference (&p -> next, obj, MDL);
	else
		omapi_io_reference (&omapi_io_states.next, obj, MDL);

	obj -> readfd = readfd;
	obj -> writefd = writefd;
	obj -> reader = reader;
	obj -> writer = writer;
	obj -> reaper = reaper;

	omapi_io_dereference(&obj, MDL);
	return ISC_R_SUCCESS;
}

/*
 * ReRegister an I/O handle so that we can do asynchronous I/O on it.
 * If the handle doesn't exist we call the register routine to build it.
 * If it does exist we change the functions associated with it, and
 * repoke the fd code to make it happy.  Neither the objects nor the
 * fd are allowed to have changed.
 */

isc_result_t omapi_reregister_io_object (omapi_object_t *h,
					 int (*readfd) (omapi_object_t *),
					 int (*writefd) (omapi_object_t *),
					 isc_result_t (*reader)
					 	(omapi_object_t *),
					 isc_result_t (*writer)
					 	(omapi_object_t *),
					 isc_result_t (*reaper)
					 	(omapi_object_t *))
{
	omapi_io_object_t *obj;
	int fd_flags = 0;

	if ((!h -> outer) || (h -> outer -> type != omapi_type_io_object)) {
		/*
		 * If we don't have an object or if the type isn't what 
		 * we expect do the normal registration (which will overwrite
		 * an incorrect type, that's what we did historically, may
		 * want to change that)
		 */
		return (omapi_register_io_object (h, readfd, writefd,
						  reader, writer, reaper));
	}

	/* We have an io object of the correct type, try to update it */
	/*sar*/
	/* Should we validate that the fd matches the previous one?
	 * It's suppossed to, that's a requirement, don't bother yet */

	obj = (omapi_io_object_t *)h->outer;

	obj->readfd = readfd;
	obj->writefd = writefd;
	obj->reader = reader;
	obj->writer = writer;
	obj->reaper = reaper;

	if (readfd) {
		fd_flags |= ISC_SOCKFDWATCH_READ;
	}

	if (writefd) {
		fd_flags |= ISC_SOCKFDWATCH_WRITE;
	}

	isc_socket_fdwatchpoke(obj->fd, fd_flags);
	
	return (ISC_R_SUCCESS);
}

isc_result_t omapi_unregister_io_object (omapi_object_t *h)
{
	omapi_io_object_t *obj, *ph;
#if SOCKDELETE
	omapi_io_object_t *p, *last; 
#endif

	if (!h -> outer || h -> outer -> type != omapi_type_io_object)
		return DHCP_R_INVALIDARG;
	obj = (omapi_io_object_t *)h -> outer;
	ph = (omapi_io_object_t *)0;
	omapi_io_reference (&ph, obj, MDL);

#if SOCKDELETE
	/*
	 * For now we leave this out.  We can't clean up the isc socket
	 * structure cleanly yet so we need to leave the io object in place.
	 * By leaving it on the io states list we avoid it being freed.
	 * We also mark it as closed to avoid using it.
	 */

	/* remove from the list of I/O states */
        last = &omapi_io_states;
	for (p = omapi_io_states.next; p; p = p -> next) {
		if (p == obj) {
			omapi_io_dereference (&last -> next, MDL);
			omapi_io_reference (&last -> next, p -> next, MDL);
			break;
		}
		last = p;
	}
	if (obj -> next)
		omapi_io_dereference (&obj -> next, MDL);
#endif

	if (obj -> outer) {
		if (obj -> outer -> inner == (omapi_object_t *)obj)
			omapi_object_dereference (&obj -> outer -> inner,
						  MDL);
		omapi_object_dereference (&obj -> outer, MDL);
	}
	omapi_object_dereference (&obj -> inner, MDL);
	omapi_object_dereference (&h -> outer, MDL);

#if SOCKDELETE
	/* remove isc socket associations */
	if (obj->fd != NULL) {
		isc_socket_cancel(obj->fd, dhcp_gbl_ctx.task,
				  ISC_SOCKCANCEL_ALL);
		isc_socket_detach(&obj->fd);
	}
#else
	obj->closed = ISC_TRUE;
#endif

	omapi_io_dereference (&ph, MDL);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_dispatch (struct timeval *t)
{
	return omapi_wait_for_completion ((omapi_object_t *)&omapi_io_states,
					  t);
}

isc_result_t omapi_wait_for_completion (omapi_object_t *object,
					struct timeval *t)
{
	isc_result_t status;
	omapi_waiter_object_t *waiter;
	omapi_object_t *inner;

	if (object) {
		waiter = (omapi_waiter_object_t *)0;
		status = omapi_waiter_allocate (&waiter, MDL);
		if (status != ISC_R_SUCCESS)
			return status;

		/* Paste the waiter object onto the inner object we're
		   waiting on. */
		for (inner = object; inner -> inner; inner = inner -> inner)
			;

		status = omapi_object_reference (&waiter -> outer, inner, MDL);
		if (status != ISC_R_SUCCESS) {
			omapi_waiter_dereference (&waiter, MDL);
			return status;
		}
		
		status = omapi_object_reference (&inner -> inner,
						 (omapi_object_t *)waiter,
						 MDL);
		if (status != ISC_R_SUCCESS) {
			omapi_waiter_dereference (&waiter, MDL);
			return status;
		}
	} else
		waiter = (omapi_waiter_object_t *)0;

	do {
		status = omapi_one_dispatch ((omapi_object_t *)waiter, t);
		if (status != ISC_R_SUCCESS)
			return status;
	} while (!waiter || !waiter -> ready);

	if (waiter -> outer) {
		if (waiter -> outer -> inner) {
			omapi_object_dereference (&waiter -> outer -> inner,
						  MDL);
			if (waiter -> inner)
				omapi_object_reference
					(&waiter -> outer -> inner,
					 waiter -> inner, MDL);
		}
		omapi_object_dereference (&waiter -> outer, MDL);
	}
	if (waiter -> inner)
		omapi_object_dereference (&waiter -> inner, MDL);
	
	status = waiter -> waitstatus;
	omapi_waiter_dereference (&waiter, MDL);
	return status;
}

isc_result_t omapi_one_dispatch (omapi_object_t *wo,
				 struct timeval *t)
{
	fd_set r, w, x, rr, ww, xx;
	int max = 0;
	int count;
	int desc;
	struct timeval now, to;
	omapi_io_object_t *io, *prev, *next;
	omapi_waiter_object_t *waiter;
	omapi_object_t *tmp = (omapi_object_t *)0;

	if (!wo || wo -> type != omapi_type_waiter)
		waiter = (omapi_waiter_object_t *)0;
	else
		waiter = (omapi_waiter_object_t *)wo;

	FD_ZERO (&x);

	/* First, see if the timeout has expired, and if so return. */
	if (t) {
		gettimeofday (&now, (struct timezone *)0);
		cur_tv.tv_sec = now.tv_sec;
		cur_tv.tv_usec = now.tv_usec;
		if (now.tv_sec > t -> tv_sec ||
		    (now.tv_sec == t -> tv_sec && now.tv_usec >= t -> tv_usec))
			return ISC_R_TIMEDOUT;
			
		/* We didn't time out, so figure out how long until
		   we do. */
		to.tv_sec = t -> tv_sec - now.tv_sec;
		to.tv_usec = t -> tv_usec - now.tv_usec;
		if (to.tv_usec < 0) {
			to.tv_usec += 1000000;
			to.tv_sec--;
		}

		/* It is possible for the timeout to get set larger than
		   the largest time select() is willing to accept.
		   Restricting the timeout to a maximum of one day should
		   work around this.  -DPN.  (Ref: Bug #416) */
		if (to.tv_sec > (60 * 60 * 24))
			to.tv_sec = 60 * 60 * 24;
	}
	
	/* If the object we're waiting on has reached completion,
	   return now. */
	if (waiter && waiter -> ready)
		return ISC_R_SUCCESS;
	
      again:
	/* If we have no I/O state, we can't proceed. */
	if (!(io = omapi_io_states.next))
		return ISC_R_NOMORE;

	/* Set up the read and write masks. */
	FD_ZERO (&r);
	FD_ZERO (&w);

	for (; io; io = io -> next) {
		/* Check for a read socket.   If we shouldn't be
		   trying to read for this I/O object, either there
		   won't be a readfd function, or it'll return -1. */
		if (io -> readfd && io -> inner &&
		    (desc = (*(io -> readfd)) (io -> inner)) >= 0) {
			FD_SET (desc, &r);
			if (desc > max)
				max = desc;
		}
		
		/* Same deal for write fdets. */
		if (io -> writefd && io -> inner &&
		    (desc = (*(io -> writefd)) (io -> inner)) >= 0) {
			FD_SET (desc, &w);
			if (desc > max)
				max = desc;
		}
	}

	/* poll if all reader are dry */ 
	now.tv_sec = 0;
	now.tv_usec = 0;
	rr=r; 
	ww=w; 
	xx=x;

	/* poll once */
	count = select(max + 1, &r, &w, &x, &now);
	if (!count) {  
		/* We are dry now */ 
		trigger_event(&rw_queue_empty);
		/* Wait for a packet or a timeout... XXX */
		r = rr;
		w = ww;
		x = xx;
		count = select(max + 1, &r, &w, &x, t ? &to : NULL);
	}

	/* Get the current time... */
	gettimeofday (&cur_tv, (struct timezone *)0);

	/* We probably have a bad file descriptor.   Figure out which one.
	   When we find it, call the reaper function on it, which will
	   maybe make it go away, and then try again. */
	if (count < 0) {
		struct timeval t0;
		omapi_io_object_t *prev = (omapi_io_object_t *)0;
		io = (omapi_io_object_t *)0;
		if (omapi_io_states.next)
			omapi_io_reference (&io, omapi_io_states.next, MDL);

		while (io) {
			omapi_object_t *obj;
			FD_ZERO (&r);
			FD_ZERO (&w);
			t0.tv_sec = t0.tv_usec = 0;

			if (io -> readfd && io -> inner &&
			    (desc = (*(io -> readfd)) (io -> inner)) >= 0) {
			    FD_SET (desc, &r);
			    count = select (desc + 1, &r, &w, &x, &t0);
			   bogon:
			    if (count < 0) {
				log_error ("Bad descriptor %d.", desc);
				for (obj = (omapi_object_t *)io;
				     obj -> outer;
				     obj = obj -> outer)
					;
				for (; obj; obj = obj -> inner) {
				    omapi_value_t *ov;
				    int len;
				    const char *s;
				    ov = (omapi_value_t *)0;
				    omapi_get_value_str (obj,
							 (omapi_object_t *)0,
							 "name", &ov);
				    if (ov && ov -> value &&
					(ov -> value -> type ==
					 omapi_datatype_string)) {
					s = (char *)
						ov -> value -> u.buffer.value;
					len = ov -> value -> u.buffer.len;
				    } else {
					s = "";
					len = 0;
				    }
				    log_error ("Object %lx %s%s%.*s",
					       (unsigned long)obj,
					       obj -> type -> name,
					       len ? " " : "",
					       len, s);
				    if (len)
					omapi_value_dereference (&ov, MDL);
				}
				(*(io -> reaper)) (io -> inner);
				if (prev) {
				    omapi_io_dereference (&prev -> next, MDL);
				    if (io -> next)
					omapi_io_reference (&prev -> next,
							    io -> next, MDL);
				} else {
				    omapi_io_dereference
					    (&omapi_io_states.next, MDL);
				    if (io -> next)
					omapi_io_reference
						(&omapi_io_states.next,
						 io -> next, MDL);
				}
				omapi_io_dereference (&io, MDL);
				goto again;
			    }
			}
			
			FD_ZERO (&r);
			FD_ZERO (&w);
			t0.tv_sec = t0.tv_usec = 0;

			/* Same deal for write fdets. */
			if (io -> writefd && io -> inner &&
			    (desc = (*(io -> writefd)) (io -> inner)) >= 0) {
				FD_SET (desc, &w);
				count = select (desc + 1, &r, &w, &x, &t0);
				if (count < 0)
					goto bogon;
			}
			if (prev)
				omapi_io_dereference (&prev, MDL);
			omapi_io_reference (&prev, io, MDL);
			omapi_io_dereference (&io, MDL);
			if (prev -> next)
			    omapi_io_reference (&io, prev -> next, MDL);
		}
		if (prev)
			omapi_io_dereference (&prev, MDL);
		
	}

	for (io = omapi_io_states.next; io; io = io -> next) {
		if (!io -> inner)
			continue;
		omapi_object_reference (&tmp, io -> inner, MDL);
		/* Check for a read descriptor, and if there is one,
		   see if we got input on that socket. */
		if (io -> readfd &&
		    (desc = (*(io -> readfd)) (tmp)) >= 0) {
			if (FD_ISSET (desc, &r))
				((*(io -> reader)) (tmp));
		}
		
		/* Same deal for write descriptors. */
		if (io -> writefd &&
		    (desc = (*(io -> writefd)) (tmp)) >= 0)
		{
			if (FD_ISSET (desc, &w))
				((*(io -> writer)) (tmp));
		}
		omapi_object_dereference (&tmp, MDL);
	}

	/* Now check for I/O handles that are no longer valid,
	   and remove them from the list. */
	prev = NULL;
	io = NULL;
	if (omapi_io_states.next != NULL) {
		omapi_io_reference(&io, omapi_io_states.next, MDL);
	}
	while (io != NULL) {
		if ((io->inner == NULL) || 
		    ((io->reaper != NULL) && 
		     ((io->reaper)(io->inner) != ISC_R_SUCCESS))) 
		{

			omapi_io_object_t *tmp = NULL;
			/* Save a reference to the next
			   pointer, if there is one. */
			if (io->next != NULL) {
				omapi_io_reference(&tmp, io->next, MDL);
				omapi_io_dereference(&io->next, MDL);
			}
			if (prev != NULL) {
				omapi_io_dereference(&prev->next, MDL);
				if (tmp != NULL)
					omapi_io_reference(&prev->next,
							   tmp, MDL);
			} else {
				omapi_io_dereference(&omapi_io_states.next, 
						     MDL);
				if (tmp != NULL)
					omapi_io_reference
					    (&omapi_io_states.next,
					     tmp, MDL);
				else
					omapi_signal_in(
							(omapi_object_t *)
						 	&omapi_io_states,
							"ready");
			}
			if (tmp != NULL)
				omapi_io_dereference(&tmp, MDL);

		} else {

			if (prev != NULL) {
				omapi_io_dereference(&prev, MDL);
			}
			omapi_io_reference(&prev, io, MDL);
		}

		/*
		 * Equivalent to:
		 *   io = io->next
		 * But using our reference counting voodoo.
		 */
		next = NULL;
		if (io->next != NULL) {
			omapi_io_reference(&next, io->next, MDL);
		}
		omapi_io_dereference(&io, MDL);
		if (next != NULL) {
			omapi_io_reference(&io, next, MDL);
			omapi_io_dereference(&next, MDL);
		}
	}
	if (prev != NULL) {
		omapi_io_dereference(&prev, MDL);
	}

	return ISC_R_SUCCESS;
}

isc_result_t omapi_io_set_value (omapi_object_t *h,
				 omapi_object_t *id,
				 omapi_data_string_t *name,
				 omapi_typed_data_t *value)
{
	if (h -> type != omapi_type_io_object)
		return DHCP_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_io_get_value (omapi_object_t *h,
				 omapi_object_t *id,
				 omapi_data_string_t *name,
				 omapi_value_t **value)
{
	if (h -> type != omapi_type_io_object)
		return DHCP_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

/* omapi_io_destroy (object, MDL);
 *
 *	Find the requested IO [object] and remove it from the list of io
 * states, causing the cleanup functions to destroy it.  Note that we must
 * hold a reference on the object while moving its ->next reference and
 * removing the reference in the chain to the target object...otherwise it
 * may be cleaned up from under us.
 */
isc_result_t omapi_io_destroy (omapi_object_t *h, const char *file, int line)
{
	omapi_io_object_t *obj = NULL, *p, *last = NULL, **holder;

	if (h -> type != omapi_type_io_object)
		return DHCP_R_INVALIDARG;
	
	/* remove from the list of I/O states */
	for (p = omapi_io_states.next; p; p = p -> next) {
		if (p == (omapi_io_object_t *)h) {
			omapi_io_reference (&obj, p, MDL);

			if (last)
				holder = &last -> next;
			else
				holder = &omapi_io_states.next;

			omapi_io_dereference (holder, MDL);

			if (obj -> next) {
				omapi_io_reference (holder, obj -> next, MDL);
				omapi_io_dereference (&obj -> next, MDL);
			}

			return omapi_io_dereference (&obj, MDL);
		}
		last = p;
	}

	return ISC_R_NOTFOUND;
}

isc_result_t omapi_io_signal_handler (omapi_object_t *h,
				      const char *name, va_list ap)
{
	if (h -> type != omapi_type_io_object)
		return DHCP_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> signal_handler)
		return (*(h -> inner -> type -> signal_handler)) (h -> inner,
								  name, ap);
	return ISC_R_NOTFOUND;
}

isc_result_t omapi_io_stuff_values (omapi_object_t *c,
				    omapi_object_t *id,
				    omapi_object_t *i)
{
	if (i -> type != omapi_type_io_object)
		return DHCP_R_INVALIDARG;

	if (i -> inner && i -> inner -> type -> stuff_values)
		return (*(i -> inner -> type -> stuff_values)) (c, id,
								i -> inner);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_waiter_signal_handler (omapi_object_t *h,
					  const char *name, va_list ap)
{
	omapi_waiter_object_t *waiter;

	if (h -> type != omapi_type_waiter)
		return DHCP_R_INVALIDARG;
	
	if (!strcmp (name, "ready")) {
		waiter = (omapi_waiter_object_t *)h;
		waiter -> ready = 1;
		waiter -> waitstatus = ISC_R_SUCCESS;
		return ISC_R_SUCCESS;
	}

	if (!strcmp(name, "status")) {
		waiter = (omapi_waiter_object_t *)h;
		waiter->ready = 1;
		waiter->waitstatus = va_arg(ap, isc_result_t);
		return ISC_R_SUCCESS;
	}

	if (!strcmp (name, "disconnect")) {
		waiter = (omapi_waiter_object_t *)h;
		waiter -> ready = 1;
		waiter -> waitstatus = DHCP_R_CONNRESET;
		return ISC_R_SUCCESS;
	}

	if (h -> inner && h -> inner -> type -> signal_handler)
		return (*(h -> inner -> type -> signal_handler)) (h -> inner,
								  name, ap);
	return ISC_R_NOTFOUND;
}

/** @brief calls a given function on every object
 *
 * @param func function to be called
 * @param p parameter to be passed to each function instance
 *
 * @return result (ISC_R_SUCCESS if successful, error code otherwise)
 */
isc_result_t omapi_io_state_foreach (isc_result_t (*func) (omapi_object_t *,
							   void *),
				     void *p)
{
	omapi_io_object_t *io = NULL;
	isc_result_t status;
	omapi_io_object_t *next = NULL;

	/*
	 * This just calls func on every inner object on the list. It would
	 * be much simpler in general case, but one of the operations could be
	 * release of the objects. Therefore we need to ref count the io and
	 * io->next pointers.
	 */

	if (omapi_io_states.next) {
		omapi_object_reference((omapi_object_t**)&io,
				       (omapi_object_t*)omapi_io_states.next,
				       MDL);
	}

	while(io) {
	    /* If there's a next object, save it */
	    if (io->next) {
		omapi_object_reference((omapi_object_t**)&next,
				       (omapi_object_t*)io->next, MDL);
	    }
	    if (io->inner) {
		status = (*func) (io->inner, p);
		if (status != ISC_R_SUCCESS) {
		    /* Something went wrong. Let's stop using io & next pointer
		     * and bail out */
		    omapi_object_dereference((omapi_object_t**)&io, MDL);
		    if (next) {
			omapi_object_dereference((omapi_object_t**)&next, MDL);
		    }
		    return status;
		}
	    }
	    /* Update the io pointer and free the next pointer */
	    omapi_object_dereference((omapi_object_t**)&io, MDL);
	    if (next) {
		omapi_object_reference((omapi_object_t**)&io,
				       (omapi_object_t*)next,
				       MDL);
		omapi_object_dereference((omapi_object_t**)&next, MDL);
	    }
	}

	/*
	 * The only way to get here is when next is NULL. There's no need
	 * to dereference it.
	 */
	return ISC_R_SUCCESS;
}
