/* $NetBSD: error.h,v 1.3 2011/02/11 23:44:43 christos Exp $ */

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

#ifndef _ERROR_H_
#define _ERROR_H_

/** error definitions */
typedef enum {
	ERROR_GENERAL,		/**< general error */
	ERROR_NOMEM,		/**< no memory available */
	ERROR_BADARG,		/**< bad argument passed to function */
	ERROR_NOTEXISTS,	/**< key/node does not exist */
	ERROR_MECH,		/**< mechanism error */
	ERROR_PARSE		/**< parse error */
} saslc__error_code_t;

/** error type */
typedef struct saslc__error_t {
	saslc__error_code_t err_no;     /**< error number */
	const char *err_str;	        /**< string error */
} saslc__error_t;

/*
 * saslc__error_set - sets error for the context of sasl session
 *
 * E - error
 * N - error type
 * S - error message
 */
#define saslc__error_set(E, N, S)	\
do {					\
	(E)->err_no = (N);		\
	(E)->err_str = (S);		\
} while(/*CONSTCOND*/0);

/*
 * saslc__eror_get_errno - gets error type
 *
 * E - error
 */
#define saslc__error_get_errno(E) ((E)->err_no)

const char *saslc__error_get_strerror(saslc__error_t *);

/*
 * saslc__error_set_errno - sets error type with the default message
 *
 * E - error
 * N - error type
 */
#define saslc__error_set_errno(E, N) saslc__error_set((E), (N), NULL)

/*
 * ERR - gets address of the error structure, this macro should be used only
 * for the saslc_t and saslc_session_t structures.
 *
 * X - context or session
 */
#define ERR(X) (&((X)->err))

#endif /* ! _ERROR_H_ */
