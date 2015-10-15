/*	$NetBSD: cards.c,v 1.27 2014/12/29 10:38:52 jnemeth Exp $	*/

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
static char sccsid[] = "@(#)cards.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: cards.c,v 1.27 2014/12/29 10:38:52 jnemeth Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/endian.h>
#include "monop.h"
#include "deck.h"

/*
 *	These routine deal with the card decks
 */

static void set_up(DECK *);
static void printmes(const char *text);

#define	GOJF	'F'	/* char for get-out-of-jail-free cards	*/

struct cardinfo {
	const char *actioncode;
	const char *text;
};

static const struct cardinfo cc_cards[] = {
	{ "FF",
		">> GET OUT OF JAIL FREE <<\n"
		"Keep this card until needed or sold\n"
	},
	{ "++25",
		"Receive for Services $25.\n"
	},
	{ "++200",
		"Bank Error in Your Favor.\n"
		"Collect $200.\n"
	},
	{ "++20",
		"Income Tax Refund.\n"
		"Collect $20.\n"
	},
	{ "--100",
		"Pay Hospital $100\n"
	},
	{ "++100",
		"Life Insurance Matures.\n"
		"Collect $100\n"
	},
	{ "++45",
		"From sale of Stock You get $45.\n"
	},
	{ "TX",
		"You are Assessed for street repairs.\n"
		"\t$40 per House\n"
		"\t$115 per Hotel\n"
	},
	{ "++100",
		"X-mas Fund Matures.\n"
	  	"Collect $100.\n"
	},
	{ "++11",
		"You have won Second Prize in a Beauty Contest\n"
		"Collect $11\n"
	},
	{ "MF0",
		"Advance to GO\n"
		"(Collect $200)\n"
	},
	{ "++100",
		"You inherit $100\n"
	},
	{ "--150",
		"Pay School Tax of $150.\n"
	},
	{ "MJ",
		"\t\t>> GO TO JAIL <<\n"
	  	"Go Directly to Jail. Do not pass GO  Do not collect $200.\n"
	},
	{ "+A50",
		"\t\t>> GRAND OPERA OPENING <<\n"
		"Collect $50 from each player for opening night seats.\n"
	},
	{ "--50",
		"Doctor's Fee:  Pay $50.\n"
	}
};

static const struct cardinfo ch_cards[] = {
	{ "FF",
		">> GET OUT OF JAIL FREE <<\n"
		"Keep this card until needed or sold\n"
	},
	{ "MR",
		"Advance to the nearest Railroad, and pay owner\n"
		"Twice the rental to which he is otherwise entitled.\n"
		"If Railroad is unowned you may buy it from the bank\n"
	},
	{ "MU",
		"Advance to the nearest Utility.\n"
		"If unowned, you may buy it from the bank.\n"
		"If owned, throw dice and pay owner a total of ten times\n"
		"the amount thrown.\n"
	},
	{ "MB3",
		"Go Back 3 Spaces\n"
	},
	{ "MR",
		"Advance to the nearest Railroad, and pay owner\n"
		"Twice the rental to which he is otherwise entitled.\n"
		"If Railroad is unowned you may buy it from the bank\n"
	},
	{ "MJ",
		"    >> GO DIRECTLY TO JAIL <<\n"
		"Do not pass GO, Do not Collect $200.\n"
	},
	{ "MF5",
		"Take a Ride on the Reading.\n"
		"If you pass GO, collect $200.\n"
	},
	{ "MF39",
		"Take a Walk on the Board Walk.\n"
		"    (Advance To Board Walk)\n"
	},
	{ "MF24",
		"Advance to Illinois Ave.\n"
	},
	{ "MF0",
		"Advance to Go\n"
	},
	{ "MF11",
		"Advance to St. Charles Place.\n"
		"If you pass GO, collect $200.\n"
	},
	{ "TX",
		"Make general repairs on all of your Property.\n"
		"For Each House pay $25.\n"
		"For Each Hotel pay $100.\n"
	},
	{ "-A50",
		"You have been elected Chairman of the Board.\n"
		"Pay each player $50.\n"
	},
	{ "--15",
		"Pay Poor Tax of $15\n"
	},
	{ "++50",
		"Bank pays you Dividend of $50.\n"
	},
	{ "++150",
		"Your Building and Loan Matures.\n"
		"Collect $150.\n"
	}
};

/*
 * This routine initializes the decks from the data above.
 */
void
init_decks(void)
{
	CC_D.info = cc_cards;
	CC_D.num_cards = sizeof(cc_cards) / sizeof(cc_cards[0]);
	CH_D.info = ch_cards;
	CH_D.num_cards = sizeof(ch_cards) / sizeof(ch_cards[0]);
	set_up(&CC_D);
	set_up(&CH_D);
}

/*
 *	This routine sets up the offset pointers for the given deck.
 */
