/*	$NetBSD: bootmenu.c,v 1.5 2022/06/08 21:55:51 wiz Exp $	*/

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

#ifndef SMALL

#include <sys/types.h>
#include <sys/reboot.h>
#include <sys/bootblock.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/bootcfg.h>
#include <lib/libsa/ufs.h>
#include <lib/libkern/libkern.h>

#include "bootmenu.h"
#include "efiboot.h"
#include "module.h"
#include "overlay.h"

static void docommandchoice(int);

extern	const char bootprog_name[], bootprog_rev[], bootprog_kernrev[];

#define MENUFORMAT_AUTO	  0
#define MENUFORMAT_NUMBER 1
#define MENUFORMAT_LETTER 2

/*
 * XXX
 * if module_add, userconf_add are strictly mi they can be folded back
 * into sys/lib/libsa/bootcfg.c:perform_bootcfg().
 */
static void
do_bootcfg_command(const char *cmd, char *arg)
{
	if (strcmp(cmd, BOOTCFG_CMD_LOAD) == 0)
		module_add(arg);
	else if (strcmp(cmd, BOOTCFG_CMD_USERCONF) == 0)
		userconf_add(arg);
#ifdef EFIBOOT_FDT
	else if (strcmp(cmd, "dtoverlay") == 0)
		dtoverlay_add(arg);
#endif
}

int
parsebootconf(const char *conf)
{
	return perform_bootcfg(conf, &do_bootcfg_command, 32768);
}

/*
 * doboottypemenu will render the menu and parse any user input
 */
static int
getchoicefrominput(char *input, int def)
{
	int choice, usedef;

	choice = -1;
	usedef = 0;

	if (*input == '\0' || *input == '\r' || *input == '\n') {
		choice = def;
		usedef = 1;
	} else if (*input >= 'A' && *input < bootcfg_info.nummenu + 'A')
		choice = (*input) - 'A';
	else if (*input >= 'a' && *input < bootcfg_info.nummenu + 'a')
		choice = (*input) - 'a';
	else if (isdigit(*input)) {
		choice = atoi(input) - 1;
		if (choice < 0 || choice >= bootcfg_info.nummenu)
			choice = -1;
	}

	if (bootcfg_info.menuformat != MENUFORMAT_LETTER &&
	    !isdigit(*input) && !usedef)
		choice = -1;

	return choice;
}

static void
docommandchoice(int choice)
{
	char input[80], *ic, *oc;

	ic = bootcfg_info.command[choice];
	/* Split command string at ; into separate commands */
	do {
		oc = input;
		/* Look for ; separator */
		for (; *ic && *ic != COMMAND_SEPARATOR; ic++)
			*oc++ = *ic;
		if (*input == '\0')
			continue;
		/* Strip out any trailing spaces */
		oc--;
		for (; *oc == ' ' && oc > input; oc--);
		*++oc = '\0';
		if (*ic == COMMAND_SEPARATOR)
			ic++;
		/* Stop silly command strings like ;;; */
		if (*input != '\0')
			docommand(input);
		/* Skip leading spaces */
		for (; *ic == ' '; ic++);
	} while (*ic);
}

__dead void
doboottypemenu(void)
{
	int choice;
	char input[80];

	printf("\n");
	/* Display menu */
	if (bootcfg_info.menuformat == MENUFORMAT_LETTER) {
		for (choice = 0; choice < bootcfg_info.nummenu; choice++)
			printf("    %c. %s\n", choice + 'A',
			    bootcfg_info.desc[choice]);
	} else {
		/* Can't use %2d format string with libsa */
		for (choice = 0; choice < bootcfg_info.nummenu; choice++)
			printf("    %s%d. %s\n",
			    (choice < 9) ?  " " : "",
			    choice + 1,
			    bootcfg_info.desc[choice]);
	}
	choice = -1;
	for (;;) {
		input[0] = '\0';

		if (bootcfg_info.timeout < 0) {
			if (bootcfg_info.menuformat == MENUFORMAT_LETTER)
				printf("\nOption: [%c]:",
				    bootcfg_info.def + 'A');
			else
				printf("\nOption: [%d]:",
				    bootcfg_info.def + 1);

			kgets(input, sizeof(input));
			choice = getchoicefrominput(input, bootcfg_info.def);
		} else if (bootcfg_info.timeout == 0)
			choice = bootcfg_info.def;
		else  {
			printf("\nChoose an option; RETURN for default; "
			       "SPACE to stop countdown.\n");
			if (bootcfg_info.menuformat == MENUFORMAT_LETTER)
				printf("Option %c will be chosen in ",
				    bootcfg_info.def + 'A');
			else
				printf("Option %d will be chosen in ",
				    bootcfg_info.def + 1);
			input[0] = awaitkey(bootcfg_info.timeout, 1);
			input[1] = '\0';
			choice = getchoicefrominput(input, bootcfg_info.def);
			/* If invalid key pressed, drop to menu */
			if (choice == -1)
				bootcfg_info.timeout = -1;
		}
		if (choice < 0)
			continue;
		if (!strcmp(bootcfg_info.command[choice], "prompt")) {
			printf("type \"?\" or \"help\" for help.\n");
			bootprompt(); /* does not return */
		} else {
			docommandchoice(choice);
		}

	}
}

#endif	/* !SMALL */
