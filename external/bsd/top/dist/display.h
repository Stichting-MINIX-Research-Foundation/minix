/*
 * Copyright (c) 1984 through 2008, William LeFebvre
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * 
 *     * Neither the name of William LeFebvre nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* interface declaration for display.c */

#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "globalstate.h"

void display_clear(void);
int display_resize(void);
int display_lines(void);
int display_setmulti(int m);
int display_columns(void);
int display_init(struct statics *statics, int percpuinfo);
void i_loadave(int mpid, double *avenrun);
void u_loadave(int mpid, double *avenrun);
void i_minibar(int (*formatter)(char *, int));
void u_minibar(int (*formatter)(char *, int));
void i_uptime(time_t *bt, time_t *tod);
void u_uptime(time_t *bt, time_t *tod);
void i_timeofday(time_t *tod);
void i_procstates(int total, int *brkdn, int threads);
void u_procstates(int total, int *brkdn, int threads);
void i_cpustates(int *states);
void u_cpustates(int *states);
void z_cpustates(void);
void i_kernel(int *stats);
void u_kernel(int *stats);
void i_memory(long *stats);
void u_memory(long *stats);
void i_swap(long *stats);
void u_swap(long *stats);
void i_message(struct timeval *now);
void u_message(struct timeval *now);
void i_header(char *text);
void u_header(char *text);
void i_process(int line, char *thisline);
void u_process(int, char *);
void i_endscreen(void);
void u_endscreen(void);
void display_header(int t);
void new_message(const char *msgfmt, ...);
void message_error(const char *msgfmt, ...);
void message_mark(void);
void message_clear(void);
void message_expire(void);
void message_prompt(const char *msgfmt, ...);
void message_prompt_plain(const char *msgfmt, ...);
int readline(char *buffer, int size, int numeric);
void display_pagerstart(void);
void display_pagerend(void);
void display_pager(const char *fmt, ...);

#endif
