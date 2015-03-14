/*	$NetBSD: init.c,v 1.18 2008/08/08 16:10:47 drochner Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Timothy C. Stoehr.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)init.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: init.c,v 1.18 2008/08/08 16:10:47 drochner Exp $");
#endif
#endif /* not lint */

/*
 * init.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include <stdlib.h>
#include <fcntl.h>

#include "rogue.h"

static void do_args(int, char **);
static void do_opts(void);
static void env_get_value(char **, char *, boolean);
static void init_str(char **, const char *);
static void player_init(void);

static char *rest_file = NULL;
static boolean init_curses = 0;

char login_name[MAX_OPT_LEN];
char *nick_name = NULL;
boolean cant_int = 0;
boolean did_int = 0;
boolean score_only;
boolean save_is_interactive = 1;
boolean ask_quit = 1;
boolean no_skull = 0;
boolean passgo = 0;
const char *error_file = "rogue.esave";
const char *byebye_string = "Okay, bye bye!";
gid_t gid, egid;

int
init(int argc, char *argv[])
{
	const char *pn;
	int seed;
	int fd;

	gid = getgid();
	egid = getegid();
	setegid(gid);
	/* Check for dirty tricks with closed fds 0, 1, 2 */
	fd = open("/dev/null", O_RDONLY);
	if (fd < 3)
		exit(1);
	close(fd);

	seed = 0;
	pn = md_gln();
	if ((!pn) || (strlen(pn) >= MAX_OPT_LEN)) {
		clean_up("Hey!  Who are you?");
	}
	/* LOGIN_NAME_SIZE == MAX_OPT_LEN now, but just in case... */
	(void)strlcpy(login_name, pn, sizeof(login_name));

	do_args(argc, argv);
	do_opts();

	if (!score_only && !rest_file) {
		printf("Hello %s, just a moment while I dig the dungeon...",
			nick_name);
		fflush(stdout);
	}

	if (!initscr()) {
		fprintf(stderr, "couldn't initialize screen\n");
		exit (0);
	}
	if ((LINES < DROWS) || (COLS < DCOLS)) {
		clean_up("must be played on at least 80 x 24 screen");
	}
	start_window();
	init_curses = 1;

	md_heed_signals();

	if (score_only) {
		put_scores(NULL, 0);
	}
	seed = md_gseed();
	(void)srrandom(seed);
	if (rest_file) {
		restore(rest_file);
		return(1);
	}
	mix_colors();
	get_wand_and_ring_materials();
	make_scroll_titles();

	level_objects.next_object = NULL;
	level_monsters.next_monster = NULL;
	player_init();
	ring_stats(0);
	return(0);
}

static void
player_init(void)
{
	object *obj;

	rogue.pack.next_object = NULL;

	obj = alloc_object();
	get_food(obj, 1);
	(void)add_to_pack(obj, &rogue.pack, 1);

	obj = alloc_object();		/* initial armor */
	obj->what_is = ARMOR;
	obj->which_kind = RINGMAIL;
	obj->class = RINGMAIL+2;
	obj->is_protected = 0;
	obj->d_enchant = 1;
	(void)add_to_pack(obj, &rogue.pack, 1);
	do_wear(obj);

	obj = alloc_object();		/* initial weapons */
	obj->what_is = WEAPON;
	obj->which_kind = MACE;
	obj->damage = "2d3";
	obj->hit_enchant = obj->d_enchant = 1;
	obj->identified = 1;
	(void)add_to_pack(obj, &rogue.pack, 1);
	do_wield(obj);

	obj = alloc_object();
	obj->what_is = WEAPON;
	obj->which_kind = BOW;
	obj->damage = "1d2";
	obj->hit_enchant = 1;
	obj->d_enchant = 0;
	obj->identified = 1;
	(void)add_to_pack(obj, &rogue.pack, 1);

	obj = alloc_object();
	obj->what_is = WEAPON;
	obj->which_kind = ARROW;
	obj->quantity = get_rand(25, 35);
	obj->damage = "1d2";
	obj->hit_enchant = 0;
	obj->d_enchant = 0;
	obj->identified = 1;
	(void)add_to_pack(obj, &rogue.pack, 1);
}

