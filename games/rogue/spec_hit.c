/*	$NetBSD: spec_hit.c,v 1.9 2011/05/23 23:01:17 joerg Exp $	*/

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
static char sccsid[] = "@(#)spec_hit.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: spec_hit.c,v 1.9 2011/05/23 23:01:17 joerg Exp $");
#endif
#endif /* not lint */

/*
 * special_hit.c
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

static void	disappear(object *);
static void	drain_life(void);
static void	drop_level(void);
static void	freeze(object *);
static int	get_dir(short, short, short, short);
static boolean	gold_at(short, short);
static void	steal_gold(object *);
static void	steal_item(object *);
static void	sting(object *);
static boolean	try_to_cough(short, short, object *);

short less_hp = 0;
boolean being_held;

void
special_hit(object *monster)
{
	if ((monster->m_flags & CONFUSED) && rand_percent(66)) {
		return;
	}
	if (monster->m_flags & RUSTS) {
		rust(monster);
	}
	if ((monster->m_flags & HOLDS) && !levitate) {
		being_held = 1;
	}
	if (monster->m_flags & FREEZES) {
		freeze(monster);
	}
	if (monster->m_flags & STINGS) {
		sting(monster);
	}
	if (monster->m_flags & DRAINS_LIFE) {
		drain_life();
	}
	if (monster->m_flags & DROPS_LEVEL) {
		drop_level();
	}
	if (monster->m_flags & STEALS_GOLD) {
		steal_gold(monster);
	} else if (monster->m_flags & STEALS_ITEM) {
		steal_item(monster);
	}
}

void
rust(object *monster)
{
	if ((!rogue.armor) || (get_armor_class(rogue.armor) <= 1) ||
		(rogue.armor->which_kind == LEATHER)) {
		return;
	}
	if ((rogue.armor->is_protected) || maintain_armor) {
		if (monster && (!(monster->m_flags & RUST_VANISHED))) {
			messagef(0, "the rust vanishes instantly");
			monster->m_flags |= RUST_VANISHED;
		}
	} else {
		rogue.armor->d_enchant--;
		messagef(0, "your armor weakens");
		print_stats(STAT_ARMOR);
	}
}

void
freeze(object *monster)
{
	short freeze_percent = 99;
	short i, n;

	if (rand_percent(12)) {
		return;
	}
	freeze_percent -= (rogue.str_current+(rogue.str_current / 2));
	freeze_percent -= ((rogue.exp + ring_exp) * 4);
	freeze_percent -= (get_armor_class(rogue.armor) * 5);
	freeze_percent -= (rogue.hp_max / 3);

	if (freeze_percent > 10) {
		monster->m_flags |= FREEZING_ROGUE;
		messagef(1, "you are frozen");

		n = get_rand(4, 8);
		for (i = 0; i < n; i++) {
			mv_mons();
		}
		if (rand_percent(freeze_percent)) {
			for (i = 0; i < 50; i++) {
				mv_mons();
			}
			killed_by(NULL, HYPOTHERMIA);
		}
		messagef(1, "%s", you_can_move_again);
		monster->m_flags &= (~FREEZING_ROGUE);
	}
}

void
steal_gold(object *monster)
{
	int amount;

	if ((rogue.gold <= 0) || rand_percent(10)) {
		return;
	}

	amount = get_rand((cur_level * 10), (cur_level * 30));

	if (amount > rogue.gold) {
		amount = rogue.gold;
	}
	rogue.gold -= amount;
	messagef(0, "your purse feels lighter");
	print_stats(STAT_GOLD);
	disappear(monster);
}

void
steal_item(object *monster)
{
	object *obj;
	short i, n, t = 0;
	char desc[80];
	boolean has_something = 0;

	if (rand_percent(15)) {
		return;
	}
	obj = rogue.pack.next_object;

	if (!obj) {
		goto DSPR;
	}
	while (obj) {
		if (!(obj->in_use_flags & BEING_USED)) {
			has_something = 1;
			break;
		}
		obj = obj->next_object;
	}
	if (!has_something) {
		goto DSPR;
	}
	n = get_rand(0, MAX_PACK_COUNT);
	obj = rogue.pack.next_object;

	for (i = 0; i <= n; i++) {
		obj = obj->next_object;
		while ((!obj) || (obj->in_use_flags & BEING_USED)) {
			if (!obj) {
				obj = rogue.pack.next_object;
			} else {
				obj = obj->next_object;
			}
		}
	}
	if (obj->what_is != WEAPON) {
		t = obj->quantity;
		obj->quantity = 1;
	}
	get_desc(obj, desc, sizeof(desc));
	messagef(0, "she stole %s", desc);

	obj->quantity = ((obj->what_is != WEAPON) ? t : 1);

	vanish(obj, 0, &rogue.pack);
DSPR:
	disappear(monster);
}

static void
disappear(object *monster)
{
	short row, col;

	row = monster->row;
	col = monster->col;

	dungeon[row][col] &= ~MONSTER;
	if (rogue_can_see(row, col)) {
		mvaddch(row, col, get_dungeon_char(row, col));
	}
	take_from_pack(monster, &level_monsters);
	free_object(monster);
	mon_disappeared = 1;
}

void
cough_up(object *monster)
{
	object *obj;
	short row, col, i, n;

	if (cur_level < max_level) {
		return;
	}

	if (monster->m_flags & STEALS_GOLD) {
		obj = alloc_object();
		obj->what_is = GOLD;
		obj->quantity = get_rand((cur_level * 15), (cur_level * 30));
	} else {
		if (!rand_percent((int)monster->drop_percent)) {
			return;
		}
		obj = gr_object();
	}
	row = monster->row;
	col = monster->col;

	for (n = 0; n <= 5; n++) {
		for (i = -n; i <= n; i++) {
			if (try_to_cough(row+n, col+i, obj)) {
				return;
			}
			if (try_to_cough(row-n, col+i, obj)) {
				return;
			}
		}
		for (i = -n; i <= n; i++) {
			if (try_to_cough(row+i, col-n, obj)) {
				return;
			}
			if (try_to_cough(row+i, col+n, obj)) {
				return;
			}
		}
	}
	free_object(obj);
}

static boolean
try_to_cough(short row, short col, object *obj)
{
	if ((row < MIN_ROW) ||
	    (row > (DROWS-2)) || (col < 0) || (col>(DCOLS-1))) {
		return(0);
	}
	if ((!(dungeon[row][col] & (OBJECT | STAIRS | TRAP))) &&
		(dungeon[row][col] & (TUNNEL | FLOOR | DOOR))) {
		place_at(obj, row, col);
		if (((row != rogue.row) || (col != rogue.col)) &&
			(!(dungeon[row][col] & MONSTER))) {
			mvaddch(row, col, get_dungeon_char(row, col));
		}
		return(1);
	}
	return(0);
}

boolean
seek_gold(object *monster)
{
	short i, j, rn, s;

	if ((rn = get_room_number(monster->row, monster->col)) < 0) {
		return(0);
	}
	for (i = rooms[rn].top_row+1; i < rooms[rn].bottom_row; i++) {
		for (j = rooms[rn].left_col+1; j < rooms[rn].right_col; j++) {
			if ((gold_at(i, j)) && !(dungeon[i][j] & MONSTER)) {
				monster->m_flags |= CAN_FLIT;
				s = mon_can_go(monster, i, j);
				monster->m_flags &= (~CAN_FLIT);
				if (s) {
					move_mon_to(monster, i, j);
					monster->m_flags |= ASLEEP;
					monster->m_flags &= (~(WAKENS | SEEKS_GOLD));
					return(1);
				}
				monster->m_flags &= (~SEEKS_GOLD);
				monster->m_flags |= CAN_FLIT;
				mv_1_monster(monster, i, j);
				monster->m_flags &= (~CAN_FLIT);
				monster->m_flags |= SEEKS_GOLD;
				return(1);
			}
		}
	}
	return(0);
}

static boolean
gold_at(short row, short col)
{
	if (dungeon[row][col] & OBJECT) {
		object *obj;

		if ((obj = object_at(&level_objects, row, col)) &&
				(obj->what_is == GOLD)) {
			return(1);
		}
	}
	return(0);
}

void
check_gold_seeker(object *monster)
{
	monster->m_flags &= (~SEEKS_GOLD);
}

boolean
check_imitator(object *monster)
{
	if (monster->m_flags & IMITATES) {
		wake_up(monster);
		if (!blind) {
			mvaddch(monster->row, monster->col,
					get_dungeon_char(monster->row, monster->col));
			check_message();
			messagef(1, "wait, that's a %s!", mon_name(monster));
		}
		return(1);
	}
	return(0);
}

boolean
imitating(short row, short col)
{
	if (dungeon[row][col] & MONSTER) {
		object *monster;

		if ((monster = object_at(&level_monsters, row, col)) != NULL) {
			if (monster->m_flags & IMITATES) {
				return(1);
			}
		}
	}
	return(0);
}

static void
sting(object *monster)
{
	short sting_chance = 35;

	if ((rogue.str_current <= 3) || sustain_strength) {
		return;
	}
	sting_chance += (6 * (6 - get_armor_class(rogue.armor)));

	if ((rogue.exp + ring_exp) > 8) {
		sting_chance -= (6 * ((rogue.exp + ring_exp) - 8));
	}
	if (rand_percent(sting_chance)) {
		messagef(0, "the %s's bite has weakened you",
			mon_name(monster));
		rogue.str_current--;
		print_stats(STAT_STRENGTH);
	}
}

static void
drop_level(void)
{
	int hp;

	if (rand_percent(80) || (rogue.exp <= 5)) {
		return;
	}
	rogue.exp_points = level_points[rogue.exp-2] - get_rand(9, 29);
	rogue.exp -= 2;
	hp = hp_raise();
	if ((rogue.hp_current -= hp) <= 0) {
		rogue.hp_current = 1;
	}
	if ((rogue.hp_max -= hp) <= 0) {
		rogue.hp_max = 1;
	}
	add_exp(1, 0);
}

void
drain_life(void)
{
	short n;

	if (rand_percent(60) || (rogue.hp_max <= 30) || (rogue.hp_current < 10)) {
		return;
	}
	n = get_rand(1, 3);		/* 1 Hp, 2 Str, 3 both */

	if ((n != 2) || (!sustain_strength)) {
		messagef(0, "you feel weaker");
	}
	if (n != 2) {
		rogue.hp_max--;
		rogue.hp_current--;
		less_hp++;
	}
	if (n != 1) {
		if ((rogue.str_current > 3) && (!sustain_strength)) {
			rogue.str_current--;
			if (coin_toss()) {
				rogue.str_max--;
			}
		}
	}
	print_stats((STAT_STRENGTH | STAT_HP));
}

