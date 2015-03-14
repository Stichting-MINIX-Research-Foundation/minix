/*	$NetBSD: pack.c,v 1.12 2011/05/23 23:01:17 joerg Exp $	*/

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
static char sccsid[] = "@(#)pack.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: pack.c,v 1.12 2011/05/23 23:01:17 joerg Exp $");
#endif
#endif /* not lint */

/*
 * pack.c
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

const char curse_message[] = "you can't, it appears to be cursed";

static object *check_duplicate(object *, object *);
static boolean is_pack_letter(short *, unsigned short *);
static boolean mask_pack(const object *, unsigned short);
static short next_avail_ichar(void);

object *
add_to_pack(object *obj, object *pack, int condense)
{
	object *op;

	if (condense) {
		if ((op = check_duplicate(obj, pack)) != NULL) {
			free_object(obj);
			return(op);
		} else {
			obj->ichar = next_avail_ichar();
		}
	}
	if (pack->next_object == 0) {
		pack->next_object = obj;
	} else {
		op = pack->next_object;

		while (op->next_object) {
			op = op->next_object;
		}
		op->next_object = obj;
	}
	obj->next_object = 0;
	return(obj);
}

void
take_from_pack(object *obj, object *pack)
{
	while (pack->next_object != obj) {
		pack = pack->next_object;
	}
	pack->next_object = pack->next_object->next_object;
}

/* Note: *status is set to 0 if the rogue attempts to pick up a scroll
 * of scare-monster and it turns to dust.  *status is otherwise set to 1.
 */

object *
pick_up(int row, int col, short *status)
{
	object *obj;

	*status = 1;

	if (levitate) {
		messagef(0, "you're floating in the air!");
		return NULL;
	}
	obj = object_at(&level_objects, row, col);
	if (!obj) {
		messagef(1, "pick_up(): inconsistent");
		return(obj);
	}
	if (	(obj->what_is == SCROL) &&
			(obj->which_kind == SCARE_MONSTER) &&
			obj->picked_up) {
		messagef(0, "the scroll turns to dust as you pick it up");
		dungeon[row][col] &= (~OBJECT);
		vanish(obj, 0, &level_objects);
		*status = 0;
		if (id_scrolls[SCARE_MONSTER].id_status == UNIDENTIFIED) {
			id_scrolls[SCARE_MONSTER].id_status = IDENTIFIED;
		}
		return NULL;
	}
	if (obj->what_is == GOLD) {
		rogue.gold += obj->quantity;
		dungeon[row][col] &= ~(OBJECT);
		take_from_pack(obj, &level_objects);
		print_stats(STAT_GOLD);
		return(obj);	/* obj will be free_object()ed in caller */
	}
	if (pack_count(obj) >= MAX_PACK_COUNT) {
		messagef(1, "pack too full");
		return NULL;
	}
	dungeon[row][col] &= ~(OBJECT);
	take_from_pack(obj, &level_objects);
	obj = add_to_pack(obj, &rogue.pack, 1);
	obj->picked_up = 1;
	return(obj);
}

void
drop(void)
{
	object *obj, *new;
	short ch;
	char desc[DCOLS];

	if (dungeon[rogue.row][rogue.col] & (OBJECT | STAIRS | TRAP)) {
		messagef(0, "there's already something there");
		return;
	}
	if (!rogue.pack.next_object) {
		messagef(0, "you have nothing to drop");
		return;
	}
	if ((ch = pack_letter("drop what?", ALL_OBJECTS)) == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		messagef(0, "no such item.");
		return;
	}
	if (obj->in_use_flags & BEING_WIELDED) {
		if (obj->is_cursed) {
			messagef(0, "%s", curse_message);
			return;
		}
		unwield(rogue.weapon);
	} else if (obj->in_use_flags & BEING_WORN) {
		if (obj->is_cursed) {
			messagef(0, "%s", curse_message);
			return;
		}
		mv_aquatars();
		unwear(rogue.armor);
		print_stats(STAT_ARMOR);
	} else if (obj->in_use_flags & ON_EITHER_HAND) {
		if (obj->is_cursed) {
			messagef(0, "%s", curse_message);
			return;
		}
		un_put_on(obj);
	}
	obj->row = rogue.row;
	obj->col = rogue.col;

	if ((obj->quantity > 1) && (obj->what_is != WEAPON)) {
		obj->quantity--;
		new = alloc_object();
		*new = *obj;
		new->quantity = 1;
		obj = new;
	} else {
		obj->ichar = 'L';
		take_from_pack(obj, &rogue.pack);
	}
	place_at(obj, rogue.row, rogue.col);
	get_desc(obj, desc, sizeof(desc));
	messagef(0, "dropped %s", desc);
	(void)reg_move();
}

