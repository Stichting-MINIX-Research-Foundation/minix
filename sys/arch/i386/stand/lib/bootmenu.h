/*	$NetBSD: bootmenu.h,v 1.2 2008/12/13 23:30:54 christos Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
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

#ifndef _BOOTMENU_H
#define _BOOTMENU_H

#define BOOTCONF "boot.cfg"
#define MAXMENU 20
#define MAXBANNER 12
#define COMMAND_SEPARATOR ';'

void parsebootconf(const char *);
void doboottypemenu(void);
int atoi(const char *);

struct bootconf_def {
	char *banner[MAXBANNER];	/* Banner text */
	char *command[MAXMENU];		/* Menu commands per entry*/
	char *consdev;			/* Console device */
	int def;			/* Default menu option */
	char *desc[MAXMENU];		/* Menu text per entry */
	int nummenu;			/* Number of menu items */
	int timeout;		 	/* Timeout in seconds */
	int menuformat;			/* Print letters instead of numbers? */
	int clear;			/* Clear the screen? */
} extern bootconf;

#endif /* !_BOOTMENU_H */
