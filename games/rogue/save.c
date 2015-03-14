/*	$NetBSD: save.c,v 1.13 2008/01/14 03:50:02 dholland Exp $	*/

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
static char sccsid[] = "@(#)save.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: save.c,v 1.13 2008/01/14 03:50:02 dholland Exp $");
#endif
#endif /* not lint */

/*
 * save.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include <stdio.h>
#include "rogue.h"

static boolean	has_been_touched(const struct rogue_time *, 
			const struct rogue_time *);
static void	r_read(FILE *, void *, size_t);
static void	r_write(FILE *, const void *, size_t);
static void	read_pack(object *, FILE *, boolean);
static void	read_string(char *, FILE *, size_t);
static void	rw_dungeon(FILE *, boolean);
static void	rw_id(struct id *, FILE *, int, boolean);
static void	rw_rooms(FILE *, boolean);
static void	write_pack(const object *, FILE *);
static void	write_string(char *, FILE *);

static short write_failed = 0;

char *save_file = NULL;

void
save_game(void)
{
	char fname[64];

	if (!get_input_line("file name?", save_file, fname, sizeof(fname),
			"game not saved", 0, 1)) {
		return;
	}
	check_message();
	messagef(0, "%s", fname);
	save_into_file(fname);
}

void
save_into_file(const char *sfile)
{
	FILE *fp;
	int file_id;
	char *name_buffer;
	size_t len;
	char *hptr;
	struct rogue_time rt_buf;

	if (sfile[0] == '~') {
		if ((hptr = md_getenv("HOME")) != NULL) {
			len = strlen(hptr) + strlen(sfile);
			name_buffer = md_malloc(len);
			if (name_buffer == NULL) {
				messagef(0,
					"out of memory for save file name");
				sfile = error_file;
			} else {
				(void)strcpy(name_buffer, hptr);
				(void)strcat(name_buffer, sfile+1);
				sfile = name_buffer;
			}
			/*
			 * Note: name_buffer gets leaked. But it's small,
			 * and in the common case we're about to exit.
			 */
		}
	}
	if (((fp = fopen(sfile, "w")) == NULL) ||
	    ((file_id = md_get_file_id(sfile)) == -1)) {
		if (fp)
			fclose(fp);
		messagef(0, "problem accessing the save file");
		return;
	}
	md_ignore_signals();
	write_failed = 0;
	(void)xxx(1);
	r_write(fp, &detect_monster, sizeof(detect_monster));
	r_write(fp, &cur_level, sizeof(cur_level));
	r_write(fp, &max_level, sizeof(max_level));
	write_string(hunger_str, fp);
	write_string(login_name, fp);
	r_write(fp, &party_room, sizeof(party_room));
	write_pack(&level_monsters, fp);
	write_pack(&level_objects, fp);
	r_write(fp, &file_id, sizeof(file_id));
	rw_dungeon(fp, 1);
	r_write(fp, &foods, sizeof(foods));
	r_write(fp, &rogue, sizeof(fighter));
	write_pack(&rogue.pack, fp);
	rw_id(id_potions, fp, POTIONS, 1);
	rw_id(id_scrolls, fp, SCROLS, 1);
	rw_id(id_wands, fp, WANDS, 1);
	rw_id(id_rings, fp, RINGS, 1);
	r_write(fp, traps, (MAX_TRAPS * sizeof(trap)));
	r_write(fp, is_wood, (WANDS * sizeof(boolean)));
	r_write(fp, &cur_room, sizeof(cur_room));
	rw_rooms(fp, 1);
	r_write(fp, &being_held, sizeof(being_held));
	r_write(fp, &bear_trap, sizeof(bear_trap));
	r_write(fp, &halluc, sizeof(halluc));
	r_write(fp, &blind, sizeof(blind));
	r_write(fp, &confused, sizeof(confused));
	r_write(fp, &levitate, sizeof(levitate));
	r_write(fp, &haste_self, sizeof(haste_self));
	r_write(fp, &see_invisible, sizeof(see_invisible));
	r_write(fp, &detect_monster, sizeof(detect_monster));
	r_write(fp, &wizard, sizeof(wizard));
	r_write(fp, &score_only, sizeof(score_only));
	r_write(fp, &m_moves, sizeof(m_moves));
	md_gct(&rt_buf);
	rt_buf.second += 10;		/* allow for some processing time */
	r_write(fp, &rt_buf, sizeof(rt_buf));
	fclose(fp);

	if (write_failed) {
		(void)md_df(sfile);	/* delete file */
	} else {
		clean_up("");
	}
}

