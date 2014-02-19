/*
 * $OpenBSD: util.h,v 1.15 2005/06/20 07:14:06 otto Exp $
 * $DragonFly: src/usr.bin/patch/util.h,v 1.2 2007/09/29 23:11:10 swildner Exp $
 * $NetBSD: util.h,v 1.12 2011/09/06 18:25:14 joerg Exp $
 */

/*
 * patch - a program to apply diffs to original files
 * 
 * Copyright 1986, Larry Wall
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following condition is met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this condition and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * -C option added in 1998, original code by Marc Espie, based on FreeBSD
 * behaviour
 */

char		*fetchname(const char *, bool *, int);
char		*checked_in(char *);
int		backup_file(const char *);
int		move_file(const char *, const char *);
int		copy_file(const char *, const char *);
void		say(const char *, ...) __printflike(1, 2);
void		fatal(const char *, ...) __printflike(1, 2) __dead;
void		pfatal(const char *, ...) __printflike(1, 2) __dead;
void		ask(const char *, ...) __printflike(1, 2);
char		*savestr(const char *);
void		set_signals(int);
void		ignore_signals(void);
void		makedirs(const char *, bool);
void		version(void) __dead;
void		my_exit(int) __dead;

/* in mkpath.c */
extern int mkpath(char *);
