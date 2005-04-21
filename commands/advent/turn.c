/*	program TURN.C						*/


#include	<stdio.h>
#include	<stdlib.h>
#include	"advent.h"
#include	"advdec.h"

_PROTOTYPE(void descitem, (void));
_PROTOTYPE(void domove, (void));
_PROTOTYPE(void goback, (void));
_PROTOTYPE(void copytrv, (struct trav *, struct trav *));
_PROTOTYPE(void dotrav, (void));
_PROTOTYPE(void badmove, (void));
_PROTOTYPE(void spcmove, (int));
_PROTOTYPE(void death, (void));
_PROTOTYPE(void dwarves, (void));
_PROTOTYPE(void dopirate, (void));
_PROTOTYPE(int stimer, (void));
_PROTOTYPE(void do_hint, (int));


/*
  Routine to take 1 turn
*/
void turn()
{
    int i, hint;
    static int waste = 0;

    if (newtravel) {
	/* If closing, then he can't leave except via the main office. */
	if (outside(g.newloc) && g.newloc != 0 && g.closing) {
	    rspeak(130);
	    g.newloc = g.loc;
	    if (!g.panic)
		g.clock2 = 15;
	    g.panic = TRUE;
	}
	/* See if a dwarf has seen him and has come from where he wants
	   to go. */
	if (g.newloc != g.loc && !forced(g.loc) && g.loc_attrib[g.loc] & NOPIRAT == 0)
	    for (i = 1; i < (DWARFMAX - 1); ++i)
		if (g.odloc[i] == g.newloc && g.dseen[i]) {
		    g.newloc = g.loc;
		    rspeak(2);
		    break;
		}

	g.loc = g.newloc;
	dwarves();			/* & special dwarf(pirate who
					   steals)	 */

	/* Check for death */
	if (g.loc == 0) {
	    death();
	    return;
	}
	/* Check for forced move */
	if (forced(g.loc)) {
	    desclg(g.loc);
	    ++g.visited[g.loc];
	    domove();
	    return;
	}
	/* Check for wandering in dark */
	if (g.wzdark && dark() && pct(35)) {
	    rspeak(23);
	    g.oldloc2 = g.loc;
	    death();
	    return;
	}
	/* see if he is wasting his batteies out in the open */
	if (outside(g.loc) && g.prop[LAMP]) {
	    waste++;
	    if (waste > 11) {
		rspeak(324);
		waste = 0;
	    }
	} else
	    waste = 0;

	/* If wumpus is chasing stooge, see if wumpus gets him */
	if (g.chase) {
	    g.chase++;
	    g.prop[WUMPUS] = g.chase / 2;
	    move(WUMPUS, g.loc);
	    if (g.chase >= 10) {
		if (dark())
		    rspeak(270);
		pspeak(WUMPUS, 5);
		death();
		return;
	    }
	}
	/* check for radiation poisoning. */
	g.health += (outside(g.loc)) ? 3 : 1;
	if (g.health > 100)
	    g.health = 100;
	if (here(RADIUM) && (g.place[RADIUM] != -SHIELD || ajar(SHIELD)))
	    g.health -= 7;
	if (g.health < 60) {
	    rspeak(391 + (60 - g.health) / 10);
	    if (g.health < 0) {
		death();
		return;
	    }
	}
	if ((g.oldloc == 188) && (g.loc != 188 && g.loc != 189)
	    && (g.prop[BOOTH] == 1)) {
	    move(GNOME, 0);
	    g.prop[BOOTH] = 0;
	}
	/* Describe his situation */
	describe();
	if (!blind()) {
	    ++g.visited[g.loc];
	    descitem();
	}
    }					/* end of newtravel start for
					   second entry point */
    /* Check if this location is eligible for any hints.  If been here
       long enough, branch to help section. Ignore "hints" < HNTMIN
       (special stuff, see database notes. */
    for (hint = HNTMIN; hint <= HNTMAX; hint++) {
	if (g.hinted[hint])
	    continue;
	if (g.loc_attrib[g.loc] / 256 != hint - 6)
	    g.hintlc[hint] = -1;
	g.hintlc[hint]++;
	if (g.hintlc[hint] >= g.hints[hint][1])
	    do_hint(hint);
    }

    if (g.closed) {
	if (g.prop[OYSTER] < 0 && toting(OYSTER))
	    pspeak(OYSTER, 1);
	for (i = 1; i < MAXOBJ; ++i)
	    if (toting(i) && g.prop[i] < 0)
		g.prop[i] = -1 - g.prop[i];
    }
    g.wzdark = dark();
    if (g.knfloc > 0 && g.knfloc != g.loc)
	g.knfloc = 0;
    ++g.turns;
    i = rand();

    if (stimer())			/* as the grains of sand slip
					   by */
	return;

    while (!english())			/* retrieve player instructions	 */
	;

    vrbx = 1;
    objx = objs[1] ? 1 : 0;
    iobx = iobjs[1] ? 1 : 0;
    verb = VAL(verbs[vrbx]);
    do {
	object = objx ? objs[objx] : 0;
	iobj = iobx ? iobjs[iobx] : 0;
	if (object && (objs[2] || iobjs[2])) {
	    pspeak(object, -1);
	    printf("      ");
	}
	switch (CLASS(verbs[vrbx])) {
	case MOTION:
	    motion = verb;
	    domove();
	    break;
	case NOUN:
	    bug(22);
	case ACTION:
	    if (object || iobj)
		trverb();
	    else
		itverb();
	    break;
	case MISC:
	    rspeak(verb);
	    if (verb == 51)
		g.hinted[1] = TRUE;
	    break;
	default:
	    bug(22);
	}
	if (objx) {
	    objx++;
	    if (objs[objx] == 0)
		objx = 0;
	}
	if ((!objx || !objs[objx]) && iobx) {
	    iobx++;
	    if (iobjs[iobx] == 0)
		iobx = 0;
	    if (iobx && iobjs[1])
		objx = 1;
	}
    } while (objx || iobx);
    return;
}

