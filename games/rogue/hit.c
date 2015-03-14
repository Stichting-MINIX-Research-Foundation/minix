/*	$NetBSD: hit.c,v 1.10 2008/01/14 03:50:01 dholland Exp $	*/

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
static char sccsid[] = "@(#)hit.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: hit.c,v 1.10 2008/01/14 03:50:01 dholland Exp $");
#endif
#endif /* not lint */

/*
 * hit.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include "rogue.h"

static int damage_for_strength(void);
static int get_w_damage(const object *);
static int to_hit(const object *);

static object *fight_monster = NULL;
char hit_message[HIT_MESSAGE_SIZE] = "";

void
mon_hit(object *monster)
{
	short damage, hit_chance;
	const char *mn;
	float minus;

	if (fight_monster && (monster != fight_monster)) {
		fight_monster = 0;
	}
	monster->trow = NO_ROOM;
	if (cur_level >= (AMULET_LEVEL * 2)) {
		hit_chance = 100;
	} else {
		hit_chance = monster->m_hit_chance;
		hit_chance -= (((2 * rogue.exp) + (2 * ring_exp)) - r_rings);
	}
	if (wizard) {
		hit_chance /= 2;
	}
	if (!fight_monster) {
		interrupted = 1;
	}
	mn = mon_name(monster);

	if (!rand_percent(hit_chance)) {
		if (!fight_monster) {
			messagef(1, "%sthe %s misses", hit_message, mn);
			hit_message[0] = 0;
		}
		return;
	}
	if (!fight_monster) {
		messagef(1, "%sthe %s hit", hit_message, mn);
		hit_message[0] = 0;
	}
	if (!(monster->m_flags & STATIONARY)) {
		damage = get_damage(monster->m_damage, 1);
		if (cur_level >= (AMULET_LEVEL * 2)) {
			minus = (float)((AMULET_LEVEL * 2) - cur_level);
		} else {
			minus = (float)get_armor_class(rogue.armor) * 3.00;
			minus = minus/100.00 * (float)damage;
		}
		damage -= (short)minus;
	} else {
		damage = monster->stationary_damage++;
	}
	if (wizard) {
		damage /= 3;
	}
	if (damage > 0) {
		rogue_damage(damage, monster, 0);
	}
	if (monster->m_flags & SPECIAL_HIT) {
		special_hit(monster);
	}
}

void
rogue_hit(object *monster, boolean force_hit)
{
	short damage, hit_chance;

	if (monster) {
		if (check_imitator(monster)) {
			return;
		}
		hit_chance = force_hit ? 100 : get_hit_chance(rogue.weapon);

		if (wizard) {
			hit_chance *= 2;
		}
		if (!rand_percent(hit_chance)) {
			if (!fight_monster) {
				(void)strlcpy(hit_message, "you miss  ",
					       sizeof(hit_message));
			}
			goto RET;
		}
		damage = get_weapon_damage(rogue.weapon);
		if (wizard) {
			damage *= 3;
		}
		if (con_mon) {
			s_con_mon(monster);
		}
		if (mon_damage(monster, damage)) {	/* still alive? */
			if (!fight_monster) {
				(void)strlcpy(hit_message, "you hit  ",
					       sizeof(hit_message));
			}
		}
RET:	check_gold_seeker(monster);
		wake_up(monster);
	}
}

void
rogue_damage(short d, object *monster, short other)
{
	if (d >= rogue.hp_current) {
		rogue.hp_current = 0;
		print_stats(STAT_HP);
		killed_by(monster, other);
	}
	if (d > 0) {
		rogue.hp_current -= d;
		print_stats(STAT_HP);
	}
}

int
get_damage(const char *ds, boolean r)
{
	int i = 0, j, n, d, total = 0;

	while (ds[i]) {
		n = get_number(ds+i);
		while ((ds[i] != 'd') && ds[i]) {
			i++;
		}
		if (ds[i] == 'd') {
			i++;
		}

		d = get_number(ds+i);
		while ((ds[i] != '/') && ds[i]) {
			i++;
		}
		if (ds[i] == '/') {
			i++;
		}

		for (j = 0; j < n; j++) {
			if (r) {
				total += get_rand(1, d);
			} else {
				total += d;
			}
		}
	}
	return(total);
}

static int
get_w_damage(const object *obj)
{
	char new_damage[32];
	int tmp_to_hit, tmp_damage;
	int i = 0;

	if ((!obj) || (obj->what_is != WEAPON)) {
		return(-1);
	}
	tmp_to_hit = get_number(obj->damage) + obj->hit_enchant;
	while ((obj->damage[i] != 'd') && obj->damage[i]) {
		i++;
	}
	if (obj->damage[i] == 'd') {
		i++;
	}
	tmp_damage = get_number(obj->damage + i) + obj->d_enchant;

	snprintf(new_damage, sizeof(new_damage), "%dd%d",
		tmp_to_hit, tmp_damage);

	return(get_damage(new_damage, 1));
}

int
get_number(const char *s)
{
	int i = 0;
	int total = 0;

	while ((s[i] >= '0') && (s[i] <= '9')) {
		total = (10 * total) + (s[i] - '0');
		i++;
	}
	return(total);
}