static object *
check_duplicate(object *obj, object *pack)
{
	object *op;

	if (!(obj->what_is & (WEAPON | FOOD | SCROL | POTION))) {
		return(0);
	}
	if ((obj->what_is == FOOD) && (obj->which_kind == FRUIT)) {
		return(0);
	}
	op = pack->next_object;

	while (op) {
		if ((op->what_is == obj->what_is) &&
			(op->which_kind == obj->which_kind)) {

			if ((obj->what_is != WEAPON) ||
			((obj->what_is == WEAPON) &&
			((obj->which_kind == ARROW) ||
			(obj->which_kind == DAGGER) ||
			(obj->which_kind == DART) ||
			(obj->which_kind == SHURIKEN)) &&
			(obj->quiver == op->quiver))) {
				op->quantity += obj->quantity;
				return(op);
			}
		}
		op = op->next_object;
	}
	return(0);
}

static short
next_avail_ichar(void)
{
	object *obj;
	int i;
	boolean ichars[26];

	for (i = 0; i < 26; i++) {
		ichars[i] = 0;
	}
	obj = rogue.pack.next_object;
	while (obj) {
		if (obj->ichar >= 'a' && obj->ichar <= 'z') {
			ichars[(obj->ichar - 'a')] = 1;
		}
		obj = obj->next_object;
	}
	for (i = 0; i < 26; i++) {
		if (!ichars[i]) {
			return(i + 'a');
		}
	}
	return('?');
}

void
wait_for_ack(void)
{
	while (rgetchar() != ' ')
		;
}

short
pack_letter(const char *prompt, unsigned short mask)
{
	short ch;
	unsigned short tmask = mask;

	if (!mask_pack(&rogue.pack, mask)) {
		messagef(0, "nothing appropriate");
		return(CANCEL);
	}
	for (;;) {

		messagef(0, "%s", prompt);

		for (;;) {
			ch = rgetchar();
			if (!is_pack_letter(&ch, &mask)) {
				sound_bell();
			} else {
				break;
			}
		}

		if (ch == LIST) {
			check_message();
			mask = tmask;
			inventory(&rogue.pack, mask);
		} else {
			break;
		}
		mask = tmask;
	}
	check_message();
	return(ch);
}

void
take_off(void)
{
	char desc[DCOLS];
	object *obj;

	if (rogue.armor) {
		if (rogue.armor->is_cursed) {
			messagef(0, "%s", curse_message);
		} else {
			mv_aquatars();
			obj = rogue.armor;
			unwear(rogue.armor);
			get_desc(obj, desc, sizeof(desc));
			messagef(0, "was wearing %s", desc);
			print_stats(STAT_ARMOR);
			(void)reg_move();
		}
	} else {
		messagef(0, "not wearing any");
	}
}

void
wear(void)
{
	short ch;
	object *obj;
	char desc[DCOLS];

	if (rogue.armor) {
		messagef(0, "you're already wearing some");
		return;
	}
	ch = pack_letter("wear what?", ARMOR);

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		messagef(0, "no such item.");
		return;
	}
	if (obj->what_is != ARMOR) {
		messagef(0, "you can't wear that");
		return;
	}
	obj->identified = 1;
	get_desc(obj, desc, sizeof(desc));
	messagef(0, "wearing %s", desc);
	do_wear(obj);
	print_stats(STAT_ARMOR);
	(void)reg_move();
}

void
unwear(object *obj)
{
	if (obj) {
		obj->in_use_flags &= (~BEING_WORN);
	}
	rogue.armor = NULL;
}

void
do_wear(object *obj)
{
	rogue.armor = obj;
	obj->in_use_flags |= BEING_WORN;
	obj->identified = 1;
}

