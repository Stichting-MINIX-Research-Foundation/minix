/*	$NetBSD: iscsi_globals.h,v 1.13 2015/05/30 20:09:47 joerg Exp $	*/

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
#ifndef _ISCSI_GLOBALS_H
#define _ISCSI_GLOBALS_H

/*#include "opt_ddb.h" */
#define DDB 1

/* Includes we need in all files */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/scsiio.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsipiconf.h>

#include "iscsi.h"
#include "iscsi_pdu.h"
#include "iscsi_ioctl.h"

/* ------------------------ Code selection constants ------------------------ */

/* #define ISCSI_DEBUG      1 */

/* -------------------------  Global Constants  ----------------------------- */

/* Version information */

#define INTERFACE_VERSION	2
#define VERSION_MAJOR		3
#define VERSION_MINOR		1
#define VERSION_STRING		"NetBSD iSCSI Software Initiator 20110407"

/*
Various checks are made that the expected cmd Serial Number is less than
the actual command serial number. The extremely paranoid amongst us
believe that a malicious iSCSI server could set this artificially low
and effectively DoS a naive initiator. For this (possibly ludicrous)
reason, I have added the two definitions below (agc, 2011/04/09). The
throttling definition enables a check that the CmdSN is less than the
ExpCmdSN in iscsi_send.c, and is enabled by default. The second definition
effectively says "don't bother testing these values", and is used right
now only in iscsi_send.c.
 */
#define ISCSI_THROTTLING_ENABLED	1
#define ISCSI_SERVER_TRUSTED	1

/*
   NOTE: CCBS_PER_SESSION must not exceed 256 due to the way the ITT
   is constructed (it has the CCB index in its lower 8 bits). If it should ever
   be necessary to increase the number beyond that (which isn't expected),
   the corresponding ITT generation and extraction code must be rewritten.
*/
#define CCBS_PER_SESSION      64	/* ToDo: Reasonable number?? */
/*
   NOTE: PDUS_PER_CONNECTION is a number that could potentially impact
   performance if set too low, as a single command may use up a lot of PDUs for
   high values of First/MaxBurstLength and small values of
   MaxRecvDataSegmentLength of the target.
*/
#define PDUS_PER_CONNECTION   64	/* ToDo: Reasonable number?? */

/* max outstanding serial nums before we give up on the connection */
#define SERNUM_BUFFER_LENGTH  (CCBS_PER_SESSION / 2)	/* ToDo: Reasonable?? */

/* The RecvDataSegmentLength for Target->Initiator */
#define DEFAULT_MaxRecvDataSegmentLength     (64*1024)

/* Command timeout (reset on received PDU associated with the command's CCB) */
#define COMMAND_TIMEOUT		(7 * hz) /* ToDo: Reasonable? (7 seconds) */
#define MAX_CCB_TIMEOUTS	3		/* Max number of tries to resend or SNACK */
#define MAX_CCB_TRIES		9      	/* Max number of total tries to recover */

/* Connectionn timeout (reset on every valid received PDU) */
#define CONNECTION_TIMEOUT       (2 * hz)	/* ToDo: Reasonable? (2 seconds) */
#define CONNECTION_IDLE_TIMEOUT  (30 * hz)	/* Adjusted to Time2Retain/2 later */
#define MAX_CONN_TIMEOUTS        4	/* Max number of tries to ping a target */

/* Maximum attempts to recover connection */
#define MAX_RECOVERY_ATTEMPTS	2	/* If two attempts don't work, something */
									/* probably is seriously broken */

/* PDU flags */

#define PDUF_BUSY	0x01	/* PDU is being sent, don't re-send */
#define PDUF_INQUEUE	0x02	/* PDU is in send queue */
#define PDUF_PRIORITY	0x04	/* Insert PDU at head of queue */
#define PDUF_NOUPDATE	0x10	/* Do not update PDU header/digest (test mode) */

/* CCB Flags */

