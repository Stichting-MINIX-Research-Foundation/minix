/*	$NetBSD: iscsi_ioctl.c,v 1.13 2015/09/19 18:32:42 dholland Exp $	*/

/*-
 * Copyright (c) 2004,2005,2006,2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "iscsi_globals.h"

#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/proc.h>

#ifndef ISCSI_MINIMAL
#include <uvm/uvm.h>
#include <uvm/uvm_pmap.h>
#endif

static uint16_t current_id = 0;	/* Global session ID counter */

/* list of event handlers */
static event_handler_list_t event_handlers =
	TAILQ_HEAD_INITIALIZER(event_handlers);

static uint32_t handler_id = 0;	/* Handler ID counter */

/* -------------------------------------------------------------------------- */

/* Event management functions */

/*
 * find_handler:
 *    Search the event handler list for the given ID.
 *
 *    Parameter:
 *          id    The handler ID.
 *
 *    Returns:
 *          Pointer to handler if found, else NULL.
 */


STATIC event_handler_t *
find_handler(uint32_t id)
{
	event_handler_t *curr;

	TAILQ_FOREACH(curr, &event_handlers, link)
		if (curr->id == id)
			break;

	return curr;
}


/*
 * register_event:
 *    Create event handler entry, return ID.
 *
 *    Parameter:
 *          par   The parameter.
 */

STATIC void
register_event(iscsi_register_event_parameters_t *par)
{
	event_handler_t *handler;
	int was_empty;
	int s;

	handler = malloc(sizeof(event_handler_t), M_DEVBUF, M_WAITOK | M_ZERO);
	if (handler == NULL) {
		DEBOUT(("No mem for event handler\n"));
		par->status = ISCSI_STATUS_NO_RESOURCES;
		return;
	}

	TAILQ_INIT(&handler->events);

	/* create a unique ID */
	s = splbio();
	do {
		++handler_id;
	} while (!handler_id || find_handler(handler_id) != NULL);
	par->event_id = handler->id = handler_id;

	was_empty = TAILQ_FIRST(&event_handlers) == NULL;

	TAILQ_INSERT_TAIL(&event_handlers, handler, link);

	if (was_empty) {
		wakeup(&iscsi_cleanupc_list);
	}
	splx(s);

	par->status = ISCSI_STATUS_SUCCESS;
	DEB(5, ("Register Event OK, ID %d\n", par->event_id));
}


/*
 * deregister_event:
 *    Destroy handler entry and any waiting events, wake up waiter.
 *
 *    Parameter:
 *          par   The parameter.
 */

STATIC void
deregister_event(iscsi_register_event_parameters_t *par)
{
	event_handler_t *handler;
	event_t *evt;
	int s;

	handler = find_handler(par->event_id);
	if (handler == NULL) {
		DEB(1, ("Deregister Event ID %d not found\n", par->event_id));
		par->status = ISCSI_STATUS_INVALID_EVENT_ID;
		return;
	}

	s = splbio();
	TAILQ_REMOVE(&event_handlers, handler, link);
	splx(s);

	if (handler->waiter != NULL) {
		handler->waiter->status = ISCSI_STATUS_EVENT_DEREGISTERED;
		wakeup(handler->waiter);
	}

	while ((evt = TAILQ_FIRST(&handler->events)) != NULL) {
		TAILQ_REMOVE(&handler->events, evt, link);
		free(evt, M_TEMP);
	}

	free(handler, M_DEVBUF);
	par->status = ISCSI_STATUS_SUCCESS;
	DEB(5, ("Deregister Event ID %d complete\n", par->event_id));
}


/*
 * check_event:
 *    Return first queued event. Optionally wait for arrival of event.
 *
 *    Parameter:
 *          par   The parameter.
 *          wait  Wait for event if true
 */

STATIC void
check_event(iscsi_wait_event_parameters_t *par, bool wait)
{
	event_handler_t *handler;
	event_t *evt;

	handler = find_handler(par->event_id);
	if (handler == NULL) {
		DEBOUT(("Wait Event ID %d not found\n", par->event_id));
		par->status = ISCSI_STATUS_INVALID_EVENT_ID;
		return;
	}
	if (handler->waiter != NULL) {
		DEBOUT(("Wait Event ID %d already waiting\n", par->event_id));
		par->status = ISCSI_STATUS_EVENT_WAITING;
		return;
	}
	par->status = ISCSI_STATUS_SUCCESS;
	DEB(99, ("Wait Event ID %d\n", par->event_id));

	do {
		int s = splbio();
		evt = TAILQ_FIRST(&handler->events);
		if (evt != NULL) {
			TAILQ_REMOVE(&handler->events, evt, link);
			splx(s);
		} else {
			if (!wait) {
				splx(s);
				par->status = ISCSI_STATUS_LIST_EMPTY;
				return;
			}
			if (par->status != ISCSI_STATUS_SUCCESS) {
				splx(s);
				return;
			}
			handler->waiter = par;
			splx(s);
			if (tsleep(par, PRIBIO | PCATCH, "iscsievtwait", 0))
				return;
		}
	} while (evt == NULL);

	par->connection_id = evt->connection_id;
	par->session_id = evt->session_id;
	par->event_kind = evt->event_kind;
	par->reason = evt->reason;

	free(evt, M_TEMP);
}

/*
 * add_event
 *    Adds an event entry to each registered handler queue.
 *    Note that events are simply duplicated because we expect the number of
 *    handlers to be very small, usually 1 (the daemon).
 *
 *    Parameters:
 *       kind     The event kind
 *       sid      The ID of the affected session
 *       cid      The ID of the affected connection
 *       reason   The reason code
 */

void
add_event(iscsi_event_t kind, uint32_t sid, uint32_t cid, uint32_t reason)
{
	event_handler_t *curr;
	event_t *evt;
	int s;

	DEB(9, ("Add_event kind %d, sid %d, cid %d, reason %d\n",
		kind, sid, cid, reason));

	s = splbio();
	TAILQ_FOREACH(curr, &event_handlers, link) {
		evt = malloc(sizeof(*evt), M_TEMP, M_WAITOK);
		if (evt == NULL) {
			panic("iSCSI: add_event failed to alloc memory");
		}
		evt->event_kind = kind;
		evt->session_id = sid;
		evt->connection_id = cid;
		evt->reason = reason;

		TAILQ_INSERT_TAIL(&curr->events, evt, link);
		if (curr->waiter != NULL) {
			wakeup(curr->waiter);
			curr->waiter = NULL;
		}
	}
	splx(s);
}


