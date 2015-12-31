/*	$NetBSD: otp.c,v 1.1.1.2 2014/04/24 12:45:51 pettai Exp $	*/

/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
__RCSID("NetBSD");
#endif

#include "otp_locl.h"
#include "otp_md.h"

static OtpAlgorithm algorithms[] = {
  {OTP_ALG_MD4, "md4", 16, otp_md4_hash, otp_md4_init, otp_md4_next},
  {OTP_ALG_MD5, "md5", 16, otp_md5_hash, otp_md5_init, otp_md5_next},
  {OTP_ALG_SHA, "sha", 20, otp_sha_hash, otp_sha_init, otp_sha_next}
};

OtpAlgorithm *
otp_find_alg (char *name)
{
  int i;

  for (i = 0; i < sizeof(algorithms)/sizeof(*algorithms); ++i)
    if (strcmp (name, algorithms[i].name) == 0)
      return &algorithms[i];
  return NULL;
}

char *
otp_error (OtpContext *o)
{
  return o->err;
}