/*
  Routine to describe current location
*/
void describe()
{
    if (toting(BEAR))
	rspeak(141);
    if (dark())
	rspeak(16);
    else if ((g.terse && verb != LOOK) || g.visited[g.loc] % g.abbnum)
	descsh(g.loc);
    else
	desclg(g.loc);
    if (g.loc == 33 && pct(25) && !g.closing)
	rspeak(8);
    if (g.loc == 147 && !g.visited[g.loc])
	rspeak(216);
    return;
}

/*
  Routine to describe visible items
*/
void descitem()
{
    int i, state;

    for (i = 1; i < MAXOBJ; ++i) {
	if (at(i)) {
	    if (i == STEPS && toting(NUGGET))
		continue;
	    if (g.prop[i] < 0) {
		if (g.closed)
		    continue;
		else {
		    g.prop[i] = 0;
		    if (i == RUG || i == CHAIN
			|| i == SWORD || i == CASK)
			g.prop[i] = 1;
		    if (i == CLOAK || i == RING)
			g.prop[i] = 2;
		    --g.tally;
		}
	    }
	    if (i == STEPS && g.loc == g.fixed[STEPS])
		state = 1;
	    else
		state = g.prop[i] % 8;
	    pspeak(i, state);
	    lookin(i);
	}
    }
    /* If remaining treasures too elusive, zap his lamp */
    if (g.tally == g.tally2 && g.tally != 0 && g.limit > 35)
	g.limit = 35;
    return;
}

/*
  Routine to handle player's demise via
  waking up the dwarves...
*/
void dwarfend()
{
    rspeak(136);
    normend();
    return;
}

/*
  normal end of game
*/
void normend()
{
    score(FALSE);
    gaveup = TRUE;
    return;
}

/*
  Routine to handle the passing on of one
  of the player's incarnations...
*/
void death()
{
    int yea, j;

    if (!g.closing) {
	if (g.limit < 0) {
	    rspeak(185);
	    normend();
	    return;
	}
	yea = yes(81 + g.numdie * 2, 82 + g.numdie * 2, 54);
	if (++g.numdie >= MAXDIE || !yea)
	    normend();
	if (g.chase) {
	    g.chase = FALSE;
	    g.prop[WUMPUS] = 0;
	    move(WUMPUS, 174);
	}
	if (toting(LAMP))
	    g.prop[LAMP] = 0;
	for (j = 1; j < MAXOBJ; ++j) {
	    if (toting(j))
		drop(j, j == LAMP ? 1 : g.oldloc2);
	    if (wearng(j)) {
		g.prop[j] = 0;
		bitoff(j, WEARBT);
	    }
	}
	g.newloc = 3;
	g.oldloc = g.loc;
	g.health = 100;
	return;
    }
    /* Closing -- no resurrection... */
    rspeak(131);
    ++g.numdie;
    normend();
    return;
}