long
lget_number(const char *s)
{
	short i = 0;
	long total = 0;

	while ((s[i] >= '0') && (s[i] <= '9')) {
		total = (10 * total) + (s[i] - '0');
		i++;
	}
	return(total);
}

static int
to_hit(const object *obj)
{
	if (!obj) {
		return(1);
	}
	return(get_number(obj->damage) + obj->hit_enchant);
}

static int
damage_for_strength(void)
{
	short strength;

	strength = rogue.str_current + add_strength;

	if (strength <= 6) {
		return(strength-5);
	}
	if (strength <= 14) {
		return(1);
	}
	if (strength <= 17) {
		return(3);
	}
	if (strength <= 18) {
		return(4);
	}
	if (strength <= 20) {
		return(5);
	}
	if (strength <= 21) {
		return(6);
	}
	if (strength <= 30) {
		return(7);
	}
	return(8);
}

int
mon_damage(object *monster, short damage)
{
	const char *mn;
	short row, col;

	monster->hp_to_kill -= damage;

	if (monster->hp_to_kill <= 0) {
		row = monster->row;
		col = monster->col;
		dungeon[row][col] &= ~MONSTER;
		mvaddch(row, col, get_dungeon_char(row, col));

		fight_monster = 0;
		cough_up(monster);
		mn = mon_name(monster);
		messagef(1, "%sdefeated the %s", hit_message, mn);
		hit_message[0] = 0;
		add_exp(monster->kill_exp, 1);
		take_from_pack(monster, &level_monsters);

		if (monster->m_flags & HOLDS) {
			being_held = 0;
		}
		free_object(monster);
		return(0);
	}
	return(1);
}

void
fight(boolean to_the_death)
{
	short ch, c, d;
	short row, col;
	boolean first_miss = 1;
	short possible_damage;
	object *monster;

	ch = 0;
	while (!is_direction(ch = rgetchar(), &d)) {
		sound_bell();
		if (first_miss) {
			messagef(0, "direction?");
			first_miss = 0;
		}
	}
	check_message();
	if (ch == CANCEL) {
		return;
	}
	row = rogue.row; col = rogue.col;
	get_dir_rc(d, &row, &col, 0);

	c = mvinch(row, col);
	if (((c < 'A') || (c > 'Z')) ||
		(!can_move(rogue.row, rogue.col, row, col))) {
		messagef(0, "I see no monster there");
		return;
	}
	if (!(fight_monster = object_at(&level_monsters, row, col))) {
		return;
	}
	if (!(fight_monster->m_flags & STATIONARY)) {
		possible_damage = ((get_damage(fight_monster->m_damage, 0) * 2) / 3);
	} else {
		possible_damage = fight_monster->stationary_damage - 1;
	}
	while (fight_monster) {
		(void)one_move_rogue(ch, 0);
		if (((!to_the_death) && (rogue.hp_current <= possible_damage)) ||
			interrupted || (!(dungeon[row][col] & MONSTER))) {
			fight_monster = 0;
		} else {
			monster = object_at(&level_monsters, row, col);
			if (monster != fight_monster) {
				fight_monster = 0;
			}
		}
	}
}

void
get_dir_rc(short dir, short *row, short *col, short allow_off_screen)
{
	switch(dir) {
	case LEFT:
		if (allow_off_screen || (*col > 0)) {
			(*col)--;
		}
		break;
	case DOWN:
		if (allow_off_screen || (*row < (DROWS-2))) {
			(*row)++;
		}
		break;
	case UPWARD:
		if (allow_off_screen || (*row > MIN_ROW)) {
			(*row)--;
		}
		break;
	case RIGHT:
		if (allow_off_screen || (*col < (DCOLS-1))) {
			(*col)++;
		}
		break;
	case UPLEFT:
		if (allow_off_screen || ((*row > MIN_ROW) && (*col > 0))) {
			(*row)--;
			(*col)--;
		}
		break;
	case UPRIGHT:
		if (allow_off_screen || ((*row > MIN_ROW) && (*col < (DCOLS-1)))) {
			(*row)--;
			(*col)++;
		}
		break;
	case DOWNRIGHT:
		if (allow_off_screen || ((*row < (DROWS-2)) && (*col < (DCOLS-1)))) {
			(*row)++;
			(*col)++;
		}
		break;
	case DOWNLEFT:
		if (allow_off_screen || ((*row < (DROWS-2)) && (*col > 0))) {
			(*row)++;
			(*col)--;
		}
		break;
	}
}

int
get_hit_chance(const object *weapon)
{
	short hit_chance;

	hit_chance = 40;
	hit_chance += 3 * to_hit(weapon);
	hit_chance += (((2 * rogue.exp) + (2 * ring_exp)) - r_rings);
	return(hit_chance);
}

int
get_weapon_damage(const object *weapon)
{
	short damage;

	damage = get_w_damage(weapon);
	damage += damage_for_strength();
	damage += ((((rogue.exp + ring_exp) - r_rings) + 1) / 2);
	return(damage);
}

void
s_con_mon(object *monster)
{
	if (con_mon) {
		monster->m_flags |= CONFUSED;
		monster->moves_confused += get_rand(12, 22);
		messagef(0, "the monster appears confused");
		con_mon = 0;
	}
}