void
restore(const char *fname)
{
	FILE *fp;
	struct rogue_time saved_time, mod_time;
	char buf[4];
	char tbuf[MAX_OPT_LEN];
	int new_file_id, saved_file_id;

	fp = NULL;
	if (((new_file_id = md_get_file_id(fname)) == -1) ||
	    ((fp = fopen(fname, "r")) == NULL)) {
		clean_up("cannot open file");
	}
	if (md_link_count(fname) > 1) {
		clean_up("file has link");
	}
	(void)xxx(1);
	r_read(fp, &detect_monster, sizeof(detect_monster));
	r_read(fp, &cur_level, sizeof(cur_level));
	r_read(fp, &max_level, sizeof(max_level));
	read_string(hunger_str, fp, sizeof hunger_str);

	(void)strlcpy(tbuf, login_name, sizeof tbuf);
	read_string(login_name, fp, sizeof login_name);
	if (strcmp(tbuf, login_name)) {
		clean_up("you're not the original player");
	}

	r_read(fp, &party_room, sizeof(party_room));
	read_pack(&level_monsters, fp, 0);
	read_pack(&level_objects, fp, 0);
	r_read(fp, &saved_file_id, sizeof(saved_file_id));
	if (new_file_id != saved_file_id) {
		clean_up("sorry, saved game is not in the same file");
	}
	rw_dungeon(fp, 0);
	r_read(fp, &foods, sizeof(foods));
	r_read(fp, &rogue, sizeof(fighter));
	read_pack(&rogue.pack, fp, 1);
	rw_id(id_potions, fp, POTIONS, 0);
	rw_id(id_scrolls, fp, SCROLS, 0);
	rw_id(id_wands, fp, WANDS, 0);
	rw_id(id_rings, fp, RINGS, 0);
	r_read(fp, traps, (MAX_TRAPS * sizeof(trap)));
	r_read(fp, is_wood, (WANDS * sizeof(boolean)));
	r_read(fp, &cur_room, sizeof(cur_room));
	rw_rooms(fp, 0);
	r_read(fp, &being_held, sizeof(being_held));
	r_read(fp, &bear_trap, sizeof(bear_trap));
	r_read(fp, &halluc, sizeof(halluc));
	r_read(fp, &blind, sizeof(blind));
	r_read(fp, &confused, sizeof(confused));
	r_read(fp, &levitate, sizeof(levitate));
	r_read(fp, &haste_self, sizeof(haste_self));
	r_read(fp, &see_invisible, sizeof(see_invisible));
	r_read(fp, &detect_monster, sizeof(detect_monster));
	r_read(fp, &wizard, sizeof(wizard));
	r_read(fp, &score_only, sizeof(score_only));
	r_read(fp, &m_moves, sizeof(m_moves));
	r_read(fp, &saved_time, sizeof(saved_time));

	if (fread(buf, 1, 1, fp) > 0) {
		clear();
		clean_up("extra characters in file");
	}

	md_gfmt(fname, &mod_time);	/* get file modification time */

	if (has_been_touched(&saved_time, &mod_time)) {
		clear();
		clean_up("sorry, file has been touched");
	}
	if ((!wizard) && !md_df(fname)) {
		clean_up("cannot delete file");
	}
	msg_cleared = 0;
	ring_stats(0);
	fclose(fp);
}

static void
write_pack(const object *pack, FILE *fp)
{
	object t;

	while ((pack = pack->next_object) != NULL) {
		r_write(fp, pack, sizeof(object));
	}
	t.ichar = t.what_is = 0;
	r_write(fp, &t, sizeof(object));
}

