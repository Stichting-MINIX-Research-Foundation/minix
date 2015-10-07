/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@netbsd.org)
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
#ifndef NETPGP_H_
#define NETPGP_H_

#ifndef __BEGIN_DECLS
#  if defined(__cplusplus)
#  define __BEGIN_DECLS           extern "C" {
#  define __END_DECLS             }
#  else
#  define __BEGIN_DECLS
#  define __END_DECLS
#  endif
#endif

__BEGIN_DECLS

/* structure used to hold (key,value) pair information */
typedef struct netpgp_t {
	unsigned	  c;		/* # of elements used */
	unsigned	  size;		/* size of array */
	char		**name;		/* key names */
	char		**value;	/* value information */
	void		 *pubring;	/* public key ring */
	void		 *secring;	/* s3kr1t key ring */
	void		 *io;		/* the io struct for results/errs */
	void		 *passfp;	/* file pointer for password input */
} netpgp_t;

/* begin and end */
int netpgp_init(netpgp_t *);
int netpgp_end(netpgp_t *);

/* debugging, reflection and information */
int netpgp_set_debug(const char *);
int netpgp_get_debug(const char *);
const char *netpgp_get_info(const char *);
int netpgp_list_packets(netpgp_t *, char *, int, char *);

/* variables */
int netpgp_setvar(netpgp_t *, const char *, const char *);
char *netpgp_getvar(netpgp_t *, const char *);

/* key management */
int netpgp_list_keys(netpgp_t *);
int netpgp_list_sigs(netpgp_t *, const char *);
int netpgp_find_key(netpgp_t *, char *);
char *netpgp_get_key(netpgp_t *, const char *);
int netpgp_export_key(netpgp_t *, char *);
int netpgp_import_key(netpgp_t *, char *);
int netpgp_generate_key(netpgp_t *, char *, int);

/* file management */
int netpgp_encrypt_file(netpgp_t *, const char *, const char *, char *, int);
int netpgp_decrypt_file(netpgp_t *, const char *, char *, int);
int netpgp_sign_file(netpgp_t *, const char *, const char *, char *, int, int, int);
int netpgp_verify_file(netpgp_t *, const char *, const char *, int);

/* memory signing */
int netpgp_sign_memory(netpgp_t *, const char *, char *, size_t, char *, size_t, const unsigned, const unsigned);
int netpgp_verify_memory(netpgp_t *, const void *, const size_t, const int);

__END_DECLS

#endif /* !NETPGP_H_ */
