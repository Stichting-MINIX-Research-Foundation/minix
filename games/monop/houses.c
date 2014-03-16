/*	$NetBSD: houses.c,v 1.15 2012/06/19 05:35:32 dholland Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)houses.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: houses.c,v 1.15 2012/06/19 05:35:32 dholland Exp $");
#endif
#endif /* not lint */

#include "monop.h"

static const char	*names[N_MON+2];
static char	cur_prop[80];

static MON	*monops[N_MON];

static void buy_h(MON *);
static void sell_h(MON *);
static void list_cur(MON *);
static int get_avail_houses(void);
static int get_avail_hotels(void);
static bool ready_for_hotels(MON *);

/*
 *	These routines deal with buying and selling houses
 */
void
buy_houses(void)
{
	int num_mon;
	MON *mp;
	OWN *op;
	bool good, got_morg;
	int i,p;

over:
	num_mon = 0;
	good = TRUE;
	got_morg = FALSE;
	for (op = cur_p->own_list; op && op->sqr->type != PRPTY; op = op->next)
		continue;
	while (op)
		if (op->sqr->desc->monop) {
			mp = op->sqr->desc->mon_desc;
			names[num_mon] = (monops[num_mon]=mp)->name;
			num_mon++;
			got_morg = good = FALSE;
			for (i = 0; i < mp->num_in; i++) {
				if (op->sqr->desc->morg)
					got_morg = TRUE;
				if (op->sqr->desc->houses != 5)
					good = TRUE;
				op = op->next;
			}
			if (!good || got_morg)
				--num_mon;
		}
		else
			op = op->next;
	if (num_mon == 0) {
		if (got_morg)
			printf("You can't build on mortgaged monopolies.\n");
		else if (!good)
			printf("You can't build any more.\n");
		else
			printf("But you don't have any monopolies!!\n");
		return;
	}
	if (num_mon == 1)
		buy_h(monops[0]);
	else {
		names[num_mon++] = "done";
		names[num_mon--] = 0;
		if ((p = getinp(
		    "Which property do you wish to buy houses for? ",
		    names)) == num_mon)
			return;
		buy_h(monops[p]);
		goto over;
	}
}

static void
buy_h(MON *mnp)
{
	int i;
	MON *mp;
	int price;
	short input[3], result[3];
	int wanted_houses, wanted_hotels;
	int total_purchase;
	PROP *pp;
	int avail_houses, avail_hotels;
	bool buying_hotels;

	mp = mnp;
	price = mp->h_cost * 50;

	avail_houses = get_avail_houses();
	avail_hotels = get_avail_hotels();
	buying_hotels = ready_for_hotels(mnp);

	if (avail_houses == 0 && !buying_hotels) {
		printf("Building shortage:  no houses available.");
		return;
	}
	if (avail_hotels == 0 && buying_hotels) {
		printf("Building shortage:  no hotels available.");
		return;
	}

blew_it:
	list_cur(mp);
	printf("Houses will cost $%d\n", price);
	printf("How many houses do you wish to buy for\n");
	for (i = 0; i < mp->num_in; i++) {
		pp = mp->sq[i]->desc;
over:
		if (pp->houses == 5) {
			printf("%s (H):\n", mp->sq[i]->name);
			input[i] = 0;
			result[i] = 5;
			continue;
		}
		(void)snprintf(cur_prop, sizeof(cur_prop), "%s (%d): ",
			mp->sq[i]->name, pp->houses);
		input[i] = get_int(cur_prop);
		result[i] = input[i] + pp->houses;
		if (result[i] > 5 || result[i] < 0) {
			printf("That's too many.  The most you can buy is %d\n",
			    5 - pp->houses);
				goto over;
			}
	}
	if (mp->num_in == 3 &&
	    (abs(result[0] - result[1]) > 1 ||
	    abs(result[0] - result[2]) > 1 ||
	     abs(result[1] - result[2]) > 1)) {
err:		printf("That makes the spread too wide.  Try again\n");
		goto blew_it;
	}
	else if (mp->num_in == 2 && abs(result[0] - result[1]) > 1)
		goto err;

	wanted_houses = 0;
	wanted_hotels = 0;
	total_purchase = 0;

	for (i = 0; i < mp->num_in; i++) {
		wanted_houses += input[i];
		total_purchase += input[i];
		if (result[i] == 5 && input[i] > 0) {
			wanted_hotels++;
			wanted_houses--;
		}
	}
	if (wanted_houses > avail_houses) {
		printf("You have asked for %d %s but only %d are available.  "
		    "Try again\n",
		    wanted_houses, wanted_houses == 1 ? "house" : "houses",
		    avail_houses);
		goto blew_it;
	} else if (wanted_hotels > avail_hotels) {
		printf("You have asked for %d %s but only %d are available.  "
		    "Try again\n",
		    wanted_hotels, wanted_hotels == 1 ? "hotel" : "hotels",
		    avail_hotels);
		goto blew_it;
	}

	if (total_purchase) {
		printf("You asked for %d %s and %d %s for $%d\n",
		    wanted_houses, wanted_houses == 1 ? "house" : "houses",
		    wanted_hotels, wanted_hotels == 1 ? "hotel" : "hotels",
		    total_purchase * price);
		if (getyn("Is that ok? ") == 0) {
			cur_p->money -= total_purchase * price;
			for (i = 0; i < mp->num_in; i++)
				mp->sq[i]->desc->houses = result[i];
		}
	}
}

