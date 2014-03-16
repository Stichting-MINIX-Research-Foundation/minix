/*	$NetBSD: execute.c,v 1.22 2012/06/19 05:35:32 dholland Exp $	*/

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
static char sccsid[] = "@(#)execute.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: execute.c,v 1.22 2012/06/19 05:35:32 dholland Exp $");
#endif
#endif /* not lint */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include "deck.h"
#include "monop.h"

#define MIN_FORMAT_VERSION 1
#define CUR_FORMAT_VERSION 1
#define MAX_FORMAT_VERSION 1

typedef	struct stat	STAT;
typedef	struct tm	TIME;

static char	buf[257];

static bool	new_play;	/* set if move on to new player		*/

static void show_move(void);

static void restore_reset(void);
static int restore_parseline(char *txt);
static int restore_toplevel_attr(const char *attribute, char *txt);
static int restore_player_attr(const char *attribute, char *txt);
static int restore_deck_attr(const char *attribute, char *txt);
static int restore_square_attr(const char *attribute, char *txt);
static int getnum(const char *what, char *txt, int min, int max, int *ret);
static int getnum_withbrace(const char *what, char *txt, int min, int max,
		int *ret);

/*
 *	This routine executes the given command by index number
 */
void
execute(int com_num)
{
	new_play = FALSE;	/* new_play is true if fixing	*/
	(*func[com_num])();
	notify();
	force_morg();
	if (new_play)
		next_play();
	else if (num_doub)
		printf("%s rolled doubles.  Goes again\n", cur_p->name);
}

/*
 *	This routine moves a piece around.
 */
void
do_move(void)
{
	int r1, r2;
	bool was_jail;

	new_play = was_jail = FALSE;
	printf("roll is %d, %d\n", r1=roll(1, 6), r2=roll(1, 6));
	if (cur_p->loc == JAIL) {
		was_jail++;
		if (!move_jail(r1, r2)) {
			new_play++;
			goto ret;
		}
	}
	else {
		if (r1 == r2 && ++num_doub == 3) {
			printf("That's 3 doubles.  You go to jail\n");
			goto_jail();
			new_play++;
			goto ret;
		}
		move(r1+r2);
	}
	if (r1 != r2 || was_jail)
		new_play++;
ret:
	return;
}

/*
 *	This routine moves a normal move
 */
void
move(int rl)
{
	int old_loc;

	old_loc = cur_p->loc;
	cur_p->loc = (cur_p->loc + rl) % N_SQRS;
	if (cur_p->loc < old_loc && rl > 0) {
		cur_p->money += 200;
		printf("You pass %s and get $200\n", board[0].name);
	}
	show_move();
}

/*
 *	This routine shows the results of a move
 */
static void
show_move(void)
{
	SQUARE *sqp;

	sqp = &board[cur_p->loc];
	printf("That puts you on %s\n", sqp->name);
	switch (sqp->type) {
	  case SAFE:
		printf("That is a safe place\n");
		break;
	  case CC:
		cc();
		break;
	  case CHANCE:
		chance();
		break;
	  case INC_TAX:
		inc_tax();
		break;
	  case GOTO_J:
		goto_jail();
		break;
	  case LUX_TAX:
		lux_tax();
		break;
	  case PRPTY:
	  case RR:
	  case UTIL:
		if (sqp->owner < 0) {
			printf("That would cost $%d\n", sqp->cost);
			if (getyn("Do you want to buy? ") == 0) {
				buy(player, sqp);
				cur_p->money -= sqp->cost;
			}
			else if (num_play > 2)
				bid();
		}
		else if (sqp->owner == player)
			printf("You own it.\n");
		else
			rent(sqp);
	}
}

/*
 * Reset the game state.
 */