/*
 * check_event_handlers
 *    Checks for dead event handlers. A dead event handler would deplete
 *    memory over time, so we have to make sure someone at the other
 *    end is actively monitoring events.
 *    This function is called every 30 seconds or so (less frequent if there
 *    is other activity for the cleanup thread to deal with) to go through
 *    the list of handlers and check whether the first element in the event
 *    list has changed at all. If not, the event is deregistered.
 *    Note that this will not detect dead handlers if no events are pending,
 *    but we don't care as long as events don't accumulate in the list.
 *
 *    this function must be called at splbio
 */

STATIC void
check_event_handlers(void)
{
	event_handler_t *curr, *next;
	event_t *evt;

	for (curr = TAILQ_FIRST(&event_handlers); curr != NULL; curr = next) {
		next = TAILQ_NEXT(curr, link);
		evt = TAILQ_FIRST(&curr->events);

		if (evt != NULL && evt == curr->first_in_list) {
			DEBOUT(("Found Dead Event Handler %d, removing\n", curr->id));

			TAILQ_REMOVE(&event_handlers, curr, link);
			while ((evt = TAILQ_FIRST(&curr->events)) != NULL) {
				TAILQ_REMOVE(&curr->events, evt, link);
				free(evt, M_TEMP);
			}
			free(curr, M_DEVBUF);
		} else
			curr->first_in_list = evt;
	}
}


/* -------------------------------------------------------------------------- */

/*
 * get_socket:
 *    Get the file pointer from the socket handle passed into login.
 *
 *    Parameter:
 *          fdes     IN: The socket handle
 *          fpp      OUT: The pointer to the resulting file pointer
 *
 *    Returns:    0 on success, else an error code.
 *
 */

STATIC int
get_socket(int fdes, struct file **fpp)
{
	struct file *fp;

	if ((fp = fd_getfile(fdes)) == NULL) {
		return EBADF;
	}
	if (fp->f_type != DTYPE_SOCKET) {
		return ENOTSOCK;
	}

	/* Add the reference */
	mutex_enter(&fp->f_lock);
	fp->f_count++;
	mutex_exit(&fp->f_lock);

	*fpp = fp;
	return 0;
}

/*
 * release_socket:
 *    Release the file pointer from the socket handle passed into login.
 *
 *    Parameter:
 *          fp       IN: The pointer to the resulting file pointer
 *
 */

STATIC void
release_socket(struct file *fp)
{
	/* Add the reference */
	mutex_enter(&fp->f_lock);
	fp->f_count--;
	mutex_exit(&fp->f_lock);
}


/*
 * find_session:
 *    Find a session by ID.
 *
 *    Parameter:  the session ID
 *
 *    Returns:    The pointer to the session (or NULL if not found)
 */

session_t *
find_session(uint32_t id)
{
	session_t *curr;
	int s;

	s = splbio();
	TAILQ_FOREACH(curr, &iscsi_sessions, sessions)
		if (curr->id == id) {
			break;
		}
	splx(s);
	return curr;
}


/*
 * find_connection:
 *    Find a connection by ID.
 *
 *    Parameter:  the session pointer and the connection ID
 *
 *    Returns:    The pointer to the connection (or NULL if not found)
 */

connection_t *
find_connection(session_t *session, uint32_t id)
{
	connection_t *curr;
	int s;

	s = splbio();
	TAILQ_FOREACH(curr, &session->conn_list, connections)
		if (curr->id == id) {
			break;
		}
	splx(s);
	return curr;
}


/*
 * kill_connection:
 *    Terminate the connection as gracefully as possible.
 *
 *    Parameter:
 *		conn		The connection to terminate
 *		status		The status code for the connection's "terminating" field
 *		logout		The logout reason code
 *		recover	Attempt to recover connection
 */

void
kill_connection(connection_t *conn, uint32_t status, int logout, bool recover)
{
	session_t *sess = conn->session;
	int s;

	DEBC(conn, 1, ("Kill_connection: terminating=%d, status=%d, logout=%d, "
			   "state=%d\n",
			   conn->terminating, status, logout, conn->state));

	if (recover &&
	    !conn->destroy &&
	    conn->recover > MAX_RECOVERY_ATTEMPTS) {
		DEBC(conn, 1,
			  ("Kill_connection: Too many recovery attempts, destroying\n"));
		conn->destroy = TRUE;
	}

	if (!recover || conn->destroy) {

		s = splbio();
		if (conn->in_session) {
			conn->in_session = FALSE;
			TAILQ_REMOVE(&sess->conn_list, conn, connections);
			sess->mru_connection = TAILQ_FIRST(&sess->conn_list);
		}
		splx(s);

		if (!conn->destroy) {
			DEBC(conn, 1, ("Kill_connection setting destroy flag\n"));
			conn->destroy = TRUE;
		}
		/* in case it was already terminated earlier and rcv/send-threads */
		/* are waiting */
		wakeup(conn);
	}

	/* Don't recurse */
	if (conn->terminating) {
		DEBC(conn, 1, ("Kill_connection exiting (already terminating)\n"));
		return;
	}

	if (conn->state == ST_FULL_FEATURE) {
		sess->active_connections--;

		/* If this is the last connection and ERL < 2, reset TSIH */
		if (!sess->active_connections && sess->ErrorRecoveryLevel < 2)
			sess->TSIH = 0;

		/* Don't try to log out if the socket is broken or we're in the middle */
		/* of logging in */
		if (logout >= 0) {
			conn->state = ST_WINDING_DOWN;
			callout_schedule(&conn->timeout, CONNECTION_TIMEOUT);

			if (sess->ErrorRecoveryLevel < 2 &&
			    logout == RECOVER_CONNECTION) {
				logout = LOGOUT_CONNECTION;
			}
			if (!sess->active_connections &&
			    logout == LOGOUT_CONNECTION) {
				logout = LOGOUT_SESSION;
			}
			if (!send_logout(conn, conn, logout, FALSE)) {
				return;
			}
			/*
			 * if the logout request was successfully sent, the logout response
			 * handler will do the rest of the termination processing. If the
			 * logout doesn't get a response, we'll get back in here once
			 * the timeout hits.
			 */
		}
	}

	conn->terminating = status;
	conn->state = ST_SETTLING;

	/* let send thread take over next step of cleanup */
	wakeup(&conn->pdus_to_send);

	DEBC(conn, 5, ("kill_connection returns\n"));
}


