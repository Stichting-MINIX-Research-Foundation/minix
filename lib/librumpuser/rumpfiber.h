/*	$NetBSD: rumpfiber.h,v 1.4 2015/02/15 00:54:32 justin Exp $	*/

/*
 * Copyright (c) 2014 Justin Cormack.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>

#include <stdint.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

struct thread {
    char *name;
    void *lwp;
    void *cookie;
    int64_t wakeup_time;
    TAILQ_ENTRY(thread) thread_list;
    ucontext_t ctx;
    int flags;
    int threrrno;
};

#define RUNNABLE_FLAG   0x00000001
#define THREAD_MUSTJOIN 0x00000002
#define THREAD_JOINED   0x00000004
#define THREAD_EXTSTACK 0x00000008
#define THREAD_TIMEDOUT 0x00000010

#define STACKSIZE 65536

/* used by experimental _lwp code */
void schedule(void);
void wake(struct thread *thread);
void block(struct thread *thread);
struct thread *init_mainthread(void *);
void exit_thread(void) __attribute__((noreturn));
void set_sched_hook(void (*f)(void *, void *));
int abssleep_real(uint64_t millisecs);
struct thread* create_thread(const char *name, void *cookie,
			     void (*f)(void *), void *data,
			     void *stack, size_t stack_size);
int is_runnable(struct thread *);
void set_runnable(struct thread *);
void clear_runnable(struct thread *);