static void
reset_game(void)
{
	int i;

	for (i = 0; i < N_SQRS; i++) {
		board[i].owner = -1;
		if (board[i].type == PRPTY) {
			board[i].desc->morg = 0;
			board[i].desc->houses = 0;
		} else if (board[i].type == RR || board[i].type == UTIL) {
			board[i].desc->morg = 0;
		}
	}

	for (i = 0; i < 2; i++) {
		deck[i].top_card = 0;
		deck[i].gojf_used = FALSE;
	}

	if (play) {
		for (i = 0; i < num_play; i++) {
			free(play[i].name);
			play[i].name = NULL;
		}
		free(play);
		play = NULL;
	}

	for (i = 0; i < MAX_PL+2; i++) {
		name_list[i] = NULL;
	}

	cur_p = NULL;
	num_play = 0;
	player = 0;
	num_doub = 0;
	fixing = FALSE;
	trading = FALSE;
	told_em = FALSE;
	spec = FALSE;
}


/*
 *	This routine saves the current game for use at a later date
 */
void
save(void)
{
	char *sp;
	FILE *outf;
	time_t t;
	struct stat sb;
	int i, j;

	printf("Which file do you wish to save it in? ");
	fgets(buf, sizeof(buf), stdin);
	if (feof(stdin))
		return;
	sp = strchr(buf, '\n');
	if (sp)
		*sp = '\0';

	/*
	 * check for existing files, and confirm overwrite if needed
	 */

	if (stat(buf, &sb) == 0
	    && getyn("File exists.  Do you wish to overwrite? ") > 0)
		return;

	outf = fopen(buf, "w");
	if (outf == NULL) {
		warn("%s", buf);
		return;
	}
	printf("\"%s\" ", buf);
	time(&t);			/* get current time		*/

	/* Header */
	fprintf(outf, "NetBSD monop format v%d\n", CUR_FORMAT_VERSION);
	fprintf(outf, "time %s", ctime(&t));  /* ctime includes a \n */
	fprintf(outf, "numplayers %d\n", num_play);
	fprintf(outf, "currentplayer %d\n", player);
	fprintf(outf, "doubles %d\n", num_doub);

	/* Players */
	for (i = 0; i < num_play; i++) {
		fprintf(outf, "player %d {\n", i);
		fprintf(outf, "    name %s\n", name_list[i]);
		fprintf(outf, "    money %d\n", play[i].money);
		fprintf(outf, "    loc %d\n", play[i].loc);
		fprintf(outf, "    num_gojf %d\n", play[i].num_gojf);
		fprintf(outf, "    in_jail %d\n", play[i].in_jail);
		fprintf(outf, "}\n");
	}

	/* Decks */
	for (i = 0; i < 2; i++) {
		fprintf(outf, "deck %d {\n", i);
		fprintf(outf, "    numcards %d\n", deck[i].num_cards);
		fprintf(outf, "    topcard %d\n", deck[i].top_card);
		fprintf(outf, "    gojf_used %d\n", deck[i].gojf_used);
		fprintf(outf, "    cards");
		for (j = 0; j < deck[i].num_cards; j++)
			fprintf(outf, " %d", deck[i].cards[j]);
		fprintf(outf, "\n");
		fprintf(outf, "}\n");
	}

	/* Board */
	for (i = 0; i < N_SQRS; i++) {
		fprintf(outf, "square %d {\n", i);
		fprintf(outf, "owner %d\n", board[i].owner);
		if (board[i].owner < 0) {
			/* nothing */
		} else if (board[i].type == PRPTY) {
			fprintf(outf, "morg %d\n", board[i].desc->morg);
			fprintf(outf, "houses %d\n", board[i].desc->houses);
		} else if (board[i].type == RR || board[i].type == UTIL) {
			fprintf(outf, "morg %d\n", board[i].desc->morg);
		}
		fprintf(outf, "}\n");
	}
	if (ferror(outf) || fflush(outf))
		warnx("write error");
	fclose(outf);

	strcpy(buf, ctime(&t));
	for (sp = buf; *sp != '\n'; sp++)
		continue;
	*sp = '\0';
	printf("[%s]\n", buf);
}

