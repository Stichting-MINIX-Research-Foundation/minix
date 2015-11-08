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

/*
 *  Top users/processes display for Unix
 */

/* prototypes for functions found in utils.c */

#ifndef _UTILS_H
#define _UTILS_H

int atoiwi(char *);
char *itoa(int);
char *itoa_w(int, int);
char *itoa7(int);
int digits(int);
char *printable(char *);
char *strcpyend(char *, const char *);
char *homogenize(const char *);
int string_index(const char *, const char **);
char **argparse(char *, int *);
long percentages(int, int *, long *, long *, long *);
const char *errmsg(int);
char *format_percent(double);
char *format_time(long);
char *format_k(long);
char *string_list(const char **);
void time_get(struct timeval *);
void time_mark(struct timeval *);
void double2tv(struct timeval *, double);
unsigned int time_elapsed(void);
unsigned int diff_per_second(unsigned int, unsigned int);
void debug_set(int);
#ifdef DEBUG
#define dprintf xdprintf
void xdprintf(char *fmt, ...);
#else
#ifdef HAVE_C99_VARIADIC_MACROS
#define dprintf(...) 
#else
#ifdef HAVE_GNU_VARIADIC_MACROS
#define dprintf(x...) 
#else
#define dprintf if (0)
#endif
#endif
#endif

#endif
