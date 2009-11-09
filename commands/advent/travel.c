/*	module TRAVEL.C						*
 *      Routine to handle motion requests			*/


#include	<stdio.h>
#include	<stdlib.h>
#include	"advent.h"
#include	"advdec.h"
#include	"advcave.h"

struct trav travel[MAXTRAV];
static int kalflg;
static int bcrossing = 0;
static int phuce[2][4] = {158, 160, 167, 166,
			  160, 158, 166, 167};

_PROTOTYPE(static void goback, (void));
_PROTOTYPE(static void ck_kal, (void));
_PROTOTYPE(static void dotrav, (void));
_PROTOTYPE(static void badmove, (void));
_PROTOTYPE(static void spcmove, (int rdest));

void domove()
{
    gettrav(g.loc, travel);
    switch (motion) {
    case NULLX:
	break;
    case BACK:
	goback();
	break;
    case CAVE:
	if (outside(g.loc))
	    rspeak(57);
	else
	    rspeak(58);
	break;
    default:
	g.oldloc2 = g.oldloc;
	g.oldloc = g.loc;
	dotrav();
    }
    newtravel = TRUE;
    return;
}

/*
  Routine to handle request to return
  from whence we came!
*/
static void goback()
{
    int kk, k2, want, temp;
    struct trav strav[MAXTRAV];

    want = forced(g.oldloc) ? g.oldloc2 : g.oldloc;
    g.oldloc2 = g.oldloc;
    g.oldloc = g.loc;
    k2 = 0;
    if (want == g.loc) {
	rspeak(91);
	ck_kal();
	return;
    }
    for (kk = 0; travel[kk].tdest != -1; ++kk) {
	if (!travel[kk].tcond && travel[kk].tdest == want) {
	    motion = travel[kk].tverb;
	    dotrav();
	    return;
	}
	if (!travel[kk].tcond) {
	    temp = travel[kk].tdest;
	    gettrav(temp, strav);
	    if (forced(temp) && strav[0].tdest == want)
		k2 = temp;
	}
    }
    if (k2) {
	motion = travel[k2].tverb;
	dotrav();
    } else
	rspeak(140);
    ck_kal();
    return;
}

static void ck_kal()
{
    if (g.newloc >= 242 && g.newloc <= 247) {
	if (g.newloc == 242)
	    kalflg = 0;
	else if (g.newloc == (g.oldloc + 1))
	    kalflg++;
	else
	    kalflg = -10;
    }
}

/*
  Routine to figure out a new location
  given current location and a motion.
*/
static void dotrav()
{
    unsigned char mvflag, hitflag, kk;
    int rdest, rverb, rcond, robject;
    int pctt;

    g.newloc = g.loc;
    mvflag = hitflag = 0;
    pctt = ranz(100);

    for (kk = 0; travel[kk].tdest >= 0 && !mvflag; ++kk) {
	rdest = travel[kk].tdest;
	rverb = travel[kk].tverb;
	rcond = travel[kk].tcond;
	robject = rcond % 100;

	if ((rverb != 1) && (rverb != motion) && !hitflag)
	    continue;
	++hitflag;
	switch (rcond / 100) {
	case 0:
	    if ((rcond == 0) || (pctt < rcond))
		++mvflag;
	    break;
	case 1:
	    if (robject == 0)
		++mvflag;
	    else if (toting(robject))
		++mvflag;
	    break;
	case 2:
	    if (toting(robject) || at(robject))
		++mvflag;
	    break;
	case 3:
	case 4:
	case 5:
	case 7:
	    if (g.prop[robject] != (rcond / 100) - 3)
		++mvflag;
	    break;
	default:
	    bug(37);
	}
    }
    if (!mvflag)
	badmove();
    else if (rdest > 500)
	rspeak(rdest - 500);
    else if (rdest > 300)
	spcmove(rdest);
    else {
	g.newloc = rdest;
	ck_kal();
    }
    newtravel = TRUE;
    return;
}

/*
  The player tried a poor move option.
*/
static void badmove()
{
    int msg;

    msg = 12;
    if (motion >= 43 && motion <= 50)
	msg = 9;
    if (motion == 29 || motion == 30)
	msg = 9;
    if (motion == 7 || motion == 36 || motion == 37)
	msg = 10;
    if (motion == 11 || motion == 19)
	msg = 11;
    if (motion == 62 || motion == 65 || motion == 82)
	msg = 42;
    if (motion == 17)
	msg = 80;
    rspeak(msg);
    return;
}