/*
 *	This routine restores an old game from a file
 */
void
restore(void)
{
	char *sp;

	for (;;) {
		printf("Which file do you wish to restore from? ");
		fgets(buf, sizeof(buf), stdin);
		if (feof(stdin))
			return;
		sp = strchr(buf, '\n');
		if (sp)
			*sp = '\0';
		if (rest_f(buf) == 0)
			break;
	}
}

/*
 * This does the actual restoring.  It returns zero on success,
 * and -1 on failure.
 */
int
rest_f(const char *file)
{
	char *sp;
	FILE *inf;
	char xbuf[80];
	STAT sbuf;
	char readbuf[512];
	int ret = 0;

	inf = fopen(file, "r");
	if (inf == NULL) {
		warn("%s", file);
		return -1;
	}
	printf("\"%s\" ", file);
	if (fstat(fileno(inf), &sbuf) < 0) {
		err(1, "%s: fstat", file);
	}

	/* Clear the game state to prevent brokenness on misordered files. */
	reset_game();

	/* Reset the parser */
	restore_reset();

	/* Note: can't use buf[], file might point at it. (Lame...) */
	while (fgets(readbuf, sizeof(readbuf), inf)) {
		/*
		 * The input buffer is long enough to handle anything
		 * that's supposed to be in the output buffer, so if
		 * we get a partial line, complain.
		 */
		sp = strchr(readbuf, '\n');
		if (sp == NULL) {
			printf("file is corrupt: long lines.\n");
			ret = -1;
			break;
		}
		*sp = '\0';

		if (restore_parseline(readbuf)) {
			ret = -1;
			break;
		}
	}

	if (ferror(inf))
		warnx("%s: read error", file);
	fclose(inf);

	if (ret < 0)
		return -1;

	name_list[num_play] = "done";

	if (play == NULL || cur_p == NULL || num_play < 2) {
		printf("save file is incomplete.\n");
		return -1;
	}

	/*
	 * We could at this point crosscheck the following:
	 *    - there are only two GOJF cards floating around
	 *    - total number of houses and hotels does not exceed maximums
	 *    - no props are both built and mortgaged
	 * but for now we don't.
	 */

	strcpy(xbuf, ctime(&sbuf.st_mtime));
	for (sp = xbuf; *sp != '\n'; sp++)
		continue;
	*sp = '\0';
	printf("[%s]\n", xbuf);
	return 0;
}

/*
 * State of the restore parser
 */
static int restore_version;
static enum {
	RI_NONE,
	RI_PLAYER,
	RI_DECK,
	RI_SQUARE
} restore_item;
static int restore_itemnum;

/*
 * Reset the restore parser
 */
static void
restore_reset(void)
{
	restore_version = -1;
	restore_item = RI_NONE;
	restore_itemnum = -1;
}

/*
 * Handle one line of the save file
 */
static int
restore_parseline(char *txt)
{
	char *attribute;
	char *s;

	if (restore_version < 0) {
		/* Haven't seen the header yet. Demand it right away. */
		if (!strncmp(txt, "NetBSD monop format v", 21)) {
			return getnum("format version", txt+21,
				      MIN_FORMAT_VERSION,
				      MAX_FORMAT_VERSION,
				      &restore_version);
		}
		printf("file is not a monop save file.\n");
		return -1;
	}

	/* Check for lines that are right braces. */
	if (!strcmp(txt, "}")) {
		if (restore_item == RI_NONE) {
			printf("mismatched close brace.\n");
			return -1;
		}
		restore_item = RI_NONE;
		restore_itemnum = -1;
		return 0;
	}

	/* Any other line must begin with a word, which is the attribute. */
	s = txt;
	while (*s==' ')
		s++;
	attribute = s;
	s = strchr(attribute, ' ');
	if (s == NULL) {
		printf("file is corrupt: attribute %s lacks value.\n",
		    attribute);
		return -1;
	}
	*(s++) = '\0';
	while (*s==' ')
		s++;
	/* keep the remaining text for further handling */
	txt = s;

	switch (restore_item) {
	    case RI_NONE:
		/* toplevel attributes */
		return restore_toplevel_attr(attribute, txt);

	    case RI_PLAYER:
		/* player attributes */
		return restore_player_attr(attribute, txt);

	    case RI_DECK:
		/* deck attributes */
		return restore_deck_attr(attribute, txt);

	    case RI_SQUARE:
		/* board square attributes */
		return restore_square_attr(attribute, txt);
	}
	/* NOTREACHED */
	printf("internal logic error\n");
	return -1;
}

