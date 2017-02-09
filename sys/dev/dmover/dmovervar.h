/*	$NetBSD: dmovervar.h,v 1.10 2005/12/24 20:27:29 perry Exp $	*/

/*
 * Copyright (c) 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DMOVER_DMOVERVAR_H_
#define _DMOVER_DMOVERVAR_H_

#include <sys/lock.h>
#include <sys/queue.h>

/*
 * Types of buffers the dmover-api can handle.
 */
typedef enum {
	DMOVER_BUF_NONE			= 0,
	DMOVER_BUF_LINEAR		= 1,
	DMOVER_BUF_UIO			= 2
} dmover_buffer_type;

typedef struct {
	void *l_addr;
	size_t l_len;
} dmover_buf_linear;

typedef union {
	dmover_buf_linear dmbuf_linear;
	struct uio *dmbuf_uio;
} dmover_buffer;

/*
 * dmover_algdesc:
 *
 *	This structure describes an dmover algorithm.
 *
 *	All members of this structure are public.
 */
struct dmover_algdesc {
	const char *dad_name;		/* algorithm name */
	void *dad_data;			/* opaque algorithm description */
	int dad_ninputs;		/* number of inputs */
};

/*
 * dmover_assignment:
 *
 *	This structure contains the information necessary to assign
 *	a request to a back-end.
 *
 *	All members of this structure are public.
 */
struct dmover_assignment {
	struct dmover_backend *das_backend;
	const struct dmover_algdesc *das_algdesc;
};

/*
 * dmover_session:
 *
 *	State for a dmover session.
 */
struct dmover_session {
	/*
	 * PUBLIC MEMBERS
	 */
	void	*dses_cookie;		/* for client */
	int	dses_ninputs;		/* number of inputs for function */

	/*
	 * PRIVATE MEMBERS
	 */
	LIST_ENTRY(dmover_session) __dses_list;

	/*
	 * XXX Assignment is static when a session is
	 * XXX created, for now.
	 */
	struct dmover_assignment __dses_assignment;

	/* List of active requests on this session. */
	TAILQ_HEAD(, dmover_request) __dses_pendreqs;
	int	__dses_npendreqs;
};

#define	dmover_session_insque(dses, dreq)				\
do {									\
	TAILQ_INSERT_TAIL(&(dses)->__dses_pendreqs, (dreq), dreq_sesq);	\
	(dses)->__dses_npendreqs++;					\
} while (/*CONSTCOND*/0)

#define	dmover_session_remque(dses, dreq)				\
do {									\
	TAILQ_REMOVE(&(dses)->__dses_pendreqs, (dreq), dreq_sesq);	\
	(dses)->__dses_npendreqs--;					\
} while (/*CONSTCOND*/0)

/*
 * dmover_request:
 *
 *	A data dmover request.
 */
struct dmover_request {
	/*
	 * PUBLIC MEMBERS
	 */

	/* Links on session and back-end queues. */
	TAILQ_ENTRY(dmover_request) dreq_sesq;
	TAILQ_ENTRY(dmover_request) dreq_dmbq;

	/* Pointer to our session. */
	struct dmover_session *dreq_session;

	/* Our current back-end assignment. */
	struct dmover_assignment *dreq_assignment;

	/* Function to call when processing is complete. */
	void	(*dreq_callback)(struct dmover_request *);
	void	*dreq_cookie;	/* for client */

	volatile int dreq_flags; /* flags; see below */
	int	dreq_error;	   /* valid if DMOVER_REQ_ERROR is set */

	/*
	 * General purpose immediate value.  Can be used as an
	 * input, output, or both, depending on the function.
	 */
	uint8_t	dreq_immediate[8];

	/* Output buffer. */
	dmover_buffer_type dreq_outbuf_type;
	dmover_buffer dreq_outbuf;

	/* Input buffer. */
	dmover_buffer_type dreq_inbuf_type;
	dmover_buffer *dreq_inbuf;
};

/* dreq_flags */
#define	DMOVER_REQ_DONE		0x0001	/* request is completed */
#define	DMOVER_REQ_ERROR	0x0002	/* error occurred */
#define	DMOVER_REQ_RUNNING	0x0004	/* request is being executed */
#define	DMOVER_REQ_WAIT		0x0008	/* wait for completion */

#define	__DMOVER_REQ_INBUF_FREE	0x01000000 /* need to free input buffer */

#define	__DMOVER_REQ_FLAGS_PRESERVE					\
	(DMOVER_REQ_WAIT | __DMOVER_REQ_INBUF_FREE)

/*
 * dmover_backend:
 *
 *	Glue between the dmover-api middle layer and the dmover
 *	backends.
 *
 *	All members of this structure are public.
 */
struct dmover_backend {
	TAILQ_ENTRY(dmover_backend) dmb_list;