/*
 * kill_session:
 *    Terminate the session as gracefully as possible.
 *
 *    Parameter:
 *		session		Session to terminate
 *		status		The status code for the termination
 *		logout		The logout reason code

 */

void
kill_session(session_t *session, uint32_t status, int logout, bool recover)
{
	connection_t *curr;
	ccb_t *ccb;
	int s;

	DEB(1, ("ISCSI: kill_session %d, status %d, logout %d, recover %d\n",
			session->id, status, logout, recover));

	/*
	 * don't do anything if session isn't established yet, termination will be
	 * handled elsewhere
	 */
	if (session->sessions.tqe_next == NULL &&
	    session->sessions.tqe_prev == NULL) {
		return;
	}

	if (recover) {
		/*
		 * Only recover when there's just one active connection left.
		 * Otherwise we get in all sorts of timing problems, and it doesn't
		 * make much sense anyway to recover when the other side has
		 * requested that we kill a multipathed session.
		 */
		if (session->active_connections == 1) {
			curr = assign_connection(session, FALSE);
			if (curr != NULL)
				kill_connection(curr, status, logout, TRUE);
		}
		/* don't allow the session to disappear when the target */
		/* requested the logout */
		return;
	}

	/* remove from session list */
	s = splbio();
	TAILQ_REMOVE(&iscsi_sessions, session, sessions);
	splx(s);
	session->sessions.tqe_next = NULL;
	session->sessions.tqe_prev = NULL;

	/* complete any throttled CCBs */
	s = splbio();
	while ((ccb = TAILQ_FIRST(&session->ccbs_throttled)) != NULL) {
		throttle_ccb(ccb, FALSE);
		splx(s);
		wake_ccb(ccb, ISCSI_STATUS_LOGOUT);
		s = splbio();
	}
	splx(s);

	/*
	 * unmap first to give the system an opportunity to flush its buffers,
	 * but don't try to unmap if it's a forced termination (connection is dead)
	 * to avoid waiting for pending commands that can't complete anyway.
	 */
	if (logout >= 0) {
		unmap_session(session);
		DEB(5, ("Unmap Returns\n"));
	}

	/* kill all connections */
	while ((curr = TAILQ_FIRST(&session->conn_list)) != NULL) {
		kill_connection(curr, status, logout, FALSE);
		logout = NO_LOGOUT;
	}
}


/*
 * create_connection:
 *    Create and init the necessary framework for a connection:
 *       Alloc the connection structure itself
 *       Copy connection parameters
 *       Create the send and receive threads
 *       And finally, log in.
 *
 *    Parameter:
 *          par      IN/OUT: The login parameters
 *          session  IN: The owning session
 *          l        IN: The lwp pointer of the caller
 *
 *    Returns:    0 on success
 *                >0 on failure, connection structure deleted
 *                <0 on failure, connection is still terminating
 */

STATIC int
create_connection(iscsi_login_parameters_t *par, session_t *session,
				  struct lwp *l)
{
	connection_t *connection;
	int rc, s;

	DEB(1, ("Create Connection for Session %d\n", session->id));

	if (session->MaxConnections &&
	    session->active_connections >= session->MaxConnections) {
		DEBOUT(("Too many connections (max = %d, curr = %d)\n",
				session->MaxConnections, session->active_connections));
		par->status = ISCSI_STATUS_MAXED_CONNECTIONS;
		return EIO;
	}

	connection = malloc(sizeof(*connection), M_DEVBUF, M_WAITOK | M_ZERO);
	if (connection == NULL) {
		DEBOUT(("No mem for connection\n"));
		par->status = ISCSI_STATUS_NO_RESOURCES;
		return EIO;
	}

	/* create a unique ID */
	do {
		++session->conn_id;
	} while (!session->conn_id ||
		 find_connection(session, session->conn_id) != NULL);

	par->connection_id = connection->id = session->conn_id;
	DEB(99, ("Connection ID = %d\n", connection->id));

	connection->session = session;

	TAILQ_INIT(&connection->ccbs_waiting);
	TAILQ_INIT(&connection->pdus_to_send);
	TAILQ_INIT(&connection->pdu_pool);

	callout_init(&connection->timeout, 0);
	callout_setfunc(&connection->timeout, connection_timeout, connection);
	connection->idle_timeout_val = CONNECTION_IDLE_TIMEOUT;

	init_sernum(&connection->StatSN_buf);
	create_pdus(connection);

	if ((rc = get_socket(par->socket, &connection->sock)) != 0) {
		DEBOUT(("Invalid socket %d\n", par->socket));

		free(connection, M_DEVBUF);
		par->status = ISCSI_STATUS_INVALID_SOCKET;
		return rc;
	}
	DEBC(connection, 1, ("get_socket: par_sock=%d, fdesc=%p\n",
			par->socket, connection->sock));

	/* close the file descriptor */
	fd_close(par->socket);

	connection->threadobj = l;
	connection->login_par = par;

	/*DEBOUT (("Creating receive thread\n")); */
	if ((rc = kthread_create(PRI_NONE, 0, NULL, iscsi_rcv_thread,
				connection, &connection->rcvproc,
				"ConnRcv")) != 0) {
		DEBOUT(("Can't create rcv thread (rc %d)\n", rc));

		release_socket(connection->sock);
		free(connection, M_DEVBUF);
		par->status = ISCSI_STATUS_NO_RESOURCES;
		return rc;
	}
	/*DEBOUT (("Creating send thread\n")); */
	if ((rc = kthread_create(PRI_NONE, 0, NULL, iscsi_send_thread,
				connection, &connection->sendproc,
				"ConnSend")) != 0) {
		DEBOUT(("Can't create send thread (rc %d)\n", rc));

		connection->terminating = ISCSI_STATUS_NO_RESOURCES;

		/*
		 * We must close the socket here to force the receive
		 * thread to wake up
		 */
		DEBC(connection, 1,
			("Closing Socket %p\n", connection->sock));
		mutex_enter(&connection->sock->f_lock);
		connection->sock->f_count += 1;
		mutex_exit(&connection->sock->f_lock);
		closef(connection->sock);

		/* give receive thread time to exit */
		tsleep(connection, PWAIT, "settle", 2 * hz);

		release_socket(connection->sock);
		free(connection, M_DEVBUF);
		par->status = ISCSI_STATUS_NO_RESOURCES;
		return rc;
	}