/*
  dwarf stuff.
*/
void dwarves()
{
    int i, j, k, attack, stick, dtotal;

    /* See if dwarves allowed here */
    if (g.newloc == 0 || forced(g.newloc) || g.loc_attrib[g.newloc] & NOPIRAT)
	return;

    /* See if dwarves are active. */
    if (!g.dflag) {
	if (inside(g.newloc))
	    ++g.dflag;
	return;
    }
    /* If first close encounter (of 3rd kind) */
    if (g.dflag == 1) {
	if (!inside(g.newloc) || pct(85))
	    return;
	++g.dflag;

	/* kill 0, 1 or 2 of the dwarfs */
	for (i = 1; i < 3; ++i)
	    if (pct(50))
		g.dloc[(ranz(DWARFMAX - 1)) + 1] = 0;

	/* If any of the survivors is at location, use alternate choise */
	for (i = 1; i <= DWARFMAX; ++i) {
	    if (g.dloc[i] == g.newloc)
		g.dloc[i] = g.daltloc;
	    g.odloc[i] = g.dloc[i];
	}
	rspeak(3);
	drop(AXE, g.newloc);
	return;
    }
    /* Things are in full swing.  Move each dwarf at random, except if
       he's seen us then he sticks with us.  Dwarfs never go to
       locations outside or meet the bear or following him into dead
       end in maze.  And of couse, dead dwarves don't do much of
       anything.  */

    dtotal = attack = stick = 0;
    for (i = 1; i <= DWARFMAX; ++i) {
	if (g.dloc[i] == 0)
	    continue;
	/* Move a dwarf at random.  we don't have a matrix around to do
	   it as in the original version... */
	do
	    j = ranz(106) + 15;
	/* allowed area */
	while (j == g.odloc[i] || j == g.dloc[i]
	       || g.loc_attrib[j] & NOPIRAT);

	if (j == 0)
	    bug(36);
	g.odloc[i] = g.dloc[i];
	g.dloc[i] = j;

	g.dseen[i] = ((g.dseen[i] && inside(g.newloc))
		      || g.dloc[i] == g.newloc
		      || g.odloc[i] == g.newloc);

	if (g.dseen[i]) {
	    g.dloc[i] = g.newloc;
	    if (i == DWARFMAX)
		dopirate();
	    else {
		++dtotal;
		if (g.odloc[i] == g.dloc[i]) {
		    ++attack;
		    if (g.knfloc >= 0)
			g.knfloc = g.newloc;
		    if (ranz(1000) < (45 * (g.dflag - 2)))
			++stick;
		}
	    }
	}
    }

    /* Now we know shat's happing, let's tell the poor sucker about it */
    if (dtotal == 0)
	return;
    if (dtotal > 1)
	printf("There are %d threatening little dwarves in the room with you!\n", dtotal);
    else
	rspeak(4);
    if (attack == 0)
	return;
    if (g.dflag == 2)
	++g.dflag;
    if (attack > 1) {
	printf("%d of them throw knives at you!!\n", attack);
	k = 6;
    } else {
	rspeak(5);
	k = 52;
    }
    if (stick <= 1) {
	rspeak(stick + k);
	if (stick == 0)
	    return;
    } else
	printf("%d of them get you !!!\n", stick);
    g.oldloc2 = g.newloc;
    death();
    return;
}