void
wield(void)
{
	short ch;
	object *obj;
	char desc[DCOLS];

	if (rogue.weapon && rogue.weapon->is_cursed) {
		messagef(0, "%s", curse_message);
		return;
	}
	ch = pack_letter("wield what?", WEAPON);

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		messagef(0, "No such item.");
		return;
	}
	if (obj->what_is & (ARMOR | RING)) {
		messagef(0, "you can't wield %s",
			((obj->what_is == ARMOR) ? "armor" : "rings"));
		return;
	}
	if (obj->in_use_flags & BEING_WIELDED) {
		messagef(0, "in use");
	} else {
		unwield(rogue.weapon);
		get_desc(obj, desc, sizeof(desc));
		messagef(0, "wielding %s", desc);
		do_wield(obj);
		(void)reg_move();
	}
}

void
do_wield(object *obj)
{
	rogue.weapon = obj;
	obj->in_use_flags |= BEING_WIELDED;
}

void
unwield(object *obj)
{
	if (obj) {
		obj->in_use_flags &= (~BEING_WIELDED);
	}
	rogue.weapon = NULL;
}

void
call_it(void)
{
	short ch;
	object *obj;
	struct id *id_table;
	char buf[MAX_TITLE_LENGTH+2];

	ch = pack_letter("call what?", (SCROL | POTION | WAND | RING));

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		messagef(0, "no such item.");
		return;
	}
	if (!(obj->what_is & (SCROL | POTION | WAND | RING))) {
		messagef(0, "surely you already know what that's called");
		return;
	}
	id_table = get_id_table(obj);

	if (get_input_line("call it:", "", buf, sizeof(buf),
			id_table[obj->which_kind].title, 1, 1)) {
		id_table[obj->which_kind].id_status = CALLED;
		(void)strlcpy(id_table[obj->which_kind].title, buf,
				sizeof(id_table[obj->which_kind].title));
	}
}

short
pack_count(const object *new_obj)
{
	object *obj;
	short count = 0;

	obj = rogue.pack.next_object;

	while (obj) {
		if (obj->what_is != WEAPON) {
			count += obj->quantity;
		} else if (!new_obj) {
			count++;
		} else if ((new_obj->what_is != WEAPON) ||
			((obj->which_kind != ARROW) &&
			(obj->which_kind != DAGGER) &&
			(obj->which_kind != DART) &&
			(obj->which_kind != SHURIKEN)) ||
			(new_obj->which_kind != obj->which_kind) ||
			(obj->quiver != new_obj->quiver)) {
			count++;
		}
		obj = obj->next_object;
	}
	return(count);
}

static boolean
mask_pack(const object *pack, unsigned short mask)
{
	while (pack->next_object) {
		pack = pack->next_object;
		if (pack->what_is & mask) {
			return(1);
		}
	}
	return(0);
}

static boolean
is_pack_letter(short *c, unsigned short *mask)
{
	if (((*c == '?') || (*c == '!') || (*c == ':') || (*c == '=') ||
		(*c == ')') || (*c == ']') || (*c == '/') || (*c == ','))) {
		switch(*c) {
		case '?':
			*mask = SCROL;
			break;
		case '!':
			*mask = POTION;
			break;
		case ':':
			*mask = FOOD;
			break;
		case ')':
			*mask = WEAPON;
			break;
		case ']':
			*mask = ARMOR;
			break;
		case '/':
			*mask = WAND;
			break;
		case '=':
			*mask = RING;
			break;
		case ',':
			*mask = AMULET;
			break;
		}
		*c = LIST;
		return(1);
	}
	return(((*c >= 'a') && (*c <= 'z')) || (*c == CANCEL) || (*c == LIST));
}

boolean
has_amulet(void)
{
	return(mask_pack(&rogue.pack, AMULET));
}

void
kick_into_pack(void)
{
	object *obj;
	char desc[DCOLS];
	short stat;

	if (!(dungeon[rogue.row][rogue.col] & OBJECT)) {
		messagef(0, "nothing here");
	} else {
		if ((obj = pick_up(rogue.row, rogue.col, &stat)) != NULL) {
			get_desc(obj, desc, sizeof(desc));
			if (obj->what_is == GOLD) {
				messagef(0, "%s", desc);
				free_object(obj);
			} else {
				messagef(0, "%s(%c)", desc, obj->ichar);
			}
		}
		if (obj || (!stat)) {
			(void)reg_move();
		}
	}
}