	/*
	 * At this point, each thread will tie 'sock' into its own file descriptor
	 * tables w/o increasing the use count - they will inherit the use
	 * increments performed in get_socket().
	 */

	if ((rc = send_login(connection)) != 0) {
		DEBC(connection, 0, ("Login failed (rc %d)\n", rc));
		/* Don't attempt to recover, there seems to be something amiss */
		kill_connection(connection, rc, NO_LOGOUT, FALSE);
		par->status = rc;
		return -1;
	}

	s = splbio();
	connection->state = ST_FULL_FEATURE;
	TAILQ_INSERT_TAIL(&session->conn_list, connection, connections);
	connection->in_session = TRUE;
	session->total_connections++;
	session->active_connections++;
	session->mru_connection = connection;
	splx(s);

	DEBC(connection, 5, ("Connection created successfully!\n"));
	return 0;
}


/*
 * recreate_connection:
 *    Revive dead connection
 *
 *    Parameter:
 *          par      IN/OUT: The login parameters
 *          conn     IN: The connection
 *          l        IN: The lwp pointer of the caller
 *
 *    Returns:    0 on success
 *                >0 on failure, connection structure deleted
 *                <0 on failure, connection is still terminating
 */

STATIC int
recreate_connection(iscsi_login_parameters_t *par, session_t *session,
					connection_t *connection, struct lwp *l)
{
	int rc, s;
	ccb_t *ccb;
	ccb_list_t old_waiting;

	DEB(1, ("ReCreate Connection %d for Session %d, ERL=%d\n",
		connection->id, connection->session->id,
		connection->session->ErrorRecoveryLevel));

	if (session->MaxConnections &&
	    session->active_connections >= session->MaxConnections) {
		DEBOUT(("Too many connections (max = %d, curr = %d)\n",
			session->MaxConnections, session->active_connections));
		par->status = ISCSI_STATUS_MAXED_CONNECTIONS;
		return EIO;
	}

	/* close old socket */
	if (connection->sock != NULL) {
		closef(connection->sock);
		connection->sock = NULL;
	}

	if ((rc = get_socket(par->socket, &connection->sock)) != 0) {
		DEBOUT(("Invalid socket %d\n", par->socket));
		par->status = ISCSI_STATUS_INVALID_SOCKET;
		return rc;
	}
	DEBC(connection, 1, ("get_socket: par_sock=%d, fdesc=%p\n",
			par->socket, connection->sock));

	/* close the file descriptor */
	fd_close(par->socket);

	connection->threadobj = l;
	connection->login_par = par;
	connection->terminating = ISCSI_STATUS_SUCCESS;
	connection->recover++;
	connection->num_timeouts = 0;
	connection->state = ST_SEC_NEG;
	connection->HeaderDigest = 0;
	connection->DataDigest = 0;

	session->active_connections++;

	TAILQ_INIT(&old_waiting);
	s = splbio();
	TAILQ_CONCAT(&old_waiting, &connection->ccbs_waiting, chain);
	splx(s);

	init_sernum(&connection->StatSN_buf);
	wakeup(connection);

	if ((rc = send_login(connection)) != 0) {
		DEBOUT(("Login failed (rc %d)\n", rc));
		while ((ccb = TAILQ_FIRST(&old_waiting)) != NULL) {
			TAILQ_REMOVE(&old_waiting, ccb, chain);
			wake_ccb(ccb, rc);
		}
		/* Don't attempt to recover, there seems to be something amiss */
		kill_connection(connection, rc, NO_LOGOUT, FALSE);
		par->status = rc;
		return -1;
	}

	DEBC(connection, 9, ("Re-Login successful\n"));
	par->status = ISCSI_STATUS_SUCCESS;

	s = splbio();
	connection->state = ST_FULL_FEATURE;
	session->mru_connection = connection;
	splx(s);

	while ((ccb = TAILQ_FIRST(&old_waiting)) != NULL) {
		TAILQ_REMOVE(&old_waiting, ccb, chain);
		s = splbio();
		suspend_ccb(ccb, TRUE);
		splx(s);

		rc = send_task_management(connection, ccb, NULL, TASK_REASSIGN);
		/* if we get an error on reassign, restart the original request */
		if (rc && ccb->pdu_waiting != NULL) {
			if (ccb->CmdSN < session->ExpCmdSN) {
				pdu_t *pdu = ccb->pdu_waiting;

				/* update CmdSN */
				DEBC(connection, 1, ("Resend Updating CmdSN - old %d, new %d\n",
					   ccb->CmdSN, session->CmdSN));
				ccb->CmdSN = session->CmdSN;
				if (!(pdu->pdu.Opcode & OP_IMMEDIATE))
					session->CmdSN++;
				pdu->pdu.p.command.CmdSN = htonl(ccb->CmdSN);
			}
			resend_pdu(ccb);
		} else {
			callout_schedule(&ccb->timeout, COMMAND_TIMEOUT);
		}
	}

	wakeup(session);

	DEBC(connection, 5, ("Connection ReCreated successfully - status %d\n",
						 par->status));

	return 0;
}

/* -------------------------------------------------------------------------- */

/*
 * check_login_pars:
 *    Check the parameters passed into login/add_connection
 *    for validity and consistency.
 *
 *    Parameter:
 *          par      The login parameters
 *
 *    Returns:    0 on success, else an error code.
 */