	const char *dmb_name;		/* name of back-end */
	u_int dmb_speed;		/* est. KB/s throughput */

	void	*dmb_cookie;		/* for back-end */

	/* List of algorithms this back-ends supports. */
	const struct dmover_algdesc *dmb_algdescs;
	int dmb_nalgdescs;

	/* Back-end functions. */
	void	(*dmb_process)(struct dmover_backend *);

	/* List of sessions currently on this back-end. */
	LIST_HEAD(, dmover_session) dmb_sessions;
	int	dmb_nsessions;		/* current number of sessions */

	/* List of active requests on this back-end. */
	TAILQ_HEAD(, dmover_request) dmb_pendreqs;
	int	dmb_npendreqs;
};

#define	dmover_backend_insque(dmb, dreq)				\
do {									\
	TAILQ_INSERT_TAIL(&(dmb)->dmb_pendreqs, (dreq), dreq_dmbq);	\
	(dmb)->dmb_npendreqs++;						\
} while (/*CONSTCOND*/0)

#define	dmover_backend_remque(dmb, dreq)				\
do {									\
	TAILQ_REMOVE(&(dmb)->dmb_pendreqs, (dreq), dreq_dmbq);		\
	(dmb)->dmb_npendreqs--;						\
} while (/*CONSTCOND*/0)

/*
 * Well-known data mover functions.  Using these for the function name
 * saves space.
 */
extern const char dmover_funcname_zero[];
#define	DMOVER_FUNC_ZERO		dmover_funcname_zero

extern const char dmover_funcname_fill8[];
#define	DMOVER_FUNC_FILL8		dmover_funcname_fill8

extern const char dmover_funcname_copy[];
#define	DMOVER_FUNC_COPY		dmover_funcname_copy

extern const char dmover_funcname_iscsi_crc32c[];
#define	DMOVER_FUNC_ISCSI_CRC32C	dmover_funcname_iscsi_crc32c

extern const char dmover_funcname_xor2[];
#define	DMOVER_FUNC_XOR2		dmover_funcname_xor2

extern const char dmover_funcname_xor3[];
#define	DMOVER_FUNC_XOR3		dmover_funcname_xor3

extern const char dmover_funcname_xor4[];
#define	DMOVER_FUNC_XOR4		dmover_funcname_xor4

extern const char dmover_funcname_xor5[];
#define	DMOVER_FUNC_XOR5		dmover_funcname_xor5

extern const char dmover_funcname_xor6[];
#define	DMOVER_FUNC_XOR6		dmover_funcname_xor6

extern const char dmover_funcname_xor7[];
#define	DMOVER_FUNC_XOR7		dmover_funcname_xor7

extern const char dmover_funcname_xor8[];
#define	DMOVER_FUNC_XOR8		dmover_funcname_xor8

extern const char dmover_funcname_xor9[];
#define	DMOVER_FUNC_XOR9		dmover_funcname_xor9

extern const char dmover_funcname_xor10[];
#define	DMOVER_FUNC_XOR10		dmover_funcname_xor10

extern const char dmover_funcname_xor11[];
#define	DMOVER_FUNC_XOR11		dmover_funcname_xor11

extern const char dmover_funcname_xor12[];
#define	DMOVER_FUNC_XOR12		dmover_funcname_xor12

extern const char dmover_funcname_xor13[];
#define	DMOVER_FUNC_XOR13		dmover_funcname_xor13

extern const char dmover_funcname_xor14[];
#define	DMOVER_FUNC_XOR14		dmover_funcname_xor14

extern const char dmover_funcname_xor15[];
#define	DMOVER_FUNC_XOR15		dmover_funcname_xor15

extern const char dmover_funcname_xor16[];
#define	DMOVER_FUNC_XOR16		dmover_funcname_xor16

/* Back-end management functions. */
void	dmover_backend_register(struct dmover_backend *);
void	dmover_backend_unregister(struct dmover_backend *);
int	dmover_backend_alloc(struct dmover_session *, const char *);
void	dmover_backend_release(struct dmover_session *);

/* Session management functions. */
void	dmover_session_initialize(void);
int	dmover_session_create(const char *, struct dmover_session **);
void	dmover_session_destroy(struct dmover_session *);

/* Request management functions. */
void	dmover_request_initialize(void);
struct dmover_request *dmover_request_alloc(struct dmover_session *,
	    dmover_buffer *);
void	dmover_request_free(struct dmover_request *);

/* Processing engine functions. */
void	dmover_process_initialize(void);
void	dmover_process(struct dmover_request *);
void	dmover_done(struct dmover_request *);

/* Utility functions. */
const struct dmover_algdesc *
	    dmover_algdesc_lookup(const struct dmover_algdesc *, int,
	    const char *);

#endif /* _DMOVER_DMOVERVAR_H_ */