/*
  Routine to handle very special movement.
*/
static void spcmove(rdest)
int rdest;
{
    int load, obj, k;

    switch (rdest - 300) {
    case 1:				/* plover movement via alcove */
	load = burden(0);
	if (!load || (load == burden(EMERALD) && holding(EMERALD)))
	    g.newloc = (99 + 100) - g.loc;
	else
	    rspeak(117);
	break;
    case 2:				/* trying to remove plover, bad
					   route */
	if (enclosed(EMERALD))
	    extract(EMERALD);
	drop(EMERALD, g.loc);
	g.newloc = 33;
	break;
    case 3:				/* troll bridge */
	if (g.prop[TROLL] == 1) {
	    pspeak(TROLL, 1);
	    g.prop[TROLL] = 0;
	    move(TROLL2, 0);
	    move((TROLL2 + MAXOBJ), 0);
	    move(TROLL, plac[TROLL]);
	    move((TROLL + MAXOBJ), fixd[TROLL]);
	    juggle(CHASM);
	    g.newloc = g.loc;
	} else {
	    g.newloc = plac[TROLL] + fixd[TROLL] - g.loc;
	    if (g.prop[TROLL] == 0)
		g.prop[TROLL] = 1;
	    if (toting(BEAR)) {
		rspeak(162);
		g.prop[CHASM] = 1;
		g.prop[TROLL] = 2;
		drop(BEAR, g.newloc);
		g.fixed[BEAR] = -1;
		g.prop[BEAR] = 3;
		if (g.prop[SPICES] < 0)
		    ++g.tally2;
		g.oldloc2 = g.newloc;
		death();
	    }
	}
	break;
    case 4:
	/* Growing or shrinking in area of tiny door.  Each time he
	   does this, everything must be moved to the new loc.
	   Presumably, all his possesions are shrunk or streched along
	   with him. Phuce[2][4] is an array containg four pairs of
	   "here" (K) and "there" (KK) locations. */
	k = phuce[0][g.loc - 161];
	g.newloc = phuce[1][g.loc - 161];
	for (obj = 1; obj < MAXOBJ; obj++) {
	    if (obj == BOAT)
		continue;
	    if (g.place[obj] == k && (g.fixed[obj] == 0 || g.fixed[obj] == -1))
		move(obj, g.newloc);
	}
	break;
    case 5:
	/* Phone booth in rotunda. Trying to shove past gnome, to get
	   into phone booth. */
	if ((g.prop[BOOTH] == 0 && pct(35)) || g.visited[g.loc] == 1) {
	    rspeak(263);
	    g.prop[BOOTH] = 1;
	    move(GNOME, 188);
	} else {
	    if (g.prop[BOOTH] == 1)
		rspeak(253);
	    else
		g.newloc = 189;
	}
	break;
    case 6:
	/* Collapsing clay bridge.  He can cross with three (or fewer)
	   thing.  If more, of if carrying obviously heavy things, he
	   may end up in the drink. */
	g.newloc = g.loc == 235 ? 190 : 235;
	bcrossing++;
	load = burden(0);
	if (load > 4) {
	    k = (load + bcrossing) * 6 - 10;
	    if (!pct(k))
		rspeak(318);
	    else {
		rspeak(319);
		g.newloc = 236;
		if (holding(LAMP))
		    move(LAMP, 236);
		if (toting(AXE) && enclosed(AXE))
		    extract(AXE);
		if (holding(AXE))
		    move(AXE, 208);
		for (obj = 1; obj < MAXOBJ; obj++)
		    if (toting(obj))
			destroy(obj);
		g.prop[CHASM2] = 1;
	    }
	}
	break;
    case 7:
	/* Kaleidoscope code is here. */
	if (kalflg == 5) {
	    g.newloc = 248;
	    g.oldloc = 247;
	} else {
	    g.newloc = 242 + ranz(5);
	    g.oldloc = g.newloc - 1;
	    kalflg = g.newloc == 242 ? 0 : -10;
	}
	break;
    default:
	bug(38);
    }
    return;
}

/*
  Routine to fill travel array for a given location
*/
void gettrav(loc, travel)
int loc;
struct trav *travel;
{
    int i;
    long t, *lptr;

    lptr = cave[loc - 1];
    for (i = 0; i < MAXTRAV; i++) {
	t = *lptr++;
	if (!(t)) {
	    travel->tdest = -1;		/* end of array	 */
	    return;			/* terminate for loop	 */
	}
	travel->tverb = (int) (t % 1000);
	t /= 1000;
	travel->tdest = (int) (t % 1000);
	t /= 1000;
	travel->tcond = (int) (t % 1000);
	travel++;
    }
    bug(25);
    return;
}