STATIC int
check_login_pars(iscsi_login_parameters_t *par)
{
	int i, n;

	if (par->is_present.auth_info) {
		/* check consistency of authentication parameters */

		if (par->auth_info.auth_number > ISCSI_AUTH_OPTIONS) {
			DEBOUT(("Auth number invalid: %d\n", par->auth_info.auth_number));
			return ISCSI_STATUS_PARAMETER_INVALID;
		}

		if (par->auth_info.auth_number > 2) {
			DEBOUT(("Auth number invalid: %d\n", par->auth_info.auth_number));
			return ISCSI_STATUS_NOTIMPL;
		}

		for (i = 0, n = 0; i < par->auth_info.auth_number; i++) {
#if 0
			if (par->auth_info.auth_type[i] < ISCSI_AUTH_None) {
				DEBOUT(("Auth type invalid: %d\n",
						par->auth_info.auth_type[i]));
				return ISCSI_STATUS_PARAMETER_INVALID;
			}
#endif
			if (par->auth_info.auth_type[i] > ISCSI_AUTH_CHAP) {
				DEBOUT(("Auth type invalid: %d\n",
						par->auth_info.auth_type[i]));
				return ISCSI_STATUS_NOTIMPL;
			}
			n = max(n, par->auth_info.auth_type[i]);
		}
		if (n) {
			if (!par->is_present.password ||
				(par->auth_info.mutual_auth &&
				 !par->is_present.target_password)) {
				DEBOUT(("Password missing\n"));
				return ISCSI_STATUS_PARAMETER_MISSING;
			}
			/* Note: Default for user-name is initiator name */
		}
	}
	if (par->login_type != ISCSI_LOGINTYPE_DISCOVERY &&
	    !par->is_present.TargetName) {
		DEBOUT(("Target name missing, login type %d\n", par->login_type));
		return ISCSI_STATUS_PARAMETER_MISSING;
	}
	if (par->is_present.MaxRecvDataSegmentLength) {
		if (par->MaxRecvDataSegmentLength < 512 ||
			par->MaxRecvDataSegmentLength > 0xffffff) {
			DEBOUT(("MaxRecvDataSegmentLength invalid: %d\n",
					par->MaxRecvDataSegmentLength));
			return ISCSI_STATUS_PARAMETER_INVALID;
		}
	}
	return 0;
}


/*
 * login:
 *    Handle the login ioctl - Create a session:
 *       Alloc the session structure
 *       Copy session parameters
 *       And call create_connection to establish the connection.
 *
 *    Parameter:
 *          par      IN/OUT: The login parameters
 *          l        IN: The lwp pointer of the caller
 */

STATIC void
login(iscsi_login_parameters_t *par, struct lwp *l)
{
	session_t *session;
	int rc, s;

	DEB(99, ("ISCSI: login\n"));

	if (!iscsi_InitiatorName[0]) {
		DEB(1, ("No Initiator Name\n"));
		par->status = ISCSI_STATUS_NO_INITIATOR_NAME;
		return;
	}

	if ((par->status = check_login_pars(par)) != 0)
		return;

	/* alloc the session */
	session = malloc(sizeof(*session), M_DEVBUF, M_WAITOK | M_ZERO);
	if (session == NULL) {
		DEBOUT(("No mem for session\n"));
		par->status = ISCSI_STATUS_NO_RESOURCES;
		return;
	}
	TAILQ_INIT(&session->conn_list);
	TAILQ_INIT(&session->ccb_pool);
	TAILQ_INIT(&session->ccbs_throttled);

	/* create a unique ID */
	do {
		++current_id;
	} while (!current_id || find_session(current_id) != NULL);
	par->session_id = session->id = current_id;

	create_ccbs(session);
	session->login_type = par->login_type;
	session->CmdSN = 1;

	if ((rc = create_connection(par, session, l)) != 0) {
		if (rc > 0) {
			free(session, M_DEVBUF);
		}
		return;
	}

	s = splbio();
	TAILQ_INSERT_HEAD(&iscsi_sessions, session, sessions);
	splx(s);

	/* Session established, map LUNs? */
	if (par->login_type == ISCSI_LOGINTYPE_MAP) {
		copyinstr(par->TargetName, session->tgtname,
		    sizeof(session->tgtname), NULL);
		if (!map_session(session)) {
			kill_session(session, ISCSI_STATUS_MAP_FAILED,
					LOGOUT_SESSION, FALSE);
			par->status = ISCSI_STATUS_MAP_FAILED;
			return;
		}
	}
}


/*
 * logout:
 *    Handle the logout ioctl - Kill a session.
 *
 *    Parameter:
 *          par      IN/OUT: The login parameters
 */

STATIC void
logout(iscsi_logout_parameters_t *par)
{
	session_t *session;

	DEB(5, ("ISCSI: logout session %d\n", par->session_id));

	if ((session = find_session(par->session_id)) == NULL) {
		DEBOUT(("Session %d not found\n", par->session_id));
		par->status = ISCSI_STATUS_INVALID_SESSION_ID;
		return;
	}
	/* If the session exists, this always succeeds */
	par->status = ISCSI_STATUS_SUCCESS;

	kill_session(session, ISCSI_STATUS_LOGOUT, LOGOUT_SESSION, FALSE);
}


/*
 * add_connection:
 *    Handle the add_connection ioctl.
 *
 *    Parameter:
 *          par      IN/OUT: The login parameters
 *          l        IN: The lwp pointer of the caller
 */

STATIC void
add_connection(iscsi_login_parameters_t *par, struct lwp *l)
{
	session_t *session;

	DEB(5, ("ISCSI: add_connection to session %d\n", par->session_id));

	if ((session = find_session(par->session_id)) == NULL) {
		DEBOUT(("Session %d not found\n", par->session_id));
		par->status = ISCSI_STATUS_INVALID_SESSION_ID;
		return;
	}
	if ((par->status = check_login_pars(par)) == 0) {
		create_connection(par, session, l);
	}
}


/*
 * remove_connection:
 *    Handle the remove_connection ioctl.
 *
 *    Parameter:
 *          par      IN/OUT: The remove parameters
 */