boolean
m_confuse(object *monster)
{
	if (!rogue_can_see(monster->row, monster->col)) {
		return(0);
	}
	if (rand_percent(45)) {
		monster->m_flags &= (~CONFUSES);	/* will not confuse the rogue */
		return(0);
	}
	if (rand_percent(55)) {
		monster->m_flags &= (~CONFUSES);
		messagef(1, "the gaze of the %s has confused you",
			mon_name(monster));
		cnfs();
		return(1);
	}
	return(0);
}

boolean
flame_broil(object *monster)
{
	short row, col, dir;

	if ((!mon_sees(monster, rogue.row, rogue.col)) || coin_toss()) {
		return(0);
	}
	row = rogue.row - monster->row;
	col = rogue.col - monster->col;
	if (row < 0) {
		row = -row;
	}
	if (col < 0) {
		col = -col;
	}
	if (((row != 0) && (col != 0) && (row != col)) ||
		((row > 7) || (col > 7))) {
		return(0);
	}
	dir = get_dir(monster->row, monster->col, row, col);
	bounce(FIRE, dir, monster->row, monster->col, 0);

	return(1);
}

static int
get_dir(short srow, short scol, short drow, short dcol)
{
	if (srow == drow) {
		if (scol < dcol) {
			return(RIGHT);
		} else {
			return(LEFT);
		}
	}
	if (scol == dcol) {
		if (srow < drow) {
			return(DOWN);
		} else {
			return(UPWARD);
		}
	}
	if ((srow > drow) && (scol > dcol)) {
		return(UPLEFT);
	}
	if ((srow < drow) && (scol < dcol)) {
		return(DOWNRIGHT);
	}
	if ((srow < drow) && (scol > dcol)) {
		return(DOWNLEFT);
	}
	/*if ((srow > drow) && (scol < dcol)) {*/
		return(UPRIGHT);
	/*}*/
}