/*
  pirate stuff
*/
void dopirate()
{
    int j;
    boolean k;

    if (g.newloc == g.chloc || g.prop[CHEST] >= 0)
	return;
    k = FALSE;
    /* Pirate won't take pyramid from plover room or dark room  (too
       easy! ) */
    for (j = 1; j < MAXOBJ; ++j)
	if (treasr(j) && !(j == CASK && liq(CASK) == WINE)
	    && !(j == PYRAMID && (g.newloc == g.place[PYRAMID]
				  || g.newloc == g.place[EMERALD]))) {
	    if (toting(j) && athand(j))
		goto stealit;
	    if (here(j))
		k = TRUE;
	}
    if (g.tally == g.tally2 + 1 && k == FALSE && g.place[CHEST] == 0 &&
	athand(LAMP) && g.prop[LAMP] == 1) {
	rspeak(186);
	move(CHEST, g.chloc);
	move(MESSAGE, g.chloc2);
	g.dloc[DWARFMAX] = g.chloc;
	g.odloc[DWARFMAX] = g.chloc;
	g.dseen[DWARFMAX] = 0;
	return;
    }
    if (g.odloc[DWARFMAX] != g.dloc[DWARFMAX] && pct(30))
	rspeak(127);
    return;

stealit:

    rspeak(128);
    /* don't steal chest back from troll! */
    if (g.place[MESSAGE] == 0)
	move(CHEST, g.chloc);
    move(MESSAGE, g.chloc2);
    for (j = 1; j < MAXOBJ; ++j) {
	if (!treasr(j) || !athand(j)
	    || (j == PYRAMID &&
	     (g.newloc == plac[PYRAMID] || g.newloc == plac[EMERALD]))
	    || (j == CASK && (liq(CASK) != WINE)))
	    continue;
	if (enclosed(j))
	    extract(j);
	if (wearng(j)) {
	    g.prop[j] = 0;
	    bitoff(j, WEARBT);
	}
	insert(j, CHEST);
    }
    g.dloc[DWARFMAX] = g.chloc;
    g.odloc[DWARFMAX] = g.chloc;
    g.dseen[DWARFMAX] = FALSE;
    return;
}

/*
  special time limit stuff...
*/
int stimer()
{
    int i, spk;
    static int clock3;

    g.foobar = g.foobar > 0 ? -g.foobar : 0;
    g.combo = g.combo > 0 ? -g.combo : 0;
    if (g.turns > 310 && g.abbnum != 10000 && !g.terse)
	rspeak(273);

    /* Bump all the right clocks for reconning battery life and closing */
    if (g.closed) {
	clock3--;
	if (clock3 == 0) {
	    g.prop[PHONE] = 0;
	    g.prop[BOOTH] = 0;
	    rspeak(284);
	} else if (clock3 < -7) {
	    rspeak(254);
	    normend();
	    return (TRUE);
	}
    }
    if (g.tally == 0 && inside(g.loc) && g.loc != Y2)
	--g.clock;
    if (g.clock == 0) {
	/* Start closing the cave */
	g.prop[GRATE] = 0;
	biton(GRATE, LOCKBT);
	bitoff(GRATE, OPENBT);
	g.prop[FISSURE] = 0;
	g.prop[TDOOR] = 0;
	biton(TDOOR, LOCKBT);
	bitoff(TDOOR, OPENBT);
	g.prop[TDOOR2] = 0;
	biton(TDOOR2, LOCKBT);
	bitoff(TDOOR2, OPENBT);
	for (i = 1; i <= DWARFMAX; ++i) {
	    g.dseen[i] = FALSE;
	    g.dloc[i] = 0;
	}
	move(TROLL, 0);
	move((TROLL + MAXOBJ), 0);
	move(TROLL2, plac[TROLL]);
	move((TROLL2 + MAXOBJ), fixd[TROLL]);
	juggle(CHASM);
	if (g.prop[BEAR] != 3)
	    destroy(BEAR);
	g.prop[CHAIN] = 0;
	g.fixed[CHAIN] = 0;
	g.prop[AXE] = 0;
	g.fixed[AXE] = 0;
	rspeak(129);
	g.clock = -1;
	g.closing = TRUE;
	return (FALSE);
    }
    if (g.clock < 0)
	--g.clock2;
    if (g.clock2 == 0) {
	/* Set up storage room... and close the cave... */
	g.prop[BOTTLE] = put(BOTTLE, 115, 8);
	g.holder[BOTTLE] = WATER;
	g.place[WATER] = -BOTTLE;
	g.hlink[WATER] = 0;
	bitoff(BOTTLE, OPENBT);
	g.prop[PLANT] = put(PLANT, 115, 0);
	g.prop[OYSTER] = put(OYSTER, 115, 0);
	g.prop[LAMP] = put(LAMP, 115, 0);
	g.prop[ROD] = put(ROD, 115, 0);
	g.prop[DWARF] = put(DWARF, 115, 0);
	g.loc = 115;
	g.oldloc = 115;
	g.newloc = 115;
	/* Leave the grate with normal (non-negative property). */
	put(GRATE, 116, 0);
	biton(GRATE, LOCKBT);
	bitoff(GRATE, OPENBT);
	g.prop[SNAKE] = put(SNAKE, 116, 1);
	g.prop[BIRD] = put(BIRD, 116, 1);
	g.prop[CAGE] = put(CAGE, 116, 0);
	g.prop[ROD2] = put(ROD2, 116, 0);
	g.prop[PILLOW] = put(PILLOW, 116, 0);

	g.prop[BOOTH] = put(BOOTH, 116, -3);
	g.fixed[BOOTH] = 115;
	g.prop[PHONE] = put(PHONE, 212, -4);

	g.prop[MIRROR] = put(MIRROR, 115, 0);
	g.fixed[MIRROR] = 116;
	g.prop[BOOK2] = put(BOOK2, 115, 0);

	for (i = 1; i < MAXOBJ; ++i) {
	    if (toting(i) && enclosed(i))
		extract(i);
	    if (toting(i))
		destroy(i);
	}
	rspeak(132);
	g.closed = TRUE;
	clock3 = 20 + ranz(20);
	newtravel = TRUE;
	return (TRUE);
    }
    if (g.prop[LAMP] == 1)
	--g.limit;
    if (g.limit == 0) {
	--g.limit;
	g.prop[LAMP] = 0;
	if (here(LAMP))
	    rspeak(184);
	return (FALSE);
    }
    if (g.limit < 0 && outside(g.loc)) {
	rspeak(185);
	normend();
	return (TRUE);
    }
    if (g.limit <= 40) {
	if (g.lmwarn || !here(LAMP))
	    return (FALSE);
	g.lmwarn = TRUE;
	spk = 187;
	if (g.prop[BATTERIES] == 1)
	    spk = 323;
	if (g.place[BATTERIES] == 0)
	    spk = 183;
	if (g.prop[VEND] == 1)
	    spk = 189;
	rspeak(spk);
	return (FALSE);
    }
    return (FALSE);
}

