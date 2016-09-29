/**
 * @file
 * Sockets API internal implementations (do not use in application code)
 */

/*
 * Copyright (c) 2017 Joel Cunningham, Garmin International, Inc. <joel.cunningham@garmin.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Joel Cunningham <joel.cunningham@me.com>
 *
 */
#ifndef LWIP_HDR_SOCKETS_PRIV_H
#define LWIP_HDR_SOCKETS_PRIV_H

#include "lwip/opt.h"

#if LWIP_SOCKET /* don't build if not configured for use in lwipopts.h */

#include "lwip/err.h"
#include "lwip/sockets.h"

#ifdef __cplusplus
extern "C" {
#endif

#if LWIP_SOCKET_SET_ERRNO
#ifndef set_errno
#define set_errno(err) do { if (err) { errno = (err); } } while(0)
#endif
#else /* LWIP_SOCKET_SET_ERRNO */
#define set_errno(err)
#endif /* LWIP_SOCKET_SET_ERRNO */

#if !LWIP_TCPIP_CORE_LOCKING
/** Maximum optlen used by setsockopt/getsockopt */
#define LWIP_SETGETSOCKOPT_MAXOPTLEN 16

/** This struct is used to pass data to the set/getsockopt_internal
 * functions running in tcpip_thread context (only a void* is allowed) */
struct lwip_setgetsockopt_data {
  /** socket index for which to change options */
  int s;
  /** level of the option to process */
  int level;
  /** name of the option to process */
  int optname;
  /** set: value to set the option to
    * get: value of the option is stored here */
#if LWIP_MPU_COMPATIBLE
  u8_t optval[LWIP_SETGETSOCKOPT_MAXOPTLEN];
#else
  union {
    void *p;
    const void *pc;
  } optval;
#endif
  /** size of *optval */
  socklen_t optlen;
  /** if an error occurs, it is temporarily stored here */
  int err;
  /** semaphore to wake up the calling task */
  void* completed_sem;
};
#endif /* !LWIP_TCPIP_CORE_LOCKING */

#ifdef __cplusplus
}
#endif

#endif /* LWIP_SOCKET */

#endif /* LWIP_HDR_SOCKETS_PRIV_H */