STATIC void
remove_connection(iscsi_remove_parameters_t *par)
{
	connection_t *conn;
	session_t *session;

	DEB(5, ("ISCSI: remove_connection %d from session %d\n",
			par->connection_id, par->session_id));

	if ((session = find_session(par->session_id)) == NULL) {
		DEBOUT(("Session %d not found\n", par->session_id));
		par->status = ISCSI_STATUS_INVALID_SESSION_ID;
		return;
	}

	if ((conn = find_connection(session, par->connection_id)) == NULL) {
		DEBOUT(("Connection %d not found in session %d\n",
				par->connection_id, par->session_id));

		par->status = ISCSI_STATUS_INVALID_CONNECTION_ID;
	} else {
		kill_connection(conn, ISCSI_STATUS_LOGOUT, LOGOUT_CONNECTION,
					FALSE);
		par->status = ISCSI_STATUS_SUCCESS;
	}
}


/*
 * restore_connection:
 *    Handle the restore_connection ioctl.
 *
 *    Parameter:
 *          par      IN/OUT: The login parameters
 *          l        IN: The lwp pointer of the caller
 */

STATIC void
restore_connection(iscsi_login_parameters_t *par, struct lwp *l)
{
	session_t *session;
	connection_t *connection;

	DEB(1, ("ISCSI: restore_connection %d of session %d\n",
			par->connection_id, par->session_id));

	if ((session = find_session(par->session_id)) == NULL) {
		DEBOUT(("Session %d not found\n", par->session_id));
		par->status = ISCSI_STATUS_INVALID_SESSION_ID;
		return;
	}

	if ((connection = find_connection(session, par->connection_id)) == NULL) {
		DEBOUT(("Connection %d not found in session %d\n",
				par->connection_id, par->session_id));
		par->status = ISCSI_STATUS_INVALID_CONNECTION_ID;
		return;
	}

	if ((par->status = check_login_pars(par)) == 0) {
		recreate_connection(par, session, connection, l);
	}
}


#ifndef ISCSI_MINIMAL

/*
 * map_databuf:
 *    Map user-supplied data buffer into kernel space.
 *
 *    Parameter:
 *          p        IN: The proc pointer of the caller
 *          buf      IN/OUT: The virtual address of the buffer, modified
 *                   on exit to reflect kernel VA.
 *          datalen  IN: The size of the data buffer
 *
 *    Returns:
 *          An ISCSI status code on error, else 0.
 */

uint32_t
map_databuf(struct proc *p, void **buf, uint32_t datalen)
{
	vaddr_t kva, databuf, offs;
	int error;

	/* page align address */
	databuf = (vaddr_t) * buf & ~PAGE_MASK;
	/* offset of VA into page */
	offs = (vaddr_t) * buf & PAGE_MASK;
	/* round to full page including offset */
	datalen = (datalen + offs + PAGE_MASK) & ~PAGE_MASK;

	/* Do some magic to the vm space reference count (copied from "copyin_proc") */
	if ((p->p_sflag & PS_WEXIT) || (p->p_vmspace->vm_refcnt < 1)) {
		return ISCSI_STATUS_NO_RESOURCES;
	}
	p->p_vmspace->vm_refcnt++;

	/* this is lifted from uvm_io */
	error = uvm_map_extract(&p->p_vmspace->vm_map, databuf, datalen,
			kernel_map, &kva,
			UVM_EXTRACT_QREF | UVM_EXTRACT_CONTIG |
				UVM_EXTRACT_FIXPROT);
	if (error) {
		DEBOUT(("uvm_map_extract failed, error = %d\n", error));
		return ISCSI_STATUS_NO_RESOURCES;
	}
	/* add offset back into kernel VA */
	*buf = (void *) (kva + offs);

	return 0;
}


/*
 * unmap_databuf:
 *    Remove kernel space mapping of data buffer.
 *
 *    Parameter:
 *          p        IN: The proc pointer of the caller
 *          buf      IN: The kernel virtual address of the buffer
 *          datalen  IN: The size of the data buffer
 *
 *    Returns:
 *          An ISCSI status code on error, else 0.
 */

void
unmap_databuf(struct proc *p, void *buf, uint32_t datalen)
{
	struct vm_map_entry *dead_entries;
	vaddr_t databuf;

	/* round to full page */
	datalen = (datalen + ((uintptr_t) buf & PAGE_MASK) + PAGE_MASK) & ~PAGE_MASK;
	/* page align address */
	databuf = (vaddr_t) buf & ~PAGE_MASK;

	/* following code lifted almost verbatim from uvm_io.c */
	vm_map_lock(kernel_map);
	uvm_unmap_remove(kernel_map, databuf, databuf + datalen, &dead_entries,
	    0);
	vm_map_unlock(kernel_map);
	if (dead_entries != NULL) {
		uvm_unmap_detach(dead_entries, AMAP_REFALL);
	}
	/* this apparently reverses the magic to the vm ref count, from copyin_proc */
	uvmspace_free(p->p_vmspace);
}


/*
 * io_command:
 *    Handle the io_command ioctl.
 *
 *    Parameter:
 *          par      IN/OUT: The iocommand parameters
 *          l        IN: The lwp pointer of the caller
 */

STATIC void
io_command(iscsi_iocommand_parameters_t *par, struct lwp *l)
{
	uint32_t datalen = par->req.datalen;
	void *databuf = par->req.databuf;
	session_t *session;

	DEB(9, ("ISCSI: io_command, SID=%d, lun=%" PRIu64 "\n", par->session_id, par->lun));
	if ((session = find_session(par->session_id)) == NULL) {
		DEBOUT(("Session %d not found\n", par->session_id));
		par->status = ISCSI_STATUS_INVALID_SESSION_ID;
		return;
	}

	par->req.senselen_used = 0;
	par->req.datalen_used = 0;
	par->req.error = 0;
	par->req.status = 0;
	par->req.retsts = SCCMD_UNKNOWN;	/* init to failure code */

	if (par->req.cmdlen > 16 || par->req.senselen > sizeof(par->req.sense)) {
		par->status = ISCSI_STATUS_PARAMETER_INVALID;
		return;
	}

	if (datalen && (par->status = map_databuf(l->l_proc,
			&par->req.databuf, datalen)) != 0) {
		return;
	}
	par->status = send_io_command(session, par->lun, &par->req,
								  par->options.immediate, par->connection_id);

	if (datalen) {
		unmap_databuf(l->l_proc, par->req.databuf, datalen);
		par->req.databuf = databuf;	/* restore original addr */
	}

	switch (par->status) {
	case ISCSI_STATUS_SUCCESS:
		par->req.retsts = SCCMD_OK;
		break;

	case ISCSI_STATUS_TARGET_BUSY:
		par->req.retsts = SCCMD_BUSY;
		break;

	case ISCSI_STATUS_TIMEOUT:
	case ISCSI_STATUS_SOCKET_ERROR:
		par->req.retsts = SCCMD_TIMEOUT;
		break;

	default:
		par->req.retsts = (par->req.senselen_used) ? SCCMD_SENSE
												   : SCCMD_UNKNOWN;
		break;
	}
}
#endif