/* HINTS
   come here if he's been long enough at required location(s)
   for some unused hint,  hint number is in variable "hint".
   Branch to quick test for additional conditions, then
   do neet stuff. If conditions are met and we want to offer
   hint.  Clear hintlc if no action is taken.
 */

#define MASE 1
#define DARK 2
#define WITT 3
#define H_SWORD 4
#define SLIDE 5
#define H_GRATE 6
#define H_BIRD 7
#define H_ELFIN 8
#define RNBOW 9
#define STYX  10
#define H_SNAKE 11
#define CASTLE 12

void do_hint(hint)
int hint;
{
    g.hintlc[hint] = 0;
    switch (hint + 1 - HNTMIN) {
    case MASE:
	if (!at(g.loc) && !at(g.oldloc)
	    && !at(g.loc) && burden(0) > 1)
	    break;
	else
	    return;
    case DARK:
	if (g.prop[EMERALD] != -1 && g.prop[PYRAMID] == -1)
	    break;
	else
	    return;
    case WITT:
	break;
    case H_SWORD:
	if ((g.prop[SWORD] == 1 || g.prop[SWORD] == 5)
	    && !toting(CROWN))
	    break;
	else
	    return;
    case SLIDE:
	break;
    case H_GRATE:
	if (g.prop[GRATE] == 0 && !athand(KEYS))
	    break;
	else
	    return;
    case H_BIRD:
	if (here(BIRD) && athand(ROD) && object == BIRD)
	    break;
	else
	    return;
    case H_ELFIN:
	if (!g.visited[159])
	    break;
	else
	    return;
    case RNBOW:
	if (!toting(SHOES) || g.visited[205])
	    break;
	else
	    return;
    case STYX:
	if (!athand(LYRE) && g.prop[DOG] != 1)
	    break;
	else
	    return;
    case H_SNAKE:
	if (here(SNAKE) && !here(BIRD))
	    break;
	else
	    return;
    case CASTLE:
	break;
    default:
	printf("  TRYING TO PRINT HINT # %d\n", hint);
	bug(27);
    }
    if (!yes(g.hints[hint][3], 0, 54))
	return;
    printf("\nI am prepared to give you a hint,");
    printf(" but it will cost you %2d points\n", g.hints[hint][2]);
    g.hinted[hint] = yes(175, g.hints[hint][4], 54);
    if (g.hinted[hint] && g.limit > 30)
	g.limit += 30 * g.hints[hint][2];
    return;
}