static int
restore_toplevel_attr(const char *attribute, char *txt)
{
	if (!strcmp(attribute, "time")) {
		/* nothing */
	} else if (!strcmp(attribute, "numplayers")) {
		if (getnum("numplayers", txt, 2, MAX_PL, &num_play) < 0) {
			return -1;
		}
		if (play != NULL) {
			printf("numplayers: multiple settings\n");
			return -1;
		}
		play = calloc((size_t)num_play, sizeof(play[0]));
		if (play == NULL) {
			err(1, "calloc");
		}
	} else if (!strcmp(attribute, "currentplayer")) {
		if (getnum("currentplayer", txt, 0, num_play-1, &player) < 0) {
			return -1;
		}
		if (play == NULL) {
			printf("currentplayer: before numplayers\n");
			return -1;
		}
		cur_p = &play[player];
	} else if (!strcmp(attribute, "doubles")) {
		if (getnum("doubles", txt, 0, 2, &num_doub) < 0) {
			return -1;
		}
	} else if (!strcmp(attribute, "player")) {
		if (getnum_withbrace("player", txt, 0, num_play-1,
		    &restore_itemnum) < 0) {
			return -1;
		}
		restore_item = RI_PLAYER;
	} else if (!strcmp(attribute, "deck")) {
		if (getnum_withbrace("deck", txt, 0, 1,
		    &restore_itemnum) < 0) {
			return -1;
		}
		restore_item = RI_DECK;
	} else if (!strcmp(attribute, "square")) {
		if (getnum_withbrace("square", txt, 0, N_SQRS-1,
		    &restore_itemnum) < 0) {
			return -1;
		}
		restore_item = RI_SQUARE;
	} else {
		printf("unknown attribute %s\n", attribute);
		return -1;
	}
	return 0;
}

static int
restore_player_attr(const char *attribute, char *txt)
{
	PLAY *pp;
	int tmp;

	if (play == NULL) {
		printf("player came before numplayers.\n");
		return -1;
	}
	pp = &play[restore_itemnum];

	if (!strcmp(attribute, "name")) {
		if (pp->name != NULL) {
			printf("player has multiple names.\n");
			return -1;
		}
		/* XXX should really systematize the max name length */
		if (strlen(txt) > 256) {
			txt[256] = 0;
		}
		pp->name = strdup(txt);
		if (pp->name == NULL)
			err(1, "strdup");
		name_list[restore_itemnum] = pp->name;
	} else if (!strcmp(attribute, "money")) {
		if (getnum(attribute, txt, 0, INT_MAX, &pp->money) < 0) {
			return -1;
		}
	} else if (!strcmp(attribute, "loc")) {
		/* note: not N_SQRS-1 */
		if (getnum(attribute, txt, 0, N_SQRS, &tmp) < 0) {
			return -1;
		}
		pp->loc = tmp;
	} else if (!strcmp(attribute, "num_gojf")) {
		if (getnum(attribute, txt, 0, 2, &tmp) < 0) {
			return -1;
		}
		pp->num_gojf = tmp;
	} else if (!strcmp(attribute, "in_jail")) {
		if (getnum(attribute, txt, 0, 3, &tmp) < 0) {
			return -1;
		}
		pp->in_jail = tmp;
		if (pp->in_jail > 0 && pp->loc != JAIL) {
			printf("player escaped from jail?\n");
			return -1;
		}
	} else {
		printf("unknown attribute %s\n", attribute);
		return -1;
	}
	return 0;
}

