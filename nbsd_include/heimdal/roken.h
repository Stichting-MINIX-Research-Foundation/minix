/* This is an OS dependent, generated file */


#ifndef __ROKEN_H__
#define __ROKEN_H__

/* -*- C -*- */
/*
 * Copyright (c) 1995-2005 Kungliga Tekniska Högskolan
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

/* $Id: roken.h,v 1.14 2010/01/25 00:26:04 christos Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include <sys/param.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <grp.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <strings.h>

#include <paths.h>


#include <roken-common.h>

ROKEN_CPP_START

#define rk_UNCONST(x) ((void *)(uintptr_t)(const void *)(x))



char * ROKEN_LIB_FUNCTION strlwr(char *);

size_t ROKEN_LIB_FUNCTION strnlen(const char*, size_t);


ssize_t ROKEN_LIB_FUNCTION strsep_copy(const char**, const char*, char*, size_t);




char * ROKEN_LIB_FUNCTION strupr(char *);











#include <pwd.h>
struct passwd * ROKEN_LIB_FUNCTION k_getpwnam (const char *);
struct passwd * ROKEN_LIB_FUNCTION k_getpwuid (uid_t);

const char * ROKEN_LIB_FUNCTION get_default_username (void);




int ROKEN_LIB_FUNCTION mkstemp(char *);




int ROKEN_LIB_FUNCTION daemon(int, int);














time_t ROKEN_LIB_FUNCTION tm2time (struct tm, int);

int ROKEN_LIB_FUNCTION unix_verify_user(char *, char *);

int ROKEN_LIB_FUNCTION roken_concat (char *, size_t, ...);

size_t ROKEN_LIB_FUNCTION roken_mconcat (char **, size_t, ...);

int ROKEN_LIB_FUNCTION roken_vconcat (char *, size_t, va_list);

size_t ROKEN_LIB_FUNCTION
    roken_vmconcat (char **, size_t, va_list);

ssize_t ROKEN_LIB_FUNCTION net_write (int, const void *, size_t);

ssize_t ROKEN_LIB_FUNCTION net_read (int, void *, size_t);

int ROKEN_LIB_FUNCTION issuid(void);


int ROKEN_LIB_FUNCTION get_window_size(int fd, int *, int *);



extern char **environ;

struct hostent * ROKEN_LIB_FUNCTION
getipnodebyname (const char *, int, int, int *);

struct hostent * ROKEN_LIB_FUNCTION
getipnodebyaddr (const void *, size_t, int, int *);

void ROKEN_LIB_FUNCTION
freehostent (struct hostent *);

struct hostent * ROKEN_LIB_FUNCTION
copyhostent (const struct hostent *);








int ROKEN_LIB_FUNCTION
getnameinfo_verified(const struct sockaddr *, socklen_t,
		     char *, size_t,
		     char *, size_t,
		     int);

int ROKEN_LIB_FUNCTION
roken_getaddrinfo_hostspec(const char *, int, struct addrinfo **); 
int ROKEN_LIB_FUNCTION
roken_getaddrinfo_hostspec2(const char *, int, int, struct addrinfo **);



void * ROKEN_LIB_FUNCTION emalloc (size_t);
void * ROKEN_LIB_FUNCTION ecalloc(size_t, size_t);
void * ROKEN_LIB_FUNCTION erealloc (void *, size_t);
char * ROKEN_LIB_FUNCTION estrdup (const char *);

/*
 * kludges and such
 */

int ROKEN_LIB_FUNCTION
roken_gethostby_setup(const char*, const char*);
struct hostent* ROKEN_LIB_FUNCTION
roken_gethostbyname(const char*);
struct hostent* ROKEN_LIB_FUNCTION 
roken_gethostbyaddr(const void*, size_t, int);

#define roken_getservbyname(x,y) getservbyname(x,y)

#define roken_openlog(a,b,c) openlog(a,b,c)

#define roken_getsockname(a,b,c) getsockname(a,b,c)




void ROKEN_LIB_FUNCTION mini_inetd_addrinfo (struct addrinfo*);
void ROKEN_LIB_FUNCTION mini_inetd (int);












ROKEN_CPP_END
#define ROKEN_VERSION 1.1

#endif /* __ROKEN_H__ */