/*
 *	This routine sells houses.
 */
void
sell_houses(void)
{
	int num_mon;
	MON *mp;
	OWN *op;
	bool good;
	int p;

over:
	num_mon = 0;
	good = TRUE;
	for (op = cur_p->own_list; op;)
		if (op->sqr->type == PRPTY && op->sqr->desc->monop) {
			mp = op->sqr->desc->mon_desc;
			names[num_mon] = (monops[num_mon]=mp)->name;
			num_mon++;
			good = 0;
			do
				if (!good && op->sqr->desc->houses != 0)
					good = TRUE;
			while (op->next && op->sqr->desc->mon_desc == mp
			    && (op = op->next));
			if (!good)
				--num_mon;
		} else
			op = op->next;
	if (num_mon == 0) {
		printf("You don't have any houses to sell!!\n");
		return;
	}
	if (num_mon == 1)
		sell_h(monops[0]);
	else {
		names[num_mon++] = "done";
		names[num_mon--] = 0;
		if ((p = getinp(
		    "Which property do you wish to sell houses from? ",
		    names)) == num_mon)
			return;
		sell_h(monops[p]);
		notify();
		goto over;
	}
}

static void
sell_h(MON *mnp)
{
	int i;
	MON *mp;
	int price;
	short input[3],temp[3];
	int tot;
	PROP *pp;

	mp = mnp;
	price = mp->h_cost * 25;
blew_it:
	printf("Houses will get you $%d apiece\n", price);
	list_cur(mp);
	printf("How many houses do you wish to sell from\n");
	for (i = 0; i < mp->num_in; i++) {
		pp = mp->sq[i]->desc;
over:
		if (pp->houses == 0) {
			printf("%s (0):\n", mp->sq[i]->name);
			input[i] = temp[i] = 0;
			continue;
		}
		if (pp->houses < 5)
			(void)snprintf(cur_prop, sizeof(cur_prop), "%s (%d): ",
				mp->sq[i]->name,pp->houses);
		else
			(void)snprintf(cur_prop, sizeof(cur_prop), "%s (H): ",
				mp->sq[i]->name);
		input[i] = get_int(cur_prop);
		temp[i] = pp->houses - input[i];
		if (temp[i] < 0) {
			printf(
			    "That's too many.  The most you can sell is %d\n",
			    pp->houses);
				goto over;
			}
	}
	if (mp->num_in == 3 && (abs(temp[0] - temp[1]) > 1 ||
	    abs(temp[0] - temp[2]) > 1 || abs(temp[1] - temp[2]) > 1)) {
err:		printf("That makes the spread too wide.  Try again\n");
		goto blew_it;
	}
	else if (mp->num_in == 2 && abs(temp[0] - temp[1]) > 1)
		goto err;
	for (tot = i = 0; i < mp->num_in; i++)
		tot += input[i];
	if (tot) {
		printf("You asked to sell %d house%s for $%d\n", tot,
		    tot == 1 ? "" : "s", tot * price);
		if (getyn("Is that ok? ") == 0) {
			cur_p->money += tot * price;
			for (tot = i = 0; i < mp->num_in; i++)
				mp->sq[i]->desc->houses = temp[i];
		}
	}
}

static void
list_cur(MON *mp)
{
	int i;
	SQUARE *sqp;

	for (i = 0; i < mp->num_in; i++) {
		sqp = mp->sq[i];
		if (sqp->desc->houses == 5)
			printf("%s (H) ", sqp->name);
		else
			printf("%s (%d) ", sqp->name, sqp->desc->houses);
	}
	putchar('\n');
}

static int
get_avail_houses(void)
{
	int i, c;
	SQUARE *sqp;

	c = 0;
	for (i = 0; i < N_SQRS; i++) {
		sqp = &board[i];
		if (sqp->type == PRPTY && sqp->owner >= 0 && sqp->desc->monop) {
			if (sqp->desc->houses < 5 && sqp->desc->houses > 0)
				c += sqp->desc->houses;
		}
	}
	return(N_HOUSE - c);
}

static int
get_avail_hotels(void)
{
	int i, c;
	SQUARE *sqp;

	c = 0;
	for (i = 0; i < N_SQRS; i++) {
		sqp = &board[i];
		if (sqp->type == PRPTY && sqp->owner >= 0 && sqp->desc->monop) {
			if (sqp->desc->houses == 5)
				c++;
		}
	}
	return(N_HOTEL - c);
}

/*
 * If we can put a hotel on, we can't put any houses on, and if we can
 * put houses on, then we can't put a hotel on yet.
 */
static bool
ready_for_hotels(MON *mp)
{
	int i;

	for (i = 0; i < mp->num_in; i++) {
		if (mp->sq[i]->desc->houses < 4)
			return(FALSE);
	}
	return(TRUE);
}