static int
restore_deck_attr(const char *attribute, char *txt)
{
	int tmp, j;
	char *s;
	DECK *dp;

	dp = &deck[restore_itemnum];

	if (!strcmp(attribute, "numcards")) {
		if (getnum(attribute, txt, dp->num_cards, dp->num_cards,
		    &tmp) < 0) {
			return -1;
		}
	} else if (!strcmp(attribute, "topcard")) {
		if (getnum(attribute, txt, 0, dp->num_cards,
		    &dp->top_card) < 0) {
			return -1;
		}
	} else if (!strcmp(attribute, "gojf_used")) {
		if (getnum(attribute, txt, 0, 1, &tmp) < 0) {
			return -1;
		}
		dp->gojf_used = tmp;
	} else if (!strcmp(attribute, "cards")) {
		errno = 0;
		s = txt;
		for (j = 0; j<dp->num_cards; j++) {
			tmp = strtol(s, &s, 10);
			if (tmp < 0 || tmp >= dp->num_cards) {
				printf("cards: out of range value\n");
				return -1;
			}
			dp->cards[j] = tmp;
		}
		if (errno) {
			printf("cards: invalid values\n");
			return -1;
		}
	} else {
		printf("unknown attribute %s\n", attribute);
		return -1;
	}
	return 0;
}

static int
restore_square_attr(const char *attribute, char *txt)
{
	SQUARE *sp = &board[restore_itemnum];
	int tmp;

	if (!strcmp(attribute, "owner")) {
		if (getnum(attribute, txt, -1, num_play-1, &tmp) < 0) {
			return -1;
		}
		sp->owner = tmp;
		if (tmp >= 0)
			add_list(tmp, &play[tmp].own_list, restore_itemnum);
	} else if (!strcmp(attribute, "morg")) {
		if (sp->type != PRPTY && sp->type != RR && sp->type != UTIL) {
			printf("unownable property is mortgaged.\n");
			return -1;
		}
		if (getnum(attribute, txt, 0, 1, &tmp) < 0) {
			return -1;
		}
		sp->desc->morg = tmp;
	} else if (!strcmp(attribute, "houses")) {
		if (sp->type != PRPTY) {
			printf("unbuildable property has houses.\n");
			return -1;
		}
		if (getnum(attribute, txt, 0, 5, &tmp) < 0) {
			return -1;
		}
		sp->desc->houses = tmp;
	} else {
		printf("unknown attribute %s\n", attribute);
		return -1;
	}
	return 0;
}

static int
getnum(const char *what, char *txt, int min, int max, int *ret)
{
	char *s;
	long l;

	errno = 0;
	l = strtol(txt, &s, 10);
	if (errno || strlen(s)>0) {
		printf("%s: not a number.\n", what);
		return -1;
	}
	if (l < min || l > max) {
		printf("%s: out of range.\n", what);
	}
	*ret = l;
	return 0;
}

static int
getnum_withbrace(const char *what, char *txt, int min, int max, int *ret)
{
	char *s;
	s = strchr(txt, ' ');
	if (s == NULL) {
		printf("%s: expected open brace\n", what);
		return -1;
	}
	*(s++) = '\0';
	while (*s == ' ')
		s++;
	if (*s != '{') {
		printf("%s: expected open brace\n", what);
		return -1;
	}
	if (s[1] != 0) {
		printf("%s: garbage after open brace\n", what);
		return -1;
	}
	return getnum(what, txt, min, max, ret);
}