#define CCBF_COMPLETE   0x0001	/* received status */
#define CCBF_RESENT     0x0002	/* ccb was resent */
#define CCBF_SENDTARGET 0x0004	/* SendTargets text request, not negotiation */
#define CCBF_WAITING    0x0008	/* CCB is waiting for MaxCmdSN, wake it up */
#define CCBF_GOT_RSP    0x0010	/* Got at least one response to this request */
#define CCBF_REASSIGN   0x0020	/* Command can be reassigned */
#define CCBF_OTHERCONN  0x0040	/* a logout for a different connection */
#define CCBF_WAITQUEUE  0x0080	/* CCB is on waiting queue */
#define CCBF_THROTTLING 0x0100	/* CCB is on throttling queue */

/* ---------------------------  Global Types  ------------------------------- */

/* Connection state */

typedef enum {
	/* first three correspond to CSG/NSG coding */
	ST_SEC_NEG	= 0,	/* security negotiation phase */
	ST_OP_NEG	= 1,	/* operational negotiation phase */
	ST_FULL_FEATURE	= 3,	/* full feature phase */
	/* rest is internal */
	ST_WINDING_DOWN	= 4,	/* connection termination initiated, logging out */
	ST_LOGOUT_SENT	= 5,	/* logout has been sent */
	ST_SETTLING	= 6,	/* waiting for things to settle down */
	ST_IDLE		= 7	/* connection is idle (ready to delete) */	
} conn_state_t;


/* Logout state */

typedef enum {
	NOT_LOGGED_OUT,				/* Not logged out */
	LOGOUT_SENT,				/* Logout was sent */
	LOGOUT_SUCCESS,				/* Logout succeeded */
	LOGOUT_FAILED				/* Logout failed */
} logout_state_t;


/* CCB Disposition */

typedef enum {
	CCBDISP_UNUSED,	/* 0 = In free pool */
	CCBDISP_BUSY,	/* This CCB is busy, don't allow rx ops */
	CCBDISP_NOWAIT,	/* Not waiting for anything */
	CCBDISP_FREE,	/* Free this CCB when done */
	CCBDISP_WAIT,	/* Calling thread is waiting for completion */
	CCBDISP_SCSIPI,	/* Call scsipi_done when operation completes */
	CCBDISP_DEFER	/* Defer waiting until all PDUs have been queued */
} ccb_disp_t;


/* PDU Disposition */

typedef enum {
	PDUDISP_UNUSED,		/* 0 = In free pool */
	PDUDISP_SIGNAL,		/* Free this PDU when done and wakeup(pdu) */
	PDUDISP_FREE,		/* Free this PDU when done */
	PDUDISP_WAIT		/* Waiting for acknowledge */
} pdu_disp_t;


typedef struct connection_s connection_t;
typedef struct session_s session_t;
typedef struct ccb_s ccb_t;
typedef struct pdu_s pdu_t;

/* the serial number management structure (a circular buffer) */

typedef struct {
	uint32_t	ExpSN;	/* ExpxxSN (Data or Stat) sent to the target */
	uint32_t	next_sn; /* next_sn (== ExpSN if no ack is pending) */
	int		top;	/* top of buffer (newest element) */
	int		bottom;	/* bottom of buffer (oldest element) */
	uint32_t	sernum[SERNUM_BUFFER_LENGTH];	/* the serial numbers */
	bool		ack[SERNUM_BUFFER_LENGTH];	/* acknowledged? */
} sernum_buffer_t;


/*
   The per-PDU data structure.
*/

struct pdu_s {
	TAILQ_ENTRY(pdu_s)	chain;	/* freelist or wait list (or no list) */
	TAILQ_ENTRY(pdu_s)	send_chain;
				/* chaining PDUs waiting to be sent */
	pdu_disp_t		disp; /* what to do with this pdu */
	uint32_t		flags; 	/* various processing flags */
	pdu_header_t		pdu; /* Buffer for PDU associated with cmd */
	void			*temp_data; /* (free after use) */
	uint32_t		temp_data_len;	/* size of temp data */

	struct uio		uio; /* UIO structure */
	struct iovec		io_vec[4];
				/* Header + data + data-digest + padding */

