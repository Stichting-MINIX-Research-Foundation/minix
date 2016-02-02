/*	$NetBSD: bootcfg.c,v 1.2 2014/08/10 07:40:49 isaki Exp $	*/

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

#include <sys/types.h>
#include <sys/reboot.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/bootcfg.h>
#include <lib/libkern/libkern.h>

#define MENUFORMAT_AUTO   0
#define MENUFORMAT_NUMBER 1
#define MENUFORMAT_LETTER 2

#define DEFAULT_FORMAT  MENUFORMAT_AUTO
#define DEFAULT_TIMEOUT 10

struct bootcfg_def bootcfg_info;

void
bootcfg_do_noop(const char *cmd, char *arg)
{
	/* noop, do nothing */
}

/*
 * This function parses a boot.cfg file in the root of the filesystem
 * (if present) and populates the global boot configuration.
 *
 * The file consists of a number of lines each terminated by \n
 * The lines are in the format keyword=value. There should not be spaces
 * around the = sign.
 *
 * perform_bootcfg(conf, command, maxsz)
 *
 * conf		Path to boot.cfg to be passed verbatim to open()
 *
 * command	Pointer to a function that will be called when
 * 		perform_bootcfg() encounters a key (command) it does not
 *		recognize.
 *		The command function is provided both the keyword and
 *		value parsed as arguments to the function.
 *
 * maxsz	Limit the size of the boot.cfg perform_bootcfg() will parse.
 * 		- If maxsz is < 0 boot.cfg will not be processed.
 * 		- If maxsz is = 0 no limit will be imposed but parsing may
 *		  fail due to platform or other constraints e.g. maximum
 *		  segment size.
 *		- If 0 < maxsz and boot.cfg exceeds maxsz it will not be
 *		  parsed, otherwise it will be parsed.
 *
 * The recognised keywords are:
 * banner: text displayed instead of the normal welcome text
 * menu: Descriptive text:command to use
 * timeout: Timeout in seconds (overrides that set by installboot)
 * default: the default menu option to use if Return is pressed
 * consdev: the console device to use
 * format: how menu choices are displayed: (a)utomatic, (n)umbers or (l)etters
 * clear: whether to clear the screen or not
 *
 * Example boot.cfg file:
 * banner=Welcome to NetBSD
 * banner=Please choose the boot type from the following menu
 * menu=Boot NetBSD:boot netbsd
 * menu=Boot into single user mode:boot netbsd -s
 * menu=:boot hd1a:netbsd -cs
 * menu=Goto boot comand line:prompt
 * timeout=10
 * consdev=com0
 * default=1
*/
void
perform_bootcfg(const char *conf, bootcfg_command command, const off_t maxsz)
{
	char *bc, *c;
	int cmenu, cbanner, len;
	int fd, err, off;
	struct stat st;
	char *next, *key, *value, *v2;

	/* clear bootcfg structure */
	memset(&bootcfg_info, 0, sizeof(bootcfg_info));

	/* set default timeout */
	bootcfg_info.timeout = DEFAULT_TIMEOUT;

	/* automatically switch between letter and numbers on menu */
	bootcfg_info.menuformat = DEFAULT_FORMAT;

	fd = open(conf, 0);
	if (fd < 0)
		return;

	err = fstat(fd, &st);
	if (err == -1) {
		close(fd);
		return;
	}

	/* if a maximum size is being requested for the boot.cfg enforce it. */
	if (0 < maxsz && st.st_size > maxsz) {
		close(fd);
		return;
	}

	bc = alloc(st.st_size + 1);
	if (bc == NULL) {
		printf("Could not allocate memory for boot configuration\n");
		close(fd);
		return;
	}

	/*
	 * XXX original code, assumes error or eof return from read()
	 *     results in the entire boot.cfg being buffered.
	 *     - should bail out on read() failing.
	 *     - assumption is made that the file size doesn't change between
	 *       fstat() and read()ing.  probably safe in this context
	 *       arguably should check that reading the file won't overflow
	 *       the storage anyway.
	 */
	off = 0;
	do {
		len = read(fd, bc + off, 1024);
		if (len <= 0)
			break;
		off += len;
	} while (len > 0);
	bc[off] = '\0';

	close(fd);

	/* bc is now assumed to contain the whole boot.cfg file (see above) */

	cmenu = 0;
	cbanner = 0;
	for (c = bc; *c; c = next) {
		key = c;
		/* find end of line */
		for (; *c && *c != '\n'; c++)
			/* zero terminate line on start of comment */
			if (*c == '#')
				*c = 0;
		/* zero terminate line */
		if (*(next = c))
			*next++ = 0;
		/* Look for = separator between key and value */
		for (c = key; *c && *c != '='; c++)
			continue;
		/* Ignore lines with no key=value pair */
		if (*c == '\0')
			continue;

		/* zero terminate key which points to keyword */
		*c++ = 0;
		value = c;
		/* Look for end of line (or file) and zero terminate value */
		for (; *c && *c != '\n'; c++)
			continue;
		*c = 0;

		if (!strncmp(key, "menu", 4)) {
			/*
			 * Parse "menu=<description>:<command>".  If the
			 * description is empty ("menu=:<command>)",
			 * then re-use the command as the description.
			 * Note that the command may contain embedded
			 * colons.
			 */
			if (cmenu >= BOOTCFG_MAXMENU)
				continue;
			bootcfg_info.desc[cmenu] = value;
			for (v2 = value; *v2 && *v2 != ':'; v2++)
				continue;
			if (*v2) {
				*v2++ = 0;
				bootcfg_info.command[cmenu] = v2;
				if (! *value)
					bootcfg_info.desc[cmenu] = v2;
				cmenu++;
			} else {
				/* No delimiter means invalid line */
				bootcfg_info.desc[cmenu] = NULL;
			}
		} else if (!strncmp(key, "banner", 6)) {
			if (cbanner < BOOTCFG_MAXBANNER)
				bootcfg_info.banner[cbanner++] = value;
		} else if (!strncmp(key, "timeout", 7)) {
			if (!isdigit(*value))
				bootcfg_info.timeout = -1;
			else
				bootcfg_info.timeout = atoi(value);
		} else if (!strncmp(key, "default", 7)) {
			bootcfg_info.def = atoi(value) - 1;
		} else if (!strncmp(key, "consdev", 7)) {
			bootcfg_info.consdev = value;
		} else if (!strncmp(key, BOOTCFG_CMD_LOAD, 4)) {
			command(BOOTCFG_CMD_LOAD, value);
		} else if (!strncmp(key, "format", 6)) {
			printf("value:%c\n", *value);
			switch (*value) {
			case 'a':
			case 'A':
				bootcfg_info.menuformat = MENUFORMAT_AUTO;
				break;

			case 'n':
			case 'N':
			case 'd':
			case 'D':
				bootcfg_info.menuformat = MENUFORMAT_NUMBER;
				break;

			case 'l':
			case 'L':
				bootcfg_info.menuformat = MENUFORMAT_LETTER;
				break;
			}
		} else if (!strncmp(key, "clear", 5)) {
			bootcfg_info.clear = !!atoi(value);
		} else if (!strncmp(key, BOOTCFG_CMD_USERCONF, 8)) {
			command(BOOTCFG_CMD_USERCONF, value);
		} else {
			command(key, value);
		}
	}

	switch (bootcfg_info.menuformat) {
	case MENUFORMAT_AUTO:
		if (cmenu > 9 && bootcfg_info.timeout > 0)
			bootcfg_info.menuformat = MENUFORMAT_LETTER;
		else
			bootcfg_info.menuformat = MENUFORMAT_NUMBER;
		break;

	case MENUFORMAT_NUMBER:
		if (cmenu > 9 && bootcfg_info.timeout > 0)
			cmenu = 9;
		break;
	}

	bootcfg_info.nummenu = cmenu;
	if (bootcfg_info.def < 0)
		bootcfg_info.def = 0;
	if (bootcfg_info.def >= cmenu)
		bootcfg_info.def = cmenu - 1;
}
