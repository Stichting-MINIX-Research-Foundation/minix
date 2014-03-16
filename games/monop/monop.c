/*	$NetBSD: monop.c,v 1.27 2012/06/19 05:35:32 dholland Exp $	*/

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
__COPYRIGHT("@(#) Copyright (c) 1980, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)monop.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: monop.c,v 1.27 2012/06/19 05:35:32 dholland Exp $");
#endif
#endif /* not lint */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "deck.h"
#include "monop.h"

int main(int, char *[]);
static void getplayers(void);
static void init_players(void);
static void init_monops(void);
static void do_quit(int);


bool	fixing,			/* set if fixing up debt		*/
	trading,		/* set if in process of trading		*/
	told_em,		/* set if told user he's out of debt	*/
	spec;			/* set if moving by card to RR or UTIL	*/

const char *name_list[MAX_PL+2];	/* list of players' names	*/
static const char *const comlist[] = {	/* list of normal commands 	*/
	"quit",		/*  0 */	"print",	/*  1 */
	"where",	/*  2 */	"own holdings",	/*  3 */
	"holdings",	/*  4 */	"mortgage",	/*  5 */
	"unmortgage",	/*  6 */	"buy houses",	/*  7 */
	"sell houses",	/*  8 */	"card",		/*  9 */
	"pay",		/* 10 */	"trade",	/* 11 */
	"resign",	/* 12 */	"save",		/* 13 */
	"restore",	/* 14 */	"roll",		/* 15 */
	"",		/* 16 */
	0
};
const char *const yncoms[] = {	/* list of commands for yes/no answers	*/
	"yes",		/*  0 */	"no",		/*  1 */
	"quit",		/*  2 */	"print",	/*  3 */
	"where",	/*  4 */	"own holdings",	/*  5 */
	"holdings",	/*  6 */
	0
};
const char *const lucky_mes[]	= {	/* "got lucky" messages		*/
	"You lucky stiff",		"You got lucky",
	"What a lucky person!",		"You must have a 4-leaf clover",
	"My, my!  Aren't we lucky!",	"Luck smiles upon you",
	"You got lucky this time",	"Lucky person!",
	"Your karma must certainly be together",
	"How beautifully Cosmic",	"Wow, you must be really with it"
	/* "I want your autograph",	-- Save for later */
};

int	player,			/* current player number		*/
	num_play,		/* current number of players		*/
	num_doub,		/* # of doubles current player rolled	*/
				/* # of "got lucky" messages		*/
	num_luck	= sizeof lucky_mes / sizeof (char *);

/* list of command functions		*/
void (*const func[])(void) = { /* array of function calls for commands */
	quit,			/* quit game		|*  0 *|	*/
	printboard,		/* print board		|*  1 *|	*/
	where,			/* where players are	|*  2 *|	*/
	list,			/* own holdings		|*  3 *|	*/
	list_all,		/* holdings list	|*  4 *|	*/
	mortgage,		/* mortgage property	|*  5 *|	*/
	unmortgage,		/* unmortgage property	|*  6 *|	*/
	buy_houses,		/* buy houses		|*  7 *|	*/
	sell_houses,		/* sell houses		|*  8 *|	*/
	card,			/* card for jail	|*  9 *|	*/
	pay,			/* pay for jail		|* 10 *|	*/
	trade,			/* trade		|* 11 *|	*/
	resign,			/* resign		|* 12 *|	*/
	save,			/* save game		|* 13 *|	*/
	restore,		/* restore game		|* 14 *|	*/
	do_move,		/* roll			|* 15 *|	*/
	do_move			/* ""			|* 16 *|	*/
	};

DECK	deck[2];		/* Chance and Community Chest		*/

PLAY	*play,			/* player structure array ("calloc"ed)	*/
	*cur_p;			/* pointer to current player's struct	*/

static RR_S rr[N_RR];		/* railroad descriptions		*/

static UTIL_S util[2];		/* utility descriptions			*/

#define MONINIT(num_in, h_cost, not_m, mon_n, sq1,sq2,sq3) \
     {0,    -1, num_in, 0,      h_cost, not_m, mon_n, {sq1,sq2,sq3}, {0,0,0}}
/* name  owner          num_own                                      sq */

static MON mon[N_MON] = {	/* monopoly descriptions		*/
/*   num_in h_cost  not_m	mon_n	    sqnums */
MONINIT(2,  1,	"Purple",	"PURPLE",   1,3, 0),
MONINIT(3,  1,	"Lt. Blue",	"LT. BLUE", 6,8,9),
MONINIT(3,  2,	"Violet",	"VIOLET",   11,13,14),
MONINIT(3,  2,	"Orange",	"ORANGE",   16,18,19),
MONINIT(3,  3,	"Red",		"RED",	    21,23,24),
MONINIT(3,  3,	"Yellow",	"YELLOW",   26,27,29),
MONINIT(3,  4,	"Green",	"GREEN",    31,32,34),
MONINIT(2,  4,	"Dk. Blue",	"DK. BLUE", 37,39, 0),
};
#undef MONINIT