	struct uio		save_uio;
				/* UIO structure save for retransmits */
	struct iovec		save_iovec[4];
				/* Header + data + data-digest + padding */
	uint32_t		data_digest;
				/* holds data digest if enabled */
	ccb_t			*owner;
				/* the ccb this PDU belongs to (if any) */
	connection_t		*connection;
				/* the connection this PDU belongs to */
};


/* the PDU list type */

TAILQ_HEAD(pdu_list_s, pdu_s);
typedef struct pdu_list_s pdu_list_t;

/*
   The per-command data structure. Calling it ccb in correspondence
   to other HA drivers.
*/

struct ccb_s {
	TAILQ_ENTRY(ccb_s)	chain;
	/* either freelist or waiting list (or no list) */

	uint32_t		status; /* Status gets entered here */
	ccb_disp_t		disp;	/* what to do with this ccb */

	struct callout		timeout; /* To make sure it isn't lost */
	int			num_timeouts;
	/* How often we've sent out SNACK without answer */
	int			total_tries;
	/* How often we've tried to recover */

	uint32_t		ITT;
	/* task tag: ITT counter + sess id + CCB index */
	sernum_buffer_t		DataSN_buf;
	/* Received Data Seq nums (read ops only) */

	void			*par;
	/* misc. parameter for this request */
	struct scsipi_xfer	*xs;
	/* the scsipi_xfer for this cmd */

	void			*temp_data;
	/* to hold state (mainly during negotiation) */
	void			*text_data;
	/* holds accumulated text for continued PDUs */
	uint32_t		text_len;
	/* length of text data so far */

	uint64_t		lun; /* LUN */
	uint8_t			*cmd; /* SCSI command block */
	uint16_t		cmdlen; /* SCSI command block length */
	bool			data_in; /* if this is a read request */
	uint8_t			*data_ptr; /* data pointer for read/write */
	uint32_t		data_len; /* total data length */
	uint32_t		xfer_len; /* data transferred on read */
	uint32_t		residual; /* residual data size */

	void			*sense_ptr; /* sense data pointer */
	int			sense_len_req; /* requested sense data length */
	int			sense_len_got; /* actual sense data length */

	pdu_t			*pdu_waiting; /* PDU waiting to be ack'ed */
	uint32_t		CmdSN; /* CmdSN associated with waiting PDU */

	int			flags;
	connection_t		*connection; /* connection for CCB */
	session_t		*session; /* session for CCB */
};


/* the CCB list type */

TAILQ_HEAD(ccb_list_s, ccb_s);
typedef struct ccb_list_s ccb_list_t;


/*
   Per connection data: the connection structure
*/
struct connection_s {
	TAILQ_ENTRY(connection_s)	connections;

	pdu_list_t			pdu_pool; /* the free PDU pool */

	ccb_list_t			ccbs_waiting;
					/* CCBs waiting for completion */

	pdu_list_t			pdus_to_send;
					/* the PDUs waiting to be sent */

	sernum_buffer_t			StatSN_buf;
					/* to keep track of received StatSNs */

	uint32_t			max_transfer;
		/* min(MaxRecvDataSegmentLength, MaxBurstLength) */
	uint32_t			max_firstimmed;
		/* 0 if InitialR2T=Yes, else
		   min of (MaxRecvDataSegmentLength, FirstBurstLength) */
	uint32_t			max_firstdata;
		/* 0 if ImmediateData=No, else min of */
		/* (MaxRecvDataSegmentLength, FirstBurstLength) */

	uint32_t			MaxRecvDataSegmentLength;
					/* Target's value */
	uint32_t			Our_MaxRecvDataSegmentLength;
					/* Our own value */
	bool				HeaderDigest;	/* TRUE if doing CRC */
	bool				DataDigest;	/* TRUE if doing CRC */
	uint32_t			Time2Wait;
					/* Negotiated default or logout value */
	uint32_t			Time2Retain;
					/* Negotiated default or logout value */

	uint16_t			id;
		/* connection ID (unique within session) */

	conn_state_t			state; /* State of connection */

	struct lwp			*threadobj;
		/* proc/thread pointer of socket owner */
	struct file			*sock;	/* the connection's socket */
	session_t			*session;
					/* back pointer to the owning session */