/*
 * send_targets:
 *    Handle the send_targets ioctl.
 *    Note: If the passed buffer is too small to hold the complete response,
 *    the response is kept in the session structure so it can be
 *    retrieved with the next call to this function without having to go to
 *    the target again. Once the complete response has been retrieved, it
 *    is discarded.
 *
 *    Parameter:
 *          par      IN/OUT: The send_targets parameters
 */

STATIC void
send_targets(iscsi_send_targets_parameters_t *par)
{
	int rc;
	uint32_t rlen, cplen;
	session_t *session;

	if ((session = find_session(par->session_id)) == NULL) {
		DEBOUT(("Session %d not found\n", par->session_id));
		par->status = ISCSI_STATUS_INVALID_SESSION_ID;
		return;
	}

	DEB(9, ("ISCSI: send_targets, rsp_size=%d; Saved list: %p\n",
			par->response_size, session->target_list));

	if (session->target_list == NULL) {
		rc = send_send_targets(session, par->key);
		if (rc) {
			par->status = rc;
			return;
		}
	}
	rlen = session->target_list_len;
	par->response_total = rlen;
	cplen = min(par->response_size, rlen);
	if (cplen) {
		copyout(session->target_list, par->response_buffer, cplen);
	}
	par->response_used = cplen;

	/* If all of the response was copied, don't keep it around */
	if (rlen && par->response_used == rlen) {
		free(session->target_list, M_TEMP);
		session->target_list = NULL;
	}

	par->status = ISCSI_STATUS_SUCCESS;
}


/*
 * set_node_name:
 *    Handle the set_node_name ioctl.
 *
 *    Parameter:
 *          par      IN/OUT: The set_node_name parameters
 */

STATIC void
set_node_name(iscsi_set_node_name_parameters_t *par)
{

	if (strlen(par->InitiatorName) >= ISCSI_STRING_LENGTH ||
		strlen(par->InitiatorAlias) >= ISCSI_STRING_LENGTH) {
		DEBOUT(("*** set_node_name string too long!\n"));
		par->status = ISCSI_STATUS_PARAMETER_INVALID;
		return;
	}
	strlcpy(iscsi_InitiatorName, par->InitiatorName, sizeof(iscsi_InitiatorName));
	strlcpy(iscsi_InitiatorAlias, par->InitiatorAlias, sizeof(iscsi_InitiatorAlias));
	memcpy(&iscsi_InitiatorISID, par->ISID, 6);
	DEB(5, ("ISCSI: set_node_name, ISID A=%x, B=%x, C=%x, D=%x\n",
			iscsi_InitiatorISID.ISID_A, iscsi_InitiatorISID.ISID_B,
			iscsi_InitiatorISID.ISID_C, iscsi_InitiatorISID.ISID_D));

	if (!iscsi_InitiatorISID.ISID_A && !iscsi_InitiatorISID.ISID_B &&
		!iscsi_InitiatorISID.ISID_C && !iscsi_InitiatorISID.ISID_D) {
		iscsi_InitiatorISID.ISID_A = T_FORMAT_EN;
		iscsi_InitiatorISID.ISID_B = htons(0x1);
		iscsi_InitiatorISID.ISID_C = 0x37;
		iscsi_InitiatorISID.ISID_D = 0;
	}

	par->status = ISCSI_STATUS_SUCCESS;
}


/*
 * connection_status:
 *    Handle the connection_status ioctl.
 *
 *    Parameter:
 *          par      IN/OUT: The status parameters
 */

STATIC void
connection_status(iscsi_conn_status_parameters_t *par)
{
	connection_t *conn;
	session_t *session;

	if ((session = find_session(par->session_id)) == NULL) {
		par->status = ISCSI_STATUS_INVALID_SESSION_ID;
		return;
	}

	if (par->connection_id) {
		conn = find_connection(session, par->connection_id);
	} else {
		conn = TAILQ_FIRST(&session->conn_list);
	}
	par->status = (conn == NULL) ? ISCSI_STATUS_INVALID_CONNECTION_ID :
					ISCSI_STATUS_SUCCESS;
	DEB(9, ("ISCSI: connection_status, session %d connection %d --> %d\n",
			par->session_id, par->connection_id, par->status));
}


/*
 * get_version:
 *    Handle the get_version ioctl.
 *
 *    Parameter:
 *          par      IN/OUT: The version parameters
 */

STATIC void
get_version(iscsi_get_version_parameters_t *par)
{
	par->status = ISCSI_STATUS_SUCCESS;
	par->interface_version = INTERFACE_VERSION;
	par->major = VERSION_MAJOR;
	par->minor = VERSION_MINOR;
	strlcpy(par->version_string, VERSION_STRING,
		sizeof(par->version_string));
}


/* -------------------------------------------------------------------- */

/*
 * kill_all_sessions:
 *    Terminate all sessions (called when the driver unloads).
 */

void
kill_all_sessions(void)
{
	session_t *sess;

	while ((sess = TAILQ_FIRST(&iscsi_sessions)) != NULL) {
		kill_session(sess, ISCSI_STATUS_DRIVER_UNLOAD, LOGOUT_SESSION,
				FALSE);
	}
}

/*
 * handle_connection_error:
 *    Deal with a problem during send or receive.
 *
 *    Parameter:
 *       conn        The connection the problem is associated with
 *       status      The status code to insert into any unfinished CCBs
 *       dologout    Whether Logout should be attempted
 */

