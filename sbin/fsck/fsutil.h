/*	$NetBSD: fsutil.h,v 1.17 2011/06/09 21:23:29 christos Exp $	*/

/*
 * Copyright (c) 1996 Christos Zoulas.  All rights reserved.
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

#include <stdarg.h>
#include <signal.h>

void errexit(const char *, ...)
    __attribute__((__noreturn__,__format__(__printf__,1,2)));  
void pfatal(const char *, ...)
    __attribute__((__format__(__printf__,1,2)));  
void pwarn(const char *, ...)
    __attribute__((__format__(__printf__,1,2)));  
void perr(const char *, ...)
    __attribute__((__format__(__printf__,1,2)));  
void panic(const char *, ...)
    __attribute__((__noreturn__,__format__(__printf__,1,2)));  
void vmsg(int, const char *, va_list)
     __attribute__((__format__(__printf__,2,0)));
const char *rawname(const char *);
const char *unrawname(const char *);
const char *blockcheck(const char *);
const char *cdevname(void);
void setcdevname(const char *, int);
int  hotroot(void);
const char *print_mtime(time_t);

#define CHECK_PREEN	1
#define	CHECK_VERBOSE	2
#define	CHECK_DEBUG	4
#define	CHECK_FORCE	8
#define	CHECK_PROGRESS	16
#define	CHECK_NOFIX	32

struct fstab;
int checkfstab(int, int, void *(*)(struct fstab *), 
    int (*) (const char *, const char *, const char *, void *, pid_t *));

void (*ckfinish)(int);
volatile sig_atomic_t returntosingle;
void catch(int);
void catchquit(int);
void voidquit(int);