	struct lwp			*rcvproc; /* receive thread */
	struct lwp			*sendproc; /* send thread */

	uint32_t			terminating;
					/* if closing down: status */
	int				recover; /* recovery count */
		/* (reset on first successful data transfer) */
	int				usecount; /* number of active CCBs */

	bool				destroy; /* conn will be destroyed */
	bool				in_session;
		/* if it's linked into the session list */
	logout_state_t			loggedout;
		/* status of logout (for recovery) */
	struct callout			timeout;
		/* Timeout for checking if connection is dead */
	int				num_timeouts;
		/* How often we've sent out a NOP without answer */
	uint32_t			idle_timeout_val;
		/* Connection timeout value when idle */

	iscsi_login_parameters_t	*login_par;
					/* only valid during login */

	pdu_t				pdu[PDUS_PER_CONNECTION]; /* PDUs */
};

/* the connection list type */

TAILQ_HEAD(connection_list_s, connection_s);
typedef struct connection_list_s connection_list_t;


/*
   Per session data: the session structure
*/

struct session_s {
	/* Interface to child drivers.
	   NOTE: sc_adapter MUST be the first field in this structure so we can
	   easily get from adapter to session.
	 */
	struct scsipi_adapter	sc_adapter;
	struct scsipi_channel	sc_channel;

	device_t		child_dev;
	/* the child we're associated with - (NULL if not mapped) */

	/* local stuff */
	TAILQ_ENTRY(session_s)	sessions;	/* the list of sessions */

	ccb_list_t		ccb_pool;	/* The free CCB pool */
	ccb_list_t		ccbs_throttled;
				/* CCBs waiting for MaxCmdSN to increase */

	uint16_t		id;	/* session ID (unique within driver) */
	uint16_t		TSIH;	/* Target assigned session ID */

	uint32_t		CmdSN;	 /* Current CmdSN */
	uint32_t		ExpCmdSN; /* Current max ExpCmdSN received */
	uint32_t		MaxCmdSN; /* Current MaxCmdSN */

	/* negotiated values */
	uint32_t		ErrorRecoveryLevel;
	uint32_t		FirstBurstLength;
	uint32_t		MaxBurstLength;
	bool			ImmediateData;
	bool			InitialR2T;
	uint32_t		MaxOutstandingR2T;
	uint32_t		MaxConnections;
	uint32_t		DefaultTime2Wait;
	uint32_t		DefaultTime2Retain;

	iscsi_login_session_type_t login_type;	/* session type */

	/* for send_targets requests */
	uint8_t			*target_list;
	uint32_t		target_list_len;

	uint32_t		conn_id;	/* connection ID counter */

	uint32_t		terminating;	/* if closing down: status */

	uint32_t		active_connections;
				/* currently active connections */
	uint32_t		total_connections;
	/* connections associated with this session (active or winding down) */
	connection_list_t	conn_list;	/* the list of connections */
	connection_t		*mru_connection;
				/* the most recently used connection */

	uint8_t			itt_id; 	/* counter for use in ITT */

	ccb_t			ccb[CCBS_PER_SESSION];		/* CCBs */

	char			tgtname[ISCSI_STRING_LENGTH + 1];
				/* iSCSI target name */
};

/* the session list type */

TAILQ_HEAD(session_list_s, session_s);
typedef struct session_list_s session_list_t;


/*
   The softc structure. This driver doesn't really need one, because there's
   always just one instance, and for the time being it's only loaded as
   an LKM (which doesn't create a softc), but we need one to put into the
   scsipi interface structures, so here it is.
*/

typedef struct iscsi_softc {
	device_t		sc_dev;
} iscsi_softc_t;


/*
   Event notification structures
*/

typedef struct event_s {
	TAILQ_ENTRY(event_s)	link;		/* next event in queue */
	iscsi_event_t		event_kind;	/* which event */
	uint32_t		session_id;	/* affected session ID */
	uint32_t		connection_id;	/* affected connection ID */
	uint32_t		reason;		/* event reason */
} event_t;

/* the event list entry type */

TAILQ_HEAD(event_list_s, event_s);
typedef struct event_list_s event_list_t;