void
handle_connection_error(connection_t *conn, uint32_t status, int dologout)
{

	DEBC(conn, 0, ("*** Connection Error, status=%d, logout=%d, state=%d\n",
				   status, dologout, conn->state));

	if (!conn->terminating && conn->state <= ST_LOGOUT_SENT) {
		/* if we get an error while winding down, escalate it */
		if (dologout >= 0 && conn->state >= ST_WINDING_DOWN) {
			dologout = NO_LOGOUT;
		}
		kill_connection(conn, status, dologout, TRUE);
	}
}


/*
 * iscsi_cleanup_thread
 *    Global thread to handle connection and session cleanup after termination.
 */

void
iscsi_cleanup_thread(void *par)
{
	int s, rc;
	connection_t *conn;
	session_t *sess, *nxt;
	uint32_t status;

	s = splbio();
	while ((conn = TAILQ_FIRST(&iscsi_cleanupc_list)) != NULL ||
		iscsi_num_send_threads ||
		!iscsi_detaching) {
		if (conn != NULL) {
			TAILQ_REMOVE(&iscsi_cleanupc_list, conn, connections);
			splx(s);

			sess = conn->session;
			status = conn->terminating;

			DEBC(conn, 5, ("Cleanup: Waiting for threads to exit\n"));
			while (conn->sendproc || conn->rcvproc)
				tsleep(conn, PWAIT, "termwait", hz);

			while (conn->usecount > 0)
				tsleep(conn, PWAIT, "finalwait", hz);

			callout_halt(&conn->timeout, NULL);
			closef(conn->sock);
			free(conn, M_DEVBUF);

			--sess->total_connections;

			s = splbio();
			TAILQ_FOREACH_SAFE(sess, &iscsi_cleanups_list, sessions, nxt) {
				if (sess->total_connections != 0)
					continue;

				TAILQ_REMOVE(&iscsi_cleanups_list, sess, sessions);
				splx(s);

				DEB(1, ("Cleanup: Unmap session %d\n", sess->id));

				rc = unmap_session(sess);
				if (rc == 0) {
					DEB(1, ("Cleanup: Unmap session %d failed\n", sess->id));
					s = splbio();
					TAILQ_INSERT_HEAD(&iscsi_cleanups_list, sess, sessions);
					splx(s);
				}

				if (sess->target_list != NULL)
					free(sess->target_list, M_TEMP);
				/* notify event handlers of session shutdown */
				add_event(ISCSI_SESSION_TERMINATED, sess->id, 0, status);
				DEB(1, ("Cleanup: session ended %d\n", sess->id));
				free(sess, M_DEVBUF);

				s = splbio();
			}
			splx(s);

			DEB(5, ("Cleanup: Done\n"));

			s = splbio();
		} else {
			/* Go to sleep, but wake up every 30 seconds to
			 * check for dead event handlers */
			splx(s);
			rc = tsleep(&iscsi_cleanupc_list, PWAIT, "cleanup",
				(TAILQ_FIRST(&event_handlers)) ? 30 * hz : 0);
			s = splbio();
			/* if timed out, not woken up */
			if (rc == EWOULDBLOCK)
				check_event_handlers();
		}
	}
	splx(s);

	add_event(ISCSI_DRIVER_TERMINATING, 0, 0, ISCSI_STATUS_DRIVER_UNLOAD);

	/*
     * Wait for all event handlers to deregister, but don't wait more
     * than 1 minute (assume registering app has died if it takes longer).
	 */
	for (s = 0; TAILQ_FIRST(&event_handlers) != NULL && s < 60; s++)
		tsleep(&s, PWAIT, "waiteventclr", hz);

	iscsi_cleanproc = NULL;
	DEB(5, ("Cleanup thread exits\n"));
	kthread_exit(0);
}


/* -------------------------------------------------------------------- */

/*
 * iscsi_ioctl:
 *    Driver ioctl entry.
 *
 *    Parameter:
 *       dev      The device (ignored)
 *       cmd      The ioctl Command
 *       addr     IN/OUT: The command parameter
 *       flag     Flags (ignored)
 *       l        IN: The lwp object of the caller
 */

int
iscsiioctl(struct file *fp, u_long cmd, void *addr)
{
	struct lwp *l = curlwp;

	DEB(1, ("ISCSI Ioctl cmd = %x\n", (int) cmd));

	switch (cmd) {
	case ISCSI_GET_VERSION:
		get_version((iscsi_get_version_parameters_t *) addr);
		break;

	case ISCSI_LOGIN:
		login((iscsi_login_parameters_t *) addr, l);
		break;

	case ISCSI_ADD_CONNECTION:
		add_connection((iscsi_login_parameters_t *) addr, l);
		break;

	case ISCSI_RESTORE_CONNECTION:
		restore_connection((iscsi_login_parameters_t *) addr, l);
		break;

	case ISCSI_LOGOUT:
		logout((iscsi_logout_parameters_t *) addr);
		break;

	case ISCSI_REMOVE_CONNECTION:
		remove_connection((iscsi_remove_parameters_t *) addr);
		break;

#ifndef ISCSI_MINIMAL
	case ISCSI_IO_COMMAND:
		io_command((iscsi_iocommand_parameters_t *) addr, l);
		break;
#endif

	case ISCSI_SEND_TARGETS:
		send_targets((iscsi_send_targets_parameters_t *) addr);
		break;

	case ISCSI_SET_NODE_NAME:
		set_node_name((iscsi_set_node_name_parameters_t *) addr);
		break;

	case ISCSI_CONNECTION_STATUS:
		connection_status((iscsi_conn_status_parameters_t *) addr);
		break;

	case ISCSI_REGISTER_EVENT:
		register_event((iscsi_register_event_parameters_t *) addr);
		break;

	case ISCSI_DEREGISTER_EVENT:
		deregister_event((iscsi_register_event_parameters_t *) addr);
		break;

	case ISCSI_WAIT_EVENT:
		check_event((iscsi_wait_event_parameters_t *) addr, TRUE);
		break;

	case ISCSI_POLL_EVENT:
		check_event((iscsi_wait_event_parameters_t *) addr, FALSE);
		break;

	default:
		DEBOUT(("Invalid IO-Control Code\n"));
		return ENOTTY;
	}

    /*
     * NOTE: We return 0 even if the function fails as long as the ioctl code
     * is good, so the status code is copied back to the caller.
	 */
	return 0;
}