PROP	prop[N_PROP]	= {	/* typical properties			*/
/* morg	monop	square	houses	mon_desc	rent	*/
{0,	0,	1,	0,	&mon[0],	{ 2, 10, 30,  90, 160, 250} },
{0,	0,	3,	0,	&mon[0],	{ 4, 20, 60, 180, 320, 450} },
{0,	0,	6,	0,	&mon[1],	{ 6, 30, 90, 270, 400, 550} },
{0,	0,	7,	0,	&mon[1],	{ 6, 30, 90, 270, 400, 550} },
{0,	0,	9,	0,	&mon[1],	{ 8, 40,100, 300, 450, 600} },
{0,	0,	11,	0,	&mon[2],	{10, 50,150, 450, 625, 750} },
{0,	0,	13,	0,	&mon[2],	{10, 50,150, 450, 625, 750} },
{0,	0,	14,	0,	&mon[2],	{12, 60,180, 500, 700, 900} },
{0,	0,	16,	0,	&mon[3],	{14, 70,200, 550, 750, 950} },
{0,	0,	17,	0,	&mon[3],	{14, 70,200, 550, 750, 950} },
{0,	0,	19,	0,	&mon[3],	{16, 80,220, 600, 800,1000} },
{0,	0,	21,	0,	&mon[4],	{18, 90,250, 700, 875,1050} },
{0,	0,	23,	0,	&mon[4],	{18, 90,250, 700, 875,1050} },
{0,	0,	24,	0,	&mon[4],	{20,100,300, 750, 925,1100} },
{0,	0,	26,	0,	&mon[5],	{22,110,330, 800, 975,1150} },
{0,	0,	27,	0,	&mon[5],	{22,110,330, 800, 975,1150} },
{0,	0,	29,	0,	&mon[5],	{24,120,360, 850,1025,1200} },
{0,	0,	31,	0,	&mon[6],	{26,130,390, 900,1100,1275} },
{0,	0,	32,	0,	&mon[6],	{26,130,390, 900,1100,1275} },
{0,	0,	34,	0,	&mon[6],	{28,150,450,1000,1200,1400} },
{0,	0,	37,	0,	&mon[7],	{35,175,500,1100,1300,1500} },
{0,	0,	39,	0,	&mon[7],	{50,200,600,1400,1700,2000} }
};

SQUARE	board[N_SQRS+1]	= {	/* board itself (+1 for Jail)		*/
/* name (COLOR)			owner	type	desc		cost	*/

{"=== GO ===",			-1,	SAFE,	NULL,		0	},
{"Mediterranean Ave. (P)",	-1,	PRPTY,	&prop[0],	60	},
{"Community Chest i",		-1,	CC,	NULL,		0	},
{"Baltic Ave. (P)",		-1,	PRPTY,	&prop[1],	60	},
{"Income Tax",			-1,	INC_TAX, NULL,		0	},
{"Reading RR",			-1,	RR,	&rr[0],		200	},
{"Oriental Ave. (L)",		-1,	PRPTY,	&prop[2],	100	},
{"Chance i",			-1,	CHANCE,	NULL,		0	},
{"Vermont Ave. (L)",		-1,	PRPTY,	&prop[3],	100	},
{"Connecticut Ave. (L)",	-1,	PRPTY,	&prop[4],	120	},
{"Just Visiting",		-1,	SAFE,	NULL,		0	},
{"St. Charles Pl. (V)",		-1,	PRPTY,	&prop[5],	140	},
{"Electric Co.",		-1,	UTIL,	&util[0],	150	},
{"States Ave. (V)",		-1,	PRPTY,	&prop[6],	140	},
{"Virginia Ave. (V)",		-1,	PRPTY,	&prop[7],	160	},
{"Pennsylvania RR",		-1,	RR,	&rr[1],		200	},
{"St. James Pl. (O)",		-1,	PRPTY,	&prop[8],	180	},
{"Community Chest ii",		-1,	CC,	NULL,		0	},
{"Tennessee Ave. (O)",		-1,	PRPTY,	&prop[9],	180	},
{"New York Ave. (O)",		-1,	PRPTY,	&prop[10],	200	},
{"Free Parking",		-1,	SAFE,	NULL,		0	},
{"Kentucky Ave. (R)",		-1,	PRPTY,	&prop[11],	220	},
{"Chance ii",			-1,	CHANCE,	NULL,		0	},
{"Indiana Ave. (R)",		-1,	PRPTY,	&prop[12],	220	},
{"Illinois Ave. (R)",		-1,	PRPTY,	&prop[13],	240	},
{"B&O RR",			-1,	RR,	&rr[2],		200	},
{"Atlantic Ave. (Y)",		-1,	PRPTY,	&prop[14],	260	},
{"Ventnor Ave. (Y)",		-1,	PRPTY,	&prop[15],	260	},
{"Water Works",			-1,	UTIL,	&util[1],	150	},
{"Marvin Gardens (Y)",		-1,	PRPTY,	&prop[16],	280	},
{"GO TO JAIL",			-1,	GOTO_J,	NULL,		0	},
{"Pacific Ave. (G)",		-1,	PRPTY,	&prop[17],	300	},
{"N. Carolina Ave. (G)",	-1,	PRPTY,	&prop[18],	300	},
{"Community Chest iii",		-1,	CC,	NULL,		0	},
{"Pennsylvania Ave. (G)",	-1,	PRPTY,	&prop[19],	320	},
{"Short Line RR",		-1,	RR,	&rr[3],		200	},
{"Chance iii",			-1,	CHANCE,	NULL,		0	},
{"Park Place (D)",		-1,	PRPTY,	&prop[20],	350	},
{"Luxury Tax",			-1,	LUX_TAX, NULL,		0	},
{"Boardwalk (D)",		-1,	PRPTY,	&prop[21],	400	},
{"JAIL",			-1,	IN_JAIL, NULL,		0	}
};