static void
read_pack(object *pack, FILE *fp, boolean is_rogue)
{
	object read_obj, *new_obj;

	for (;;) {
		r_read(fp, &read_obj, sizeof(object));
		if (read_obj.ichar == 0) {
			pack->next_object = NULL;
			break;
		}
		new_obj = alloc_object();
		*new_obj = read_obj;
		if (is_rogue) {
			if (new_obj->in_use_flags & BEING_WORN) {
				do_wear(new_obj);
			} else if (new_obj->in_use_flags & BEING_WIELDED) {
				do_wield(new_obj);
			} else if (new_obj->in_use_flags & (ON_EITHER_HAND)) {
				do_put_on(new_obj,
					((new_obj->in_use_flags & ON_LEFT_HAND) ? 1 : 0));
			}
		}
		pack->next_object = new_obj;
		pack = new_obj;
	}
}

static void
rw_dungeon(FILE *fp, boolean rw)
{
	short i, j;
	char buf[DCOLS];

	for (i = 0; i < DROWS; i++) {
		if (rw) {
			r_write(fp, dungeon[i], (DCOLS * sizeof(dungeon[0][0])));
			for (j = 0; j < DCOLS; j++) {
				buf[j] = mvinch(i, j);
			}
			r_write(fp, buf, DCOLS);
		} else {
			r_read(fp, dungeon[i], (DCOLS * sizeof(dungeon[0][0])));
			r_read(fp, buf, DCOLS);
			for (j = 0; j < DCOLS; j++) {
				mvaddch(i, j, buf[j]);
			}
		}
	}
}

static void
rw_id(struct id id_table[], FILE *fp, int n, boolean wr)
{
	int i;

	for (i = 0; i < n; i++) {
		if (wr) {
			r_write(fp, &id_table[i].value, sizeof(short));
			r_write(fp, &id_table[i].id_status,
				sizeof(unsigned short));
			write_string(id_table[i].title, fp);
		} else {
			r_read(fp, &id_table[i].value, sizeof(short));
			r_read(fp, &id_table[i].id_status,
				sizeof(unsigned short));
			read_string(id_table[i].title, fp, MAX_ID_TITLE_LEN);
		}
	}
}

static void
write_string(char *s, FILE *fp)
{
	short n;

	n = strlen(s) + 1;
	xxxx(s, n);
	r_write(fp, &n, sizeof(short));
	r_write(fp, s, n);
}

static void
read_string(char *s, FILE *fp, size_t len)
{
	short n;

	r_read(fp, &n, sizeof(short));
	if (n<=0 || (size_t)(unsigned short)n > len) {
		clean_up("read_string: corrupt game file");
	}
	r_read(fp, s, n);
	xxxx(s, n);
	/* ensure null termination */
	s[n-1] = 0;
}

static void
rw_rooms(FILE *fp, boolean rw)
{
	short i;

	for (i = 0; i < MAXROOMS; i++) {
		rw ? r_write(fp, (rooms + i), sizeof(room)) :
			r_read(fp, (rooms + i), sizeof(room));
	}
}

static void
r_read(FILE *fp, void *buf, size_t n)
{
	if (fread(buf, 1, n, fp) != n) {
		clean_up("fread() failed, don't know why");
	}
}

static void
r_write(FILE *fp, const void *buf, size_t n)
{
	if (!write_failed) {
		if (fwrite(buf, 1, n, fp) != n) {
			messagef(0, "write() failed, don't know why");
			sound_bell();
			write_failed = 1;
		}
	}
}

static boolean
has_been_touched(const struct rogue_time *saved_time,
		 const struct rogue_time *mod_time)
{
	if (saved_time->year < mod_time->year) {
		return(1);
	} else if (saved_time->year > mod_time->year) {
		return(0);
	}
	if (saved_time->month < mod_time->month) {
		return(1);
	} else if (saved_time->month > mod_time->month) {
		return(0);
	}
	if (saved_time->day < mod_time->day) {
		return(1);
	} else if (saved_time->day > mod_time->day) {
		return(0);
	}
	if (saved_time->hour < mod_time->hour) {
		return(1);
	} else if (saved_time->hour > mod_time->hour) {
		return(0);
	}
	if (saved_time->minute < mod_time->minute) {
		return(1);
	} else if (saved_time->minute > mod_time->minute) {
		return(0);
	}
	if (saved_time->second < mod_time->second) {
		return(1);
	}
	return(0);
}
