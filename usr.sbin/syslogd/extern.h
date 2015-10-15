/*	$NetBSD: extern.h,v 1.4 2015/02/10 20:38:15 christos Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Schütte.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * extern.h
 *
 * declarations for variables and functions from syslogd.c
 * that are used in tls.c and sign.c
 */
#ifndef EXTERN_H_
#define EXTERN_H_


/* variables */
extern int Debug;
extern struct tls_global_options_t tls_opt;
extern struct TLS_Incoming TLS_Incoming_Head;
extern struct sign_global_t GlobalSign;
extern char  *linebuf;
extern size_t linebufsize;
extern int    RemoteAddDate;

extern bool	BSDOutputFormat;
extern time_t	now;
extern char	timestamp[];
extern char	appname[];
extern char    *LocalFQDN;
extern char    *include_pid;

/* functions */
extern void	logerror(const char *, ...)
    __attribute__((__format__(__printf__,1,2)));
extern void	loginfo(const char *, ...)
    __attribute__((__format__(__printf__,1,2)));
extern void	printline(const char *, char *, int);
extern void	die(int fd, short event, void *ev)
    __attribute__((__noreturn__));
extern struct event *allocev(void);
extern void	send_queue(int __unused, short __unused, void *);
extern void	schedule_event(struct event **, struct timeval *,
    void (*)(int, short, void *), void *);
extern char    *make_timestamp(time_t *, bool, size_t);
#ifndef DISABLE_TLS
extern struct filed *get_f_by_conninfo(struct tls_conn_settings *conn_info);
#endif
extern bool	message_queue_remove(struct filed *, struct buf_queue *);
extern void	buf_msg_free(struct buf_msg *msg);
extern void	message_queue_freeall(struct filed *);
extern bool	copy_string(char **, const char *, const char *);
extern bool	copy_config_value_quoted(const char *, char **, const char **);
extern size_t message_allqueues_purge(void);
extern bool  format_buffer(struct buf_msg*, char**, size_t*, size_t*, size_t*,
    size_t*);
extern void  fprintlog(struct filed *, struct buf_msg *, struct buf_queue *);
extern struct buf_msg *buf_msg_new(const size_t);

#endif /*EXTERN_H_*/