/*
 *	This program implements a monopoly game
 */
int
main(int ac, char *av[])
{
	/* Revoke setgid privileges */
	setgid(getgid());

	srandom((unsigned long)time(NULL));
	num_luck = sizeof lucky_mes / sizeof (char *);
	init_decks();
	init_monops();
	if (ac > 1) {
		if (rest_f(av[1]) < 0)
			restore();
	}
	else {
		getplayers();
		init_players();
	}
	signal(SIGINT, do_quit);
	for (;;) {
		printf("\n%s (%d) (cash $%d) on %s\n", cur_p->name, player + 1,
			cur_p->money, board[cur_p->loc].name);
		printturn();
		force_morg();
		execute(getinp("-- Command: ", comlist));
	}
}

/*ARGSUSED*/
static void
do_quit(int n __unused)
{
	quit();
}

/*
 *	This routine gets the names of the players
 */
static void
getplayers(void)
{
	int i, j;
	char buf[257];

blew_it:
	for (;;) {
		if ((num_play = get_int("How many players? ")) <= 1 ||
		    num_play > MAX_PL)
			printf("Sorry. Number must range from 2 to %d\n",
			    MAX_PL);
		else
			break;
	}
	cur_p = play = calloc((size_t)num_play, sizeof (PLAY));
	if (play == NULL)
		err(1, NULL);
	for (i = 0; i < num_play; i++) {
		do {
			printf("Player %d's name: ", i + 1);
			fgets(buf, sizeof(buf), stdin);
			if (feof(stdin)) {
				quit();
			}
			buf[strcspn(buf, "\n")] = '\0';
		} while (strlen(buf) == 0);
		name_list[i] = play[i].name = strdup(buf);
		if (name_list[i] == NULL)
			err(1, NULL);
		play[i].money = 1500;
	}
	name_list[i++] = "done";
	name_list[i] = 0;
	for (i = 0; i < num_play; i++)
		for (j = i + 1; j <= num_play; j++)
			if (strcasecmp(name_list[i], name_list[j]) == 0) {
				if (j != num_play)
					printf("Hey!!! Some of those are "
					    "IDENTICAL!!  Let's try that "
					    "again...\n");
				else
					printf("\"done\" is a reserved word.  "
					    "Please try again\n");
				for (i = 0; i < num_play; i++)
					free(play[i].name);
				free(play);
				goto blew_it;
			}
}

/*
 *	This routine figures out who goes first
 */
static void
init_players(void)
{
	int i, rl, cur_max;
	bool over = 0;
	int max_pl = 0;

again:
	putchar('\n');
	for (cur_max = i = 0; i < num_play; i++) {
		printf("%s (%d) rolls %d\n", play[i].name, i+1, rl=roll(2, 6));
		if (rl > cur_max) {
			over = FALSE;
			cur_max = rl;
			max_pl = i;
		}
		else if (rl == cur_max)
			over++;
	}
	if (over) {
		printf("%d people rolled the same thing, so we'll try again\n",
		    over + 1);
		goto again;
	}
	player = max_pl;
	cur_p = &play[max_pl];
	printf("%s (%d) goes first\n", cur_p->name, max_pl + 1);
}

/*
 *	This routine initializes the monopoly structures.
 */
static void
init_monops(void)
{
	MON *mp;
	int i;

	for (mp = mon; mp < &mon[N_MON]; mp++) {
		mp->name = mp->not_m;
		for (i = 0; i < mp->num_in; i++)
			mp->sq[i] = &board[mp->sqnums[i]];
	}
}
