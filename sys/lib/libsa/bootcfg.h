/*	$NetBSD: bootcfg.h,v 1.1 2014/06/28 09:16:18 rtr Exp $	*/

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

#ifndef _BOOTCFG_H
#define _BOOTCFG_H

#define BOOTCFG_FILENAME "boot.cfg"
#define BOOTCFG_MAXMENU	 20
#define BOOTCFG_MAXBANNER 12

#define BOOTCFG_CMD_LOAD	  "load"
#define BOOTCFG_CMD_USERCONF	  "userconf"

typedef void (*bootcfg_command)(const char *cmd, char *arg);

struct bootcfg_def {
	char *banner[BOOTCFG_MAXBANNER];	/* Banner text */
	char *command[BOOTCFG_MAXMENU];		/* Menu commands per entry*/
	char *consdev;				/* Console device */
	int def;				/* Default menu option */
	char *desc[BOOTCFG_MAXMENU];		/* Menu text per entry */
	int nummenu;				/* Number of menu items */
	int timeout;		 		/* Timeout in seconds */
	int menuformat;				/* Letters instead of numbers */
	int clear;				/* Clear the screen? */
} extern bootcfg_info;

void perform_bootcfg(const char *, bootcfg_command, const off_t);
void bootcfg_do_noop(const char *, char *);

#endif /* !_BOOTCFG_H */
