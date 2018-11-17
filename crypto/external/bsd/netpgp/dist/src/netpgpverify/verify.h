/*-
 * Copyright (c) 2012,2013,2014,2015,2016 Alistair Crooks <agc@NetBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef NETPGP_VERIFY_H_
#define NETPGP_VERIFY_H_	20170201

#define NETPGPVERIFY_VERSION	"netpgpverify portable 20170201"

#include <sys/types.h>

#include <inttypes.h>

struct pgpv_t;
typedef struct pgpv_t	pgpv_t;

struct pgpv_cursor_t;
typedef struct pgpv_cursor_t	pgpv_cursor_t;

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

pgpv_t *pgpv_new(void);
pgpv_cursor_t *pgpv_new_cursor(void);

int pgpv_read_pubring(pgpv_t */*pgp*/, const void */*keyringfile/mem*/, ssize_t /*size*/);
int pgpv_read_ssh_pubkeys(pgpv_t */*pgp*/, const void */*keyring*/, ssize_t /*size*/);

size_t pgpv_verify(pgpv_cursor_t */*cursor*/, pgpv_t */*pgp*/, const void */*mem/file*/, ssize_t /*size*/);
size_t pgpv_get_verified(pgpv_cursor_t */*cursor*/, size_t /*cookie*/, char **/*ret*/);
size_t pgpv_dump(pgpv_t */*pgp*/, char **/*data*/);

size_t pgpv_get_entry(pgpv_t */*pgp*/, unsigned /*ent*/, char **/*ret*/, const char */*modifiers*/);

int64_t pgpv_get_cursor_num(pgpv_cursor_t */*cursor*/, const char */*field*/);
char *pgpv_get_cursor_str(pgpv_cursor_t */*cursor*/, const char */*field*/);
int pgpv_get_cursor_element(pgpv_cursor_t */*cursor*/, size_t /*element*/);

int pgpv_close(pgpv_t */*pgp*/);
int pgpv_cursor_close(pgpv_cursor_t */*cursor*/);

__END_DECLS

#endif