typedef struct event_handler_s {
	TAILQ_ENTRY(event_handler_s)	link;	/* next handler */
	uint32_t			id;	/* unique ID */
	event_list_t			events;	/* list of events pending */
	iscsi_wait_event_parameters_t	*waiter; /* waiting parameter */
	/* following to detect dead handlers */
	event_t				*first_in_list;
} event_handler_t;

/* the event list entry type */

TAILQ_HEAD(event_handler_list_s, event_handler_s);
typedef struct event_handler_list_s event_handler_list_t;

/* /dev/iscsi0 state */
struct iscsifd {
	char dummy;
};

/* -------------------------  Global Variables  ----------------------------- */

/* In iscsi_main.c */

extern struct cfattach iscsi_ca;		/* the device attach structure */

extern session_list_t iscsi_sessions;		/* the list of sessions */

extern connection_list_t iscsi_cleanupc_list;	/* connections to clean up */
extern session_list_t iscsi_cleanups_list;	/* sessions to clean up */
extern bool iscsi_detaching;			/* signal to cleanup thread it should exit */
extern struct lwp *iscsi_cleanproc;		/* pointer to cleanup proc */

extern uint32_t iscsi_num_send_threads;		/* the number of active send threads */

extern uint8_t iscsi_InitiatorName[ISCSI_STRING_LENGTH];
extern uint8_t iscsi_InitiatorAlias[ISCSI_STRING_LENGTH];
extern login_isid_t iscsi_InitiatorISID;

/* Debugging stuff */

#ifndef DDB
#define Debugger() panic("should call debugger here (iscsi.c)")
#endif /* ! DDB */

#ifdef ISCSI_DEBUG

extern int iscsi_debug_level;	/* How much debug info to display */

#define DEBOUT(x) printf x
#define DEB(lev,x) { if (iscsi_debug_level >= lev) printf x ;}
#define DEBC(conn,lev,x) { if (iscsi_debug_level >= lev) { printf("S%dC%d: ", \
				conn ? conn->session->id : -1, \
				conn ? conn->id : -1); printf x ;}}
void dump(void *buf, int len);

#define STATIC static

#else

#define DEBOUT(x)
#define DEB(lev,x)
#define DEBC(conn,lev,x)
#define dump(a,b)

#define STATIC static

#endif

/* Critical section macros */

/* misc stuff */
#define min(a, b) ((a) < (b)) ? (a) : (b)
#define max(a, b) ((a) < (b)) ? (b) : (a)


/*
   Convert unsigned int to 3-byte value (for DataSegmentLength field in PDU)
*/

static __inline void
hton3(uint32_t val, uint8_t *bytes)
{
	bytes[0] = (uint8_t) (val >> 16);
	bytes[1] = (uint8_t) (val >> 8);
	bytes[2] = (uint8_t) val;
}

/*
   Convert 3-byte value to unsigned int (for DataSegmentLength field in PDU)
*/

