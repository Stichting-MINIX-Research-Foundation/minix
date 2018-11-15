/*	$NetBSD: otp_challenge.c,v 1.2 2017/01/28 21:31:50 christos Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
__RCSID("$NetBSD: otp_challenge.c,v 1.2 2017/01/28 21:31:50 christos Exp $");
#endif

#include "otp_locl.h"

int
otp_challenge (OtpContext *ctx, char *user, char *str, size_t len)
{
  void *dbm;
  int ret;

  ctx->challengep = 0;
  ctx->err = NULL;
  ctx->user = strdup(user);
  if (ctx->user == NULL) {
    ctx->err = "Out of memory";
    return -1;
  }
  dbm = otp_db_open ();
  if (dbm == NULL) {
    ctx->err = "Cannot open database";
    return -1;
  }
  ret = otp_get (dbm, ctx);
  otp_db_close (dbm);
  if (ret)
    return ret;
  snprintf (str, len,
	    "[ otp-%s %u %s ]",
	    ctx->alg->name, ctx->n-1, ctx->seed);
  ctx->challengep = 1;
  return 0;
}