void
clean_up(const char *estr)
{
	if (save_is_interactive) {
		if (init_curses) {
			move(DROWS-1, 0);
			refresh();
			stop_window();
		}
		printf("\n%s\n", estr);
	}
	md_exit(0);
}

void
start_window(void)
{
	cbreak();
	noecho();
#ifndef BAD_NONL
	nonl();
#endif
}

void
stop_window(void)
{
	endwin();
}

void
byebye(int dummy __unused)
{
	md_ignore_signals();
	if (ask_quit) {
		quit(1);
	} else {
		clean_up(byebye_string);
	}
	md_heed_signals();
}

void
onintr(int dummy __unused)
{
	md_ignore_signals();
	if (cant_int) {
		did_int = 1;
	} else {
		check_message();
		messagef(1, "interrupt");
	}
	md_heed_signals();
}

void
error_save(int dummy __unused)
{
	save_is_interactive = 0;
	save_into_file(error_file);
	clean_up("");
}

static void
do_args(int argc, char *argv[])
{
	int i, j;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (j = 1; argv[i][j]; j++) {
				switch(argv[i][j]) {
				case 's':
					score_only = 1;
					break;
				}
			}
		} else {
			rest_file = argv[i];
		}
	}
}

static void
do_opts(void)
{
	char *eptr;

	if ((eptr = md_getenv("ROGUEOPTS")) != NULL) {
		for (;;) {
			while ((*eptr) == ' ') {
				eptr++;
			}
			if (!(*eptr)) {
				break;
			}
			if (!strncmp(eptr, "fruit=", 6)) {
				eptr += 6;
				env_get_value(&fruit, eptr, 1);
			} else if (!strncmp(eptr, "file=", 5)) {
				eptr += 5;
				env_get_value(&save_file, eptr, 0);
			} else if (!strncmp(eptr, "jump", 4)) {
				jump = 1;
			} else if (!strncmp(eptr, "name=", 5)) {
				eptr += 5;
				env_get_value(&nick_name, eptr, 0);
			} else if (!strncmp(eptr, "noaskquit", 9)) {
				ask_quit = 0;
			} else if (!strncmp(eptr, "noskull", 5) ||
					!strncmp(eptr,"notomb", 6)) {
				no_skull = 1;
			} else if (!strncmp(eptr, "passgo", 5)) {
				passgo = 1;
			}
			while ((*eptr) && (*eptr != ',')) {
				eptr++;
			}
			if (!(*(eptr++))) {
				break;
			}
		}
	}
	/* If some strings have not been set through ROGUEOPTS, assign defaults
	 * to them so that the options editor has data to work with.
	 */
	init_str(&nick_name, login_name);
	init_str(&save_file, "rogue.save");
	init_str(&fruit, "slime-mold");
}

static void
env_get_value(char **s, char *e, boolean add_blank)
{
	short i = 0;
	const char *t;

	t = e;

	while ((*e) && (*e != ',')) {
		if (*e == ':') {
			*e = ';';		/* ':' reserved for score file purposes */
		}
		e++;
		if (++i >= MAX_OPT_LEN) {
			break;
		}
	}
	/* note: edit_opts() in room.c depends on this being the right size */
	*s = md_malloc(MAX_OPT_LEN + 2);
	if (*s == NULL)
		clean_up("out of memory");
	(void)strncpy(*s, t, i);
	if (add_blank) {
		(*s)[i++] = ' ';
	}
	(*s)[i] = '\0';
}

static void
init_str(char **str, const char *dflt)
{
	if (!(*str)) {
		/* note: edit_opts() in room.c depends on this size */
		*str = md_malloc(MAX_OPT_LEN + 2);
		if (*str == NULL)
			clean_up("out of memory");
		(void)strlcpy(*str, dflt, MAX_OPT_LEN + 2);
	}
}