static __inline uint32_t
ntoh3(uint8_t *bytes)
{
	return (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
}


/*
 * Convert uint64 to network byte order (for LUN field in PDU)
*/
static __inline uint64_t
htonq(uint64_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	uint8_t *s = (uint8_t *) & x;
	return (uint64_t) ((uint64_t) s[0] << 56 | (uint64_t) s[1] << 48 |
			(uint64_t) s[2] << 40 | (uint64_t) s[3] << 32 |
			(uint64_t) s[4] << 24 | (uint64_t) s[5] << 16 |
			(uint64_t) s[6] <<  8 | (uint64_t) s[7]);
#else
	return x;
#endif
}

#define ntohq(x) htonq(x)

/*
 * Serial number buffer empty?
*/

static __inline bool
sn_empty(sernum_buffer_t *buf)
{
	return buf->top == buf->bottom;
}


/*
 * Serial number compare
*/

static __inline bool
sn_a_lt_b(uint32_t a, uint32_t b)
{
	return (a < b && !((b - a) & 0x80000000)) ||
	       (a > b && ((a - b) & 0x80000000));
}

static __inline bool
sn_a_le_b(uint32_t a, uint32_t b)
{
	return (a <= b && !((b - a) & 0x80000000)) ||
	       (a >= b && ((a - b) & 0x80000000));
}

/* in iscsi_ioctl.c */

/* Parameter for logout is reason code in logout PDU, -1 for don't send logout */
#define NO_LOGOUT          -1
#define LOGOUT_SESSION     0
#define LOGOUT_CONNECTION  1
#define RECOVER_CONNECTION 2

void add_event(iscsi_event_t, uint32_t, uint32_t, uint32_t);

void kill_connection(connection_t *, uint32_t, int, bool);
void kill_session(session_t *, uint32_t, int, bool);
void kill_all_sessions(void);
void handle_connection_error(connection_t *, uint32_t, int);
void iscsi_cleanup_thread(void *);

#ifndef ISCSI_MINIMAL
uint32_t map_databuf(struct proc *, void **, uint32_t);
void unmap_databuf(struct proc *, void *, uint32_t);
#endif
int iscsiioctl(struct file *, u_long, void *);

session_t *find_session(uint32_t);
connection_t *find_connection(session_t *, uint32_t);


/* in iscsi_main.c */

/*void iscsiattach(void *); */
int iscsidetach(device_t, int);

void iscsi_done(ccb_t *);
int map_session(session_t *);
int unmap_session(session_t *);

/* in iscsi_send.c */

void iscsi_send_thread(void *);

connection_t *assign_connection(session_t *session, bool waitok);
void resend_pdu(ccb_t *);
int send_login(connection_t *);
int send_logout(connection_t *, connection_t *, int, bool);
int send_data_out(connection_t *, pdu_t *, ccb_t *, ccb_disp_t, bool);
void send_run_xfer(session_t *, struct scsipi_xfer *);
int send_send_targets(session_t *, uint8_t *);
int send_task_management(connection_t *, ccb_t *, struct scsipi_xfer *, int);

void negotiate_login(connection_t *, pdu_t *, ccb_t *);
void acknowledge_text(connection_t *, pdu_t *, ccb_t *);
void start_text_negotiation(connection_t *);
void negotiate_text(connection_t *, pdu_t *, ccb_t *);
int send_nop_out(connection_t *, pdu_t *);
void snack_missing(connection_t *, ccb_t *, uint8_t, uint32_t, uint32_t);
void send_snack(connection_t *, pdu_t *, ccb_t *, uint8_t);
int send_send_targets(session_t *, uint8_t *);

void send_command(ccb_t *, ccb_disp_t, bool, bool);
#ifndef ISCSI_MINIMAL
int send_io_command(session_t *, uint64_t, scsireq_t *, bool, uint32_t);
#endif

void connection_timeout(void *);
void ccb_timeout(void *);

/* in iscsi_rcv.c */

void iscsi_rcv_thread(void *);

/* in iscsi_utils.c */

uint32_t gen_digest(void *, int);
uint32_t gen_digest_2(void *, int, void *, int);

void create_ccbs(session_t *);
ccb_t *get_ccb(connection_t *, bool);
void free_ccb(ccb_t *);
void suspend_ccb(ccb_t *, bool);
void throttle_ccb(ccb_t *, bool);
void wake_ccb(ccb_t *, uint32_t);

void create_pdus(connection_t *);
pdu_t *get_pdu(connection_t *, bool);
void free_pdu(pdu_t *);

void init_sernum(sernum_buffer_t *);
int add_sernum(sernum_buffer_t *, uint32_t);
uint32_t ack_sernum(sernum_buffer_t *, uint32_t);

/* in iscsi_text.c */

int assemble_login_parameters(connection_t *, ccb_t *, pdu_t *);
int assemble_security_parameters(connection_t *, ccb_t *, pdu_t *, pdu_t *);
int assemble_negotiation_parameters(connection_t *, ccb_t *, pdu_t *, pdu_t *);
int init_text_parameters(connection_t *, ccb_t *);
int assemble_send_targets(pdu_t *, uint8_t *);
void set_negotiated_parameters(ccb_t *);

#endif /* !_ISCSI_GLOBALS_H */
