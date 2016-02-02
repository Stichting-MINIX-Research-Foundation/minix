/* $NetBSD: mech.h,v 1.4 2011/02/12 23:21:32 christos Exp $ */

/* Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mateusz Kocielski.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.	IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MECH_H_
#define _MECH_H_

#include <sys/queue.h>

#include <assert.h>
#include <stdint.h>

#include "dict.h"
#include "list.h"

/** mechanism status */
enum {
	STATUS_AUTHENTICATION,	/**< authentication in progress */
	STATUS_AUTHENTICATED	/**< session authenticated. this value is used
			         * after last step of the authentication and
			         * means only that last step was performed. */
};

/* mechanism cont return values - used by _cont() functions */
enum {
	MECH_ERROR	= -1,	/* error */
	MECH_OK		= 0,	/* mechanism authenticated */
	MECH_STEP	= 1	/* mechanism needs one or more steps more */
};

/* qop enums and flags */
/* NB: used to index saslc__mech_qop_tbl[] */
typedef enum {
	QOP_NONE = 0,		/* no QOP layer */
	QOP_INT  = 1,		/* integrity */
	QOP_CONF = 2		/* confirmation */
} saslc__mech_sess_qop_t;

/* NB: These flags must match the security layer flags provided by the
 * GSSAPI server.  See RFC 2222 section 7.2.3 and RFC 4752 section 3.3. */
#define F_QOP_NONE		(1 << QOP_NONE)
#define F_QOP_INT		(1 << QOP_INT)
#define F_QOP_CONF		(1 << QOP_CONF)

/* mechanism session */
typedef struct saslc__mech_sess_t {
	uint32_t status;		/**< status of authentication */
	uint32_t step;			/**< step counter */
	saslc__mech_sess_qop_t qop;	/**< quality of protection layer */
} saslc__mech_sess_t;

/* mechanism functions */
typedef int (*saslc__mech_create_t)(saslc_sess_t *);
typedef int (*saslc__mech_cont_t)(saslc_sess_t *, const void *, size_t,
    void **, size_t *);
typedef ssize_t (*saslc__mech_xxcode_t)(saslc_sess_t *, const void *, size_t,
    void **, size_t *);
typedef int (*saslc__mech_destroy_t)(saslc_sess_t *);

/** mechanism structure */
typedef struct saslc__mech_t {
	const char *name;		/**< mechanism name */
	const uint32_t flags;		/**< mechanism flags */
#define FLAG_NONE	 0		/**< no flags */
#define FLAG_ANONYMOUS	(1 << 0)	/**< anonymous authentication */
#define FLAG_PLAINTEXT	(1 << 1)	/**< mechanism uses plaintext
				  	   for sharing secrets */
#define FLAG_DICTIONARY (1 << 2)	/**< dictionary attack against
					   authentication is possible */
#define FLAG_ACTIVE	(1 << 3)	/**< nondictionary active attack
					   against authentication is
					   possible */
#define FLAG_MUTUAL	(1 << 4)	/**< mutual authentication */

/* see xsess.c:flags_OK() for REJ_FLAGS and REQ_FLAGS meaning */
#define REJ_FLAGS	(FLAG_ANONYMOUS | FLAG_PLAINTEXT |\
			 FLAG_DICTIONARY | FLAG_ACTIVE)
#define REQ_FLAGS	(FLAG_MUTUAL)

	saslc__mech_create_t create;	/**< create function - creates
					   mechanism instance */
	saslc__mech_cont_t cont;	/**< step function - performs
					   one step of authentication */
	saslc__mech_xxcode_t encode;	/**< encoding function - encodes input
					   according to negotiated security
					   layer */
	saslc__mech_xxcode_t decode;	/**< decoding function - decodes input
					   according to negotiated security
					   layer */
	saslc__mech_destroy_t destroy;	/**< destroy function - destroys
					   mechanism instance */
} saslc__mech_t;

/* mechanism list */

/** mechanisms list node */
typedef struct saslc__mech_list_node_t {
	LIST_ENTRY(saslc__mech_list_node_t) nodes;	/**< nodes */
	const saslc__mech_t *mech;			/**< mechanism */
	saslc__dict_t *prop;				/**< mechanism config */
} saslc__mech_list_node_t;

/* mechanisms list head */
typedef struct saslc__mech_list_t saslc__mech_list_t;
LIST_HEAD(saslc__mech_list_t, saslc__mech_list_node_t);

/* mechanism list functions */
saslc__mech_list_t *saslc__mech_list_create(saslc_t *);
void saslc__mech_list_destroy(saslc__mech_list_t *);
saslc__mech_list_node_t *saslc__mech_list_get(saslc__mech_list_t *,
    const char *);

/* generic functions */
int saslc__mech_generic_create(saslc_sess_t *);
int saslc__mech_generic_destroy(saslc_sess_t *);

/* additional functions */
int saslc__mech_strdup(saslc_sess_t *, char **, size_t *, const char *,
    const char *);

/* qop inline routines */
extern const named_flag_t saslc__mech_qop_tbl[4];

static inline const char *
saslc__mech_qop_name(saslc__mech_sess_qop_t qop)
{

	/* NULL terminated table */
	assert(qop < __arraycount(saslc__mech_qop_tbl) - 1);
	return saslc__mech_qop_tbl[qop].name;
}

static inline int
saslc__mech_qop_flag(saslc__mech_sess_qop_t qop)
{

	/* NULL terminated table */
	assert(qop < __arraycount(saslc__mech_qop_tbl) - 1);
	return saslc__mech_qop_tbl[qop].flag;
}

static inline unsigned int
saslc__mech_qop_list_flags(list_t *list)
{

	return saslc__list_flags(list, saslc__mech_qop_tbl);
}

#endif /* ! _MECH_H_ */
