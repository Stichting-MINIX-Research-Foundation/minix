/*	$NetBSD: bootmenu.c,v 1.14 2014/08/10 07:40:49 isaki Exp $	*/

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

#include <libi386.h>
#include <bootmenu.h>

static void docommandchoice(int);

extern struct x86_boot_params boot_params;
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
}

void
parsebootconf(const char *conf)
{
	perform_bootcfg(conf, &do_bootcfg_command, 32768);
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

void
bootdefault(void)
{
	int choice;
	static int entered;

	if (bootcfg_info.nummenu > 0) {
		if (entered) {
			printf("default boot twice, skipping...\n");
			return;
		}
		entered = 1;
		choice = bootcfg_info.def;
		printf("command(s): %s\n", bootcfg_info.command[choice]);
		docommandchoice(choice);
	}
}

#if defined(__minix)
static void
showmenu(void)
{
	int choice;

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
}
#endif /* defined(__minix) */

__dead void
doboottypemenu(void)
{
#if !defined(__minix)
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
#else
	int choice, editing;
	char input[256], *ic, *oc;
#endif /* !defined(__minix) */
#if defined(__minix)
	showmenu();
#endif /* defined(__minix) */
	choice = -1;
#if defined(__minix)
	editing = 0;
#endif /* defined(__minix) */
	for (;;) {
		input[0] = '\0';

		if (bootcfg_info.timeout < 0) {
			if (bootcfg_info.menuformat == MENUFORMAT_LETTER)
#if !defined(__minix)
				printf("\nOption: [%c]:",
#else
				printf("\nOption%s: [%c]:",
				    editing ? " (edit)" : "",
#endif /* !defined(__minix) */
				    bootcfg_info.def + 'A');
			else
#if !defined(__minix)
				printf("\nOption: [%d]:",
#else
				printf("\nOption%s: [%d]:",
				    editing ? " (edit)" : "",
#endif /* !defined(__minix) */
				    bootcfg_info.def + 1);

#if !defined(__minix)
			gets(input);
#else
			editline(input, sizeof(input), NULL);
#endif /* !defined(__minix) */
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
#if !defined(__minix)
		if (!strcmp(bootcfg_info.command[choice], "prompt") &&
		    ((boot_params.bp_flags & X86_BP_FLAGS_PASSWORD) == 0 ||
		    check_password((char *)boot_params.bp_password))) {
			printf("type \"?\" or \"help\" for help.\n");
			bootmenu(); /* does not return */
		} else {
			docommandchoice(choice);
		}
#else
		ic = bootcfg_info.command[choice];
		if (editing) {
			printf("> ");
			editline(input, sizeof(input), ic);
			ic = input;
		}
		if (!strcmp(ic, "edit") &&
		    ((boot_params.bp_flags & X86_BP_FLAGS_PASSWORD) == 0 ||
		    check_password((char *)boot_params.bp_password))) {
			editing = 1;
			bootcfg_info.timeout = -1;
		} else if (!strcmp(ic, "prompt") &&
		    ((boot_params.bp_flags & X86_BP_FLAGS_PASSWORD) == 0 ||
		    check_password((char *)boot_params.bp_password))) {
			printf("type \"?\" or \"help\" for help, "
			    "or \"menu\" to return to the menu.\n");
			prompt(1);
			showmenu();
			editing = 0;
			bootcfg_info.timeout = -1;
		} else {
			/* Split command string at ; into separate commands */
			do {
				/*
				 * This must support inline editing, since ic
				 * may also point to input.
				 */
				oc = input;
				/* Look for ; separator */
				for (; *ic && *ic != COMMAND_SEPARATOR; ic++)
					*oc++ = *ic;
				if (*ic == COMMAND_SEPARATOR)
					ic++;
				if (oc == input)
					continue;
				/* Strip out any trailing spaces */
				oc--;
				for (; *oc == ' ' && oc > input; oc--);
				*++oc = '\0';
				/* Stop silly command strings like ;;; */
				if (*input != '\0')
					docommand(input);
				/* Skip leading spaces */
				for (; *ic == ' '; ic++);
			} while (*ic);
		}
#endif /* !defined(__minix) */

	}
}

#endif	/* !SMALL */