static void
set_up(DECK *dp)
{
	int r1, r2;
	int i;

	dp->cards = calloc((size_t)dp->num_cards, sizeof(dp->cards[0]));
	if (dp->cards == NULL)
		errx(1, "out of memory");

	for (i = 0; i < dp->num_cards; i++)
		dp->cards[i] = i;

	dp->top_card = 0;
	dp->gojf_used = FALSE;

	for (i = 0; i < dp->num_cards; i++) {
		int temp;

		r1 = roll(1, dp->num_cards) - 1;
		r2 = roll(1, dp->num_cards) - 1;
		temp = dp->cards[r2];
		dp->cards[r2] = dp->cards[r1];
		dp->cards[r1] = temp;
	}
}

/*
 *	This routine draws a card from the given deck
 */
void
get_card(DECK *dp)
{
	char type_maj, type_min;
	int num;
	int i, per_h, per_H, num_h, num_H;
	OWN *op;
	const struct cardinfo *thiscard;

	do {
		thiscard = &dp->info[dp->top_card];
		type_maj = thiscard->actioncode[0];
		dp->top_card = (dp->top_card + 1) % dp->num_cards;
	} while (dp->gojf_used && type_maj == GOJF);
	type_min = thiscard->actioncode[1];
	num = atoi(thiscard->actioncode+2);

	printmes(thiscard->text);
	switch (type_maj) {
	  case '+':		/* get money		*/
		if (type_min == 'A') {
			for (i = 0; i < num_play; i++)
				if (i != player)
					play[i].money -= num;
			num = num * (num_play - 1);
		}
		cur_p->money += num;
		break;
	  case '-':		/* lose money		*/
		if (type_min == 'A') {
			for (i = 0; i < num_play; i++)
				if (i != player)
					play[i].money += num;
			num = num * (num_play - 1);
		}
		cur_p->money -= num;
		break;
	  case 'M':		/* move somewhere	*/
		switch (type_min) {
		  case 'F':		/* move forward	*/
			num -= cur_p->loc;
			if (num < 0)
				num += 40;
			break;
		  case 'J':		/* move to jail	*/
			goto_jail();
			return;
		  case 'R':		/* move to railroad	*/
			spec = TRUE;
			num = (int)((cur_p->loc + 5)/10)*10 + 5 - cur_p->loc;
			break;
		  case 'U':		/* move to utility	*/
			spec = TRUE;
			if (cur_p->loc >= 12 && cur_p->loc < 28)
				num = 28 - cur_p->loc;
			else {
				num = 12 - cur_p->loc;
				if (num < 0)
					num += 40;
			}
			break;
		  case 'B':
			num = -num;
			break;
		}
		move(num);
		break;
	  case 'T':			/* tax			*/
		if (dp == &CC_D) {
			per_h = 40;
			per_H = 115;
		}
		else {
			per_h = 25;
			per_H = 100;
		}
		num_h = num_H = 0;
		for (op = cur_p->own_list; op; op = op->next)
			if (op->sqr->type == PRPTY) {
				if (op->sqr->desc->houses == 5)
					++num_H;
				else
					num_h += op->sqr->desc->houses;
			}
		num = per_h * num_h + per_H * num_H;
		printf(
		    "You had %d Houses and %d Hotels, so that cost you $%d\n",
		    num_h, num_H, num);
		if (num == 0)
			lucky("");
		else
			cur_p->money -= num;
		break;
	  case GOJF:		/* get-out-of-jail-free card	*/
		cur_p->num_gojf++;
		dp->gojf_used = TRUE;
		break;
	}
	spec = FALSE;
}

/*
 *	This routine prints out the message on the card
 */
static void
printmes(const char *text)
{
	int i;

	printline();
	fflush(stdout);
	for (i = 0; text[i] != '\0'; i++)
		putchar(text[i]);
	printline();
	fflush(stdout);
}

/*
 *	This routine returns the players get-out-of-jail-free card
 * to the bottom of a deck.  XXX currently does not return to the correct
 * deck.
 */
void
ret_card(PLAY *plr)
{
	char type_maj;
	int gojfpos, last_card;
	int i;
	DECK *dp;
	int temp;

	plr->num_gojf--;
	if (CC_D.gojf_used)
		dp = &CC_D;
	else
		dp = &CH_D;
	dp->gojf_used = FALSE;

	/* Put at bottom of deck (top_card - 1) and remove it from wherever else
	 * it used to be.
	 */
	last_card = dp->top_card - 1;
	if (last_card < 0)
		last_card += dp->num_cards;
	gojfpos = dp->top_card;
	do {
		gojfpos = (gojfpos + 1) % dp->num_cards;
		type_maj = dp->info[gojfpos].actioncode[0];
	} while (type_maj != GOJF);
	temp = dp->cards[gojfpos];
	/* Only one of the next two loops does anything */
	for (i = gojfpos - 1; i > last_card; i--)
		dp->cards[i + 1] = dp->cards[i];
	for (i = gojfpos; i < last_card; i++)
		dp->cards[i] = dp->cards[i + 1];
	if (gojfpos > last_card) {
		dp->cards[dp->top_card] = temp;
		dp->top_card++;
		dp->top_card %= dp->num_cards;
	} else
		dp->cards[last_card] = temp;
}
