/*	program VERB.C						*/

#include	"stdio.h"
#include	"advent.h"
#include	"advdec.h"

 /* Initialize default verb messages */
static _CONST int actmsg[56] = {
     0,  24,  29,  0,  33,   0,  33,  38,  38,  42,
    14,  43, 110, 29, 110,  73,  75,  29,  13,  59,
    59, 174, 313, 67,  13, 147, 155, 369, 146, 110,
    13,  13,  24, 25, 110, 262,  14,  29, 271,  14,
    14,  24,  29, 38,  24, 331,  24, 109, 332,   0,
     0, 348, 358,  0, 364,   0};

_PROTOTYPE(static int ck_obj, (void));
_PROTOTYPE(void von, (void));
_PROTOTYPE(void voff, (void));
_PROTOTYPE(void vwave, (void));
_PROTOTYPE(void veat, (void));
_PROTOTYPE(void vthrow, (void));
_PROTOTYPE(void vfind, (void));
_PROTOTYPE(void vfill, (void));
_PROTOTYPE(void vfeed, (void));
_PROTOTYPE(void vbreak, (void));
_PROTOTYPE(void vwake, (void));
_PROTOTYPE(void vdrop, (void));
_PROTOTYPE(void vpour, (void));
_PROTOTYPE(void vput, (void));
_PROTOTYPE(void vread, (void));
_PROTOTYPE(void vinsert, (void));
_PROTOTYPE(void vextract, (void));
_PROTOTYPE(static boolean do_battle, (int *));
_PROTOTYPE(void vhit, (void));
_PROTOTYPE(void vanswer, (void));
_PROTOTYPE(void vblow, (void));
_PROTOTYPE(void vdial, (void));
_PROTOTYPE(void vplay, (void));
_PROTOTYPE(void vpick, (void));
_PROTOTYPE(void vput, (void));
_PROTOTYPE(void vturn, (void));
_PROTOTYPE(void vget, (void));
_PROTOTYPE(void vlook, (void));


/*
  Routine to process a transitive verb
*/
void trverb()
{
    newtravel = FALSE;
    switch (verb) {
    case NOTHING:
    case CALM:
    case WALK:
    case QUIT:
    case SCORE:
    case FOO:
    case SUSPEND:			break;
    case TAKE:		vtake();	break;
    case DROP:		vdrop();	break;
    case SAY:		bug(34);	break;
    case OPEN:		vopen();	break;
    case CLOSE:		vclose();	break;
    case LOCK:		vlock();	break;
    case UNLOCK:	vunlock();	break;
    case ON:		von();		break;
    case OFF:		voff();		break;
    case WAVE:		vwave();	break;
    case KILL:		vkill();	break;
    case POUR:		vpour();	break;
    case EAT:		veat();		break;
    case DRINK:		vdrink();	break;
    case RUB:
	if (object != LAMP)
	    rspeak(76);
	else
	    actspk(RUB);
	break;
    case THROW:
	if (prep == PREPDN)
	    vput();
	else
	    vthrow();
	break;
    case FEED:		vfeed();	break;
    case FIND:
    case INVENTORY:	vfind();	break;
    case FILL:		vfill();	break;
    case BLAST:		ivblast();	break;
    case READ:		vread();	break;
    case BREAK:		vbreak();	break;
    case WAKE:		vwake();	break;
    case REMOVE:	vextract();	break;
    case YANK:		vyank();	break;
    case WEAR:		vwear();	break;
    case HIT:		vhit();		break;
    case ANSWER:	vanswer();	break;
    case BLOW:		vblow();	break;
    case DIAL:		vdial();	break;
    case PLAY:		vplay();	break;
    case PICK:		vpick();	break;
    case PUT:		vput();		break;
    case TURN:		vturn();	break;
    case GET:		vget();		break;
    case INSRT:		vinsert();	break;
    case LOOK:		vlook();	break;
    default:
	printf("This verb is not implemented yet.\n");
    }
    return;
}

/*
  Routine to speak default verb message
*/
void actspk(verb)
int verb;
{
    int i;

    if (verb < 1 || verb > 55)
	bug(39);
    i = actmsg[verb];
    if (i)
	rspeak(i);
    return;
}

/*
  CARRY TAKE etc.
*/
void vtake()
{
    int msg;

    msg = 0;
    if (object == BIRD && !g.closed && athand(BIRD)
	&& g.place[BIRD] != g.loc) {
	rspeak(407);
	return;
    }
    if (prep == PREPOF) {
	if (object && iobj) {
	    rspeak(confuz());
	    return;
	} else if (!object) {
	    object = iobj;
	    iobj = 0;
	    vdrop();
	    return;
	}
    }
    msg = 24;
    if (object == BOAT)
	msg = 281;
    if (plural(object))
	msg = 297;
    if (holding(object)) {
	rspeak(msg);
	return;
    }
    /* Special case objects and fixed objects */
    msg = ck_obj();
    if (g.fixed[object]) {
	rspeak(msg);
	return;
    }
    if (prep == PREPIN) {
	vinsert();
	return;
    }
    /* Special case for liquids */
    if (object == WATER || object == OIL || object == WINE) {
	if (here(BOTTLE) && here(CASK)) {
	    rspeak(315);
	    return;
	}
	iobj = object;
	if (here(BOTTLE)) {
	    object = BOTTLE;
	    if (holding(BOTTLE))
		vfill();
	    else
		rspeak(312);
	    return;
	} else if (here(CASK)) {
	    object = CASK;
	    if (holding(CASK))
		vfill();
	    else
		rspeak(312);
	    return;
	} else {
	    rspeak(312);
	    return;
	}
    }
    if (object != BEAR && ((burden(0) + burden(object)) > 15)) {
	if (wearng(object)) {
	    g.prop[object] = 0;
	    bitoff(object, WEARBT);
	}
	rspeak(92);
	return;
    }
    if (prep == PREPFR || enclosed(object)) {
	vextract();
	return;
    }
    msg = 343;
    /* Poster: hides wall safe */
    if (object == POSTER && g.place[SAFE] == 0) {
	g.prop[POSTER] = 1;
	msg = 362;
	/* Move safe and wall containing safe into view */
	drop(SAFE, g.loc);
	drop(WALL2, g.loc);
    }
    /* Boat: need the pole to push it */
    if (object == BOAT) {
	if (!toting(POLE) && g.place[POLE] != -BOAT) {
	    rspeak(218);
	    return;
	} else {
	    g.prop[BOAT] = 1;
	    msg = 221;
	}
    }
    /* Special case for bird. */
    if (object == BIRD && g.prop[BIRD] <= 0) {
	if (athand(ROD)) {
	    rspeak(26);
	    return;
	}
	if (!holding(CAGE)) {
	    rspeak(27);
	    return;
	}
	if (!ajar(CAGE)) {
	    rspeak(358);
	    return;
	}
	insert(BIRD, CAGE);
	bitoff(CAGE, OPENBT);
	pspeak(object, -1);
	rspeak(54);
	return;
    }
    /* SWORD If in anvil, need crown & must yank */
    if (object == SWORD && g.prop[SWORD] != 0) {
	if (iobj && iobj != ANVIL) {
	    rspeak(noway());
	    return;
	}
	if (verb != YANK)
	    if (!yes(215, 0, 54))
		return;

	if (!wearng(CROWN)) {
	    g.fixed[SWORD] = -1;
	    g.prop[SWORD] = 3;
	    pspeak(SWORD, 2);
	    return;
	}
    }
    carry(object, g.loc);
    if (object == POLE || object == SKEY || object == SWORD
	 || ((object == CLOAK || object == RING) && !wearng(object)) )
	g.prop[object] = 0;

    if (verb == YANK || object == SWORD)
	msg = 204;
    rspeak(msg);
    return;
}

static int ck_obj()
{
    int msg;

    msg = noway();
    if (object == PLANT && g.prop[PLANT] <= 0)
	msg = 115;
    if (object == BEAR && g.prop[BEAR] == 1)
	msg = 169;
    if (object == CHAIN && g.prop[BEAR] != 0)
	msg = 170;
    if (object == SWORD && g.prop[SWORD] == 5)
	msg = 208;
    if (object == CLOAK && g.prop[CLOAK] == 2)
	msg = 242;
    if (object == AXE && g.prop[AXE] == 2)
	msg = 246;
    if (object == PHONE)
	msg = 251;
    if (object == BEES || object == HIVE)
	msg = 295;
    if (object == STICKS)
	msg = 296;
    return (msg);
}

/*
  DROP etc.
*/
void vdrop()
{
    int msg;

    /* Check for dynamite */
    if (holding(ROD2) && object == ROD && !holding(ROD))
	object = ROD2;
    if (plural(object))
	msg = 105;
    else
	msg = 29;

    if (object == liq(BOTTLE))
	object = BOTTLE;
    else if (object == liq(CASK))
	object = CASK;

    if (!toting(object)) {
	rspeak(msg);
	return;
    }
    if (prep == PREPIN) {
	vinsert();
	return;
    }
    /* Snake and bird */
    if (object == BIRD && here(SNAKE)) {
	rspeak(30);
	if (g.closed) {
	    dwarfend();
	    return;
	}
	extract(BIRD);
	destroy(SNAKE);
	/* Set snake prop for use by travel options */
	g.prop[SNAKE] = 1;
	drop(BIRD, g.loc);
	return;
    }
    msg = 344;
    if (verb == LEAVE)
	msg = 353;
    if (verb == THROW)
	msg = 352;
    if (verb == TAKE)
	msg = 54;
    if (object == POLE && holding(BOAT)) {
	rspeak(280);
	return;
    }
    /* Coins and vending machine */
    if (object == COINS && here(VEND)) {
	destroy(COINS);
	drop(BATTERIES, g.loc);
	pspeak(BATTERIES, 0);
	return;
    }
    /* Bird and dragon (ouch!!) */
    if (object == BIRD && at(DRAGON) && g.prop[DRAGON] == 0) {
	rspeak(154);
	extract(BIRD);
	destroy(BIRD);
	if (g.place[SNAKE] == plac[SNAKE])
	    g.tally2++;
	return;
    }
    /* Bear and troll */
    if (object == BEAR && at(TROLL)) {
	msg = 163;
	destroy(TROLL);
	destroy(TROLL + MAXOBJ);
	move(TROLL2, plac[TROLL]);
	move((TROLL2 + MAXOBJ), fixd[TROLL]);
	juggle(CHASM);
	g.prop[TROLL] = 2;
    }
    /* Vase */
    else if (object == VASE) {
	if (g.loc == plac[PILLOW])
	    msg = 54;
	else {
	    g.prop[VASE] = at(PILLOW) ? 0 : 2;
	    pspeak(VASE, g.prop[VASE] + 1);
	    if (g.prop[VASE] != 0)
		g.fixed[VASE] = -1;
	}
    } else {
	if (worn(object) || object == POLE || object == BOAT)
	    g.prop[object] = 0;
	if (worn(object))
	    bitoff(object, WEARBT);
	if (object == POLE)
	    g.prop[BOAT] = 0;
    }

    if (enclosed(object))
	extract(object);
    drop(object, g.loc);
    rspeak(msg);
    return;
}

/*
  OPEN. special stuff for opening clam/oyster.
  The following can be opened without a key:
  clam/oyster, door, pdoor, bottle, cask, cage
*/
void vopen()
{
    int msg, oyclam;

    if (!hinged(object))
	msg = noway();
    else if (object == PDOOR && g.prop[PDOOR] == 1)
	msg = 253;
    else if (ajar(object))
	msg = 336;
    else if (locks(object) || iobj == KEYS || iobj == SKEY) {
	vunlock();
	return;
    } else if (locked(object))
	if (object == DOOR)
	    msg = 111;
	else
	    msg = 337;
    else if (object == CLAM || object == OYSTER) {
	oyclam = (object == OYSTER ? 1 : 0);
	msg = oyclam + holding(object) ? 120 : 124;
	if (!athand(TRIDENT))
	    msg = 122 + oyclam;
	if (iobj != 0 && iobj != TRIDENT)
	    msg = 376 + oyclam;

	if (msg == 124) {
	    destroy(CLAM);
	    drop(OYSTER, g.loc);
	    drop(PEARL, 105);
	}
    } else {
	msg = 54;
	biton(object, OPENBT);
    }
    rspeak(msg);
    return;
}

/*
   close, shut
   the following can be closed without keys:
   door, pdoor, bottle, cask, cage
*/
void vclose()
{
    if (!hinged(object))
	rspeak(noway());
    else if (!ajar(object))
	rspeak(338);
    else if (locks(object))
	vlock();
    else {
	rspeak(54);
	bitoff(object, OPENBT);
    }
}

/*
  Lamp ON.
*/
void von()
{
    if (!athand(LAMP))
	actspk(verb);
    else if (g.limit < 0)
	rspeak(184);
    else if (g.prop[LAMP] == 1)
	rspeak(321);
    else {
	g.prop[LAMP] = 1;
	if (g.loc == 200)
	    rspeak(108);
	else
	    rspeak(39);
	if (g.wzdark) {
	    g.wzdark = 0;
	    describe();
	    descitem();
	}
    }
    return;
}

/*
  Lamp OFF.
*/
void voff()
{
    if (!athand(LAMP))
	actspk(verb);
    else if (g.prop[LAMP] == 0)
	rspeak(322);
    else {
	g.prop[LAMP] = 0;
	rspeak(40);
	if (dark())
	    rspeak(16);
    }
    return;
}

/*
  WAVE. no effect unless waving rod at fissure.
*/
void vwave()
{
    if (!holding(object) &&
	(object != ROD || !holding(ROD2)))
	rspeak(29);
    else if (object != ROD || !at(FISSURE) ||
	     !holding(object) || g.closing)
	actspk(verb);
    else if (iobj != 0 && iobj != FISSURE)
	actspk(verb);
    else {
	g.prop[FISSURE] = 1 - g.prop[FISSURE];
	pspeak(FISSURE, 2 - g.prop[FISSURE]);
	if (g.chase == 0 || g.prop[FISSURE] != 0)
	    return;
	if ((g.loc == 17 && g.oldloc != 27)
	    || (g.loc == 27 && g.oldloc != 17))
	    return;
	/* Demise of the Wumpus.  Champ must have just crossed bridge */
	rspeak(244);
	g.chase = 0;
	drop(RING, 209);
	g.prop[WUMPUS] = 6;
	move(WUMPUS, 209);
	biton(WUMPUS, DEADBT);
	if (g.place[AXE] != plac[WUMPUS])
	    return;
	g.fixed[AXE] = 0;
	g.prop[AXE] = 0;

    }
    return;
}

/*
  ATTACK, KILL etc.
*/
void vkill()
{
    int msg, i, k;
    boolean survival;

    survival = TRUE;
    switch (object) {
    case BIRD:
	if (g.closed)
	    msg = 137;
	else {
	    destroy(BIRD);
	    g.prop[BIRD] = 0;
	    if (g.place[SNAKE] == plac[SNAKE])
		g.tally2++;
	    msg = 45;
	}
	break;
    case DWARF:
	if (g.closed) {
	    dwarfend();
	    return;
	}
	survival = do_battle(&msg);
	break;
    case 0:
	msg = 44;
	break;
    case CLAM:
    case OYSTER:
	msg = 150;
	break;
    case DOG:
	if (g.prop[DOG] == 1)
	    msg = 291;
	else if (iobj == AXE) {
	    object = AXE;
	    iobj = DOG;
	    vthrow();
	    return;
	} else
	    msg = 110;
	break;
    case SNAKE:
	msg = 46;
	break;
    case TROLL:
	if (iobj == AXE)
	    msg = 158;
	else
	    msg = 110;
	break;
    case BEAR:
	msg = 165 + (g.prop[BEAR] + 1) / 2;
	break;
    case WUMPUS:
	if (g.prop[WUMPUS] == 6)
	    msg = 167;
	else if (iobj == AXE) {
	    object = AXE;
	    iobj = WUMPUS;
	    vthrow();
	    return;
	} else
	    msg = 110;
	break;
    case GNOME:
	msg = 320;
	break;
    case DRAGON:
	if (g.prop[DRAGON] != 0) {
	    msg = 167;
	    break;
	}
	if (!yes(49, 0, 0))
	    break;
	pspeak(DRAGON, 1);
	biton(DRAGON, DEADBT);
	g.prop[DRAGON] = 2;
	g.prop[RUG] = 0;
	k = (plac[DRAGON] + fixd[DRAGON]) / 2;
	move((DRAGON + MAXOBJ), -1);
	move((RUG + MAXOBJ), 0);
	move(DRAGON, k);
	move(RUG, k);
	for (i = 1; i < MAXOBJ; i++)
	    if (g.place[i] == plac[DRAGON]
		|| g.place[i] == fixd[DRAGON]
		|| holding(i))
		move(i, k);
	g.loc = k;
	g.newloc = k;
	return;
    default:
	actspk(verb);
	return;
    }
    rspeak(msg);
    if (!survival) {
	g.oldloc2 = g.loc;
	death();
    }
    return;
}

static boolean do_battle(msg_ptr)
int *msg_ptr;
{
    boolean survival;
    int temp;

    survival = TRUE;
    if (iobj == 0)
	*msg_ptr = 49;
    else if (iobj != AXE && iobj != SWORD) {
	*msg_ptr = 355;
	survival = FALSE;
    } else if (pct(25)) {
	temp = iobj;
	iobj = object;
	object = temp;
	vthrow();
	return (TRUE);
    } else if (pct(25)) {
	*msg_ptr = 355;
	survival = FALSE;
    } else if (pct(36))
	*msg_ptr = 354;
    else {
	rspeak(356);
	if (pct(61))
	    *msg_ptr = 52;
	else {
	    *msg_ptr = 53;
	    survival = FALSE;
	}
    }
    return (survival);
}

/*
  POUR
*/
void vpour()
{
    int msg;

    if (object == BOTTLE || object == CASK) {
	iobj = object;
	object = liq(iobj);
	if (object == 0) {
	    rspeak(316);
	    return;
	}
    } else {
	if (object < WATER || object > (WINE + 1)) {
	    rspeak(78);
	    return;
	}
    }
    if (!holding(BOTTLE) && !holding(CASK)) {
	rspeak(29);
	return;
    }
    if (holding(BOTTLE) && liq(BOTTLE) == object)
	iobj = BOTTLE;
    if (holding(CASK) && liq(CASK) == object)
	iobj = CASK;
    if (iobj == 0) {
	rspeak(29);
	return;
    }
    if (!ajar(iobj)) {
	rspeak(335);
	return;
    }
    if (iobj == CASK)
	object++;
    g.prop[iobj] = 1;
    extract(object);
    g.place[object] = 0;
    msg = 77;
    if (iobj == CASK) {
	object--;
	msg = 104;
    }
    if (at(PLANT) || at(DOOR) || (at(SWORD) && g.prop[SWORD] != 0)) {
	if (at(DOOR)) {
	    g.prop[DOOR] = 0;
	    if (object == OIL) {
		g.prop[DOOR] = 1;
		bitoff(DOOR, LOCKBT);
		biton(DOOR, OPENBT);
	    }
	    msg = 113 + g.prop[DOOR];
	} else if (at(SWORD)) {
	    /* If sword is alread oily, don't let him clean it. No
	       soap. */
	    if (g.prop[SWORD] != 5) {
		g.prop[SWORD] = 4;
		if (object == OIL) {
		    g.prop[SWORD] = 5;
		    g.fixed[SWORD] = -1;
		}
		msg = 206 + g.prop[SWORD] - 4;
	    }
	} else {
	    msg = 112;
	    if (object == WATER) {
		if (g.prop[PLANT] < 0)
		    g.prop[PLANT] = -g.prop[PLANT] - 1;
		pspeak(PLANT, g.prop[PLANT] + 1);
		g.prop[PLANT] = (g.prop[PLANT] + 2) % 6;
		g.prop[PLANT2] = g.prop[PLANT] / 2;
		newtravel = TRUE;
		return;
	    }
	}
    }
    rspeak(msg);
    return;
}

/*
  EAT
  If he ate the right thing and is in the right place, move him to
  the other place with all his junk.  Otherwise, narky message.
*/
void veat()
{
    int msg, i, k, ll, kk;

    switch (object) {
    case HONEY:
	g.tally2++;
    case FOOD:
	destroy(object);
	msg = 72;
	break;
    case BIRD:
    case SNAKE:
    case CLAM:
    case OYSTER:
    case FLOWER:
	msg = 301;
	break;
    case DWARF:
    case DRAGON:
    case TROLL:
    case DOG:
    case WUMPUS:
    case BEAR:
    case GNOME:
	msg = 250;
	break;
    case MUSHRM:
    case CAKES:
	k = object - MUSHRM;
	ll = 229 + k;
	k = 159 - k;
	kk = SKEY;
	if (object == MUSHRM) {
	    kk = TDOOR;
	    if (g.loc != 158)
		g.tally2++;
	}
	destroy(object);
	msg = 228;
	if (!(here(kk) || g.fixed[kk] == g.loc))
	    break;
	msg = ll;
	/* If he hasn't taken tiny key off shelf, don't let him get it
	   for free! */
	for (i = 1; i < MAXOBJ; i++) {
	    if (i == SKEY && g.prop[SKEY] == 1)
		continue;
	    if (g.place[i] == plac[kk] && g.fixed[i] == 0)
		move(i, k);
	}
	if (g.loc == plac[SKEY] && g.place[SKEY] == plac[SKEY])
	    g.tally2++;
	g.loc = k;
	g.newloc = k;
	newtravel = TRUE;
	break;
    default:
	actspk(verb);
	return;
    }
    rspeak(msg);
    return;
}

/*
  DRINK
*/
void vdrink()
{
    int msg, k, j;

    if (object == 0 && (iobj == BOTTLE || iobj == CASK))
	object = liq(iobj);
    msg = 110;
    if (object == OIL)
	msg = 301;
    if (object != WATER && object != WINE) {
	rspeak(msg);
	return;
    }
    if (iobj == 0) {
	if (object == liqloc(g.loc))
	    iobj = -1;
	if (athand(CASK) && object == liq(CASK))
	    iobj = CASK;
	if (athand(BOTTLE) && object == liq(BOTTLE))
	    iobj = BOTTLE;
    }
    msg = 73;
    if (iobj != -1) {
	if (iobj == CASK)
	    object++;
	extract(object);
	g.place[object] = 0;
	g.prop[iobj] = 1;
	msg = (iobj == CASK) ? 299 : 74;
    }
    if (object == WATER || object == (WATER + 1)) {
	rspeak(msg);
	return;
    }
    /* UH-OH. He's a wino. Let him reap the rewards of incontinence.
       He'll wander around for awhile, then wake up somewhere or other,
       having dropped most of his stuff. */
    rspeak(300);
    if (g.prop[LAMP] == 1)
	g.limit -= ranz(g.limit) / 2;
    if (g.limit < 10)
	g.limit = 25;
    k = 0;
    if (pct(15))
	k = 49;
    if (k == 0 && pct(15))
	k = 53;
    if (k == 0 && pct(25))
	k = 132;
    if (k == 0)
	k = 175;
    if (outside(g.loc))
	k = 5;
    if (k == g.loc) {
	rspeak(msg);
	return;
    }
    if (holding(AXE))
	move(AXE, k);
    if (holding(LAMP))
	move(LAMP, k);
    for (j = 1; j < MAXOBJ; j++) {
	if (wearng(j))
	    bitoff(j, WEARBT);
	if (holding(j))
	    drop(j, g.loc);
    }
    g.loc = k;
    g.newloc = k;
}

/*
  THROW etc.
*/
void vthrow()
{
    int msg, i, k, dwarfn;

    if (holding(ROD2) && object == ROD && !holding(ROD))
	object = ROD2;
    if (!holding(object)) {
	actspk(verb);
	return;
    }
    if (object == BOAT || object == BEAR) {
	rspeak(noway());
	return;
    }
    dwarfn = dcheck();
    if (iobj == 0) {
	/* No indirect object was specified.  If a dwarf is present,
	   assume it is the object. If not, look for other living
	   thing. If no living things present, treat 'THROW' as 'DROP'. */

	if (dwarfn)
	    iobj = DWARF;
	else {
	    /* No dwarves present; figure out pausible object. */
	    k = 0;
	    for (i = 1; i < MAXOBJ; i++) {
		if (at(i) && living(i)) {
		    iobj = i;
		    k++;
		}
	    }
	    if (k == 0) {
		vdrop();
		return;
	    }
	    /* It is a beastie of some sort.  Is there more than one?
	       Don't kill the bird by default. */
	    if (k > 1) {
		rspeak(43);
		return;
	    } else {
		if (iobj == BIRD) {
		    vdrop();
		    return;
		}
		if (treasr(object) && at(TROLL))
		    iobj = TROLL;
	    }
	}
    }
    if (object == SWORD || object == BOTTLE) {
	vbreak();
	return;
    }
    if (object == FLOWER && iobj == HIVE)
	iobj = BEES;
    if (edible(object) && living(iobj)) {
	vfeed();
	return;
    }
    /* If not axe, same as drop... */
    if (object != AXE && iobj != TROLL) {
	vdrop();
	return;
    }
    /* AXE is THROWN */
    msg = 48;
    switch (iobj) {
    case DRAGON:
	if (g.prop[DRAGON] == 0)
	    msg = 152;
	break;
    case DWARF:
	/* At a dwarf... */
	if (pct(75)) {
	    g.dseen[dwarfn] = g.dloc[dwarfn] = 0;
	    msg = 47;
	    ++g.dkill;
	    if (g.dkill == 1)
		msg = 149;
	}
	break;
    case BEAR:
	/* This'll teach him to throw axe at the bear */
	if (g.prop[BEAR] == 0) {
	    msg = 164;
	    drop(AXE, g.loc);
	    g.fixed[AXE] = -1;
	    g.prop[AXE] = 1;
	    juggle(BEAR);
	}
	rspeak(msg);
	return;
    case WUMPUS:
	/* Or the WUMPUS! */
	if (g.prop[WUMPUS] == 6) {
	    vdrop();
	    return;
	} else {
	    msg = 245;
	    g.prop[AXE] = 2;
	    if (g.prop[WUMPUS] == 0) {
		drop(AXE, g.loc);
		g.fixed[AXE] = -1;
		juggle(iobj);
	    } else {
		msg = 243;
		destroy(AXE);
	    }
	}
	rspeak(msg);
	return;
    case DOG:
	/* Or the nice doggie! */
	if (g.prop[DOG] != 1) {
	    msg = 248;
	    g.prop[AXE] = 3;
	    drop(AXE, g.loc);
	    g.fixed[AXE] = -1;
	    juggle(iobj);
	}
	rspeak(msg);
	return;
    case TROLL:
	/* Snarf a treasure for the troll */
	if (object == AXE) {
	    msg = 158;
	} else if (!treasr(object) ||
		   (object == CASK && (liq(CASK) != WINE))) {
	    vdrop();
	    return;
	} else {
	    msg = 159;
	    drop(object, 0);
	    if (object == CASK)
		g.place[WINE + 1] = 0;
	    move(TROLL, 0);
	    move((TROLL + MAXOBJ), 0);
	    drop(TROLL2, plac[TROLL]);
	    drop((TROLL2 + MAXOBJ), fixd[TROLL]);
	    juggle(CHASM);
	    rspeak(msg);
	    return;
	}
	break;

    default:
	/* Otherwise it is an attack */
	verb = KILL;
	object = iobj;
	iobj = objs[objx];
	vkill();
	return;
    }

    rspeak(msg);
    drop(AXE, g.loc);
    g.newloc = g.loc;
    describe();
}

/*
  FIND might be carrying it, or it might be here. else give caveat.
*/
void vfind()
{
    int msg;

    if (at(object) ||
	(liq(BOTTLE) == object && at(BOTTLE)) ||
	object == liqloc(g.loc))
	msg = 94;
    else if (dcheck() && g.dflag >= 2 && object == DWARF)
	msg = 94;
    else if (g.closed)
	msg = 138;
    else if (at(object))
	msg = 24;
    else {
	actspk(verb);
	return;
    }
    rspeak(msg);
    return;
}

/*
  FEED
*/
void vfeed()
{
    int msg;

    if (iobj == 0 || !living(iobj)) {
	int i, k, kk;

	if (object == BIRD) {
	    rspeak(100);
	    return;
	}
	if (!living(object)) {
	    rspeak(noway());
	    return;
	}
	/* See if there is anything edible around here. */

	kk = 0;
	k = 0;
	for (i = 1; i < MAXOBJ; i++)
	    if (here(i) && edible(i)) {
		k++;
		kk = i;
	    }
	iobj = object;
	object = kk;
	if (k != 1 && !dead(iobj)) {
	    printf("What do you want to feed the %s\n", otxt[objx]);
	    objs[1] = 0;
	    objx = 0;
	    return;
	}
    }
    /* Feed object ot indirect object */
    msg = 102;
    switch (iobj) {
    case DRAGON:
	if (g.prop[DRAGON] != 0)
	    msg = noway();
	break;
    case TROLL:
	msg = 182;
	break;
    case SNAKE:
	if (object == BIRD && !g.closed) {
	    msg = 101;
	    destroy(BIRD);
	    g.prop[BIRD] = 0;
	    g.tally2++;
	}
	break;
    case DWARF:
	msg = 103;
	g.dflag++;
	break;
    case BEAR:
	if (g.prop[BEAR] == 3)
	    msg = noway();
	if (g.prop[BEAR] == 1 || g.prop[BEAR] == 2)
	    msg = 264;
	if (object == FOOD)
	    msg = 278;
	if (object == HONEY) {
	    g.prop[BEAR] = 1;
	    g.fixed[AXE] = 0;
	    destroy(HONEY);
	    msg = 168;
	}
	break;
    case DOG:
	msg = 291;
	if (object == FOOD && g.prop[DOG] != 1) {
	    msg = 249;
	    destroy(FOOD);
	}
	break;
    case WUMPUS:
	if (g.prop[WUMPUS] == 6)
	    msg = 326;
	if (g.prop[WUMPUS] == 0)
	    msg = 327;
	if (object == FOOD)
	    msg = 240;
	break;
    case BEES:
	if (object == FLOWER) {
	    if (enclosed(FLOWER))
		extract(FLOWER);
	    drop(FLOWER, g.loc);
	    g.fixed[FLOWER] = -1;
	    g.prop[FLOWER] = 1;
	    drop(HONEY, g.loc);
	    juggle(HONEY);
	    msg = 267;
	    g.prop[HIVE] = 1;
	}
    }
    rspeak(msg);
    return;
}

/*
  FILL. object with iobj
*/
void vfill()
{
    int msg, k;

    if (!vessel(object))
	msg = 313;
    else {
	if (iobj == 0)
	    iobj = liqloc(g.loc);
	if (object == BOTTLE || object == CASK) {
	    k = (object == CASK) ? 1 : 0;
	    msg = 0;
	    if (iobj == 0)
		msg = 304 + k;
	    if (liq(object) != 0)
		msg = 302 + k;
	    if (msg != 0) {
		rspeak(msg);
		return;
	    }
	    msg = 306 + k;
	    if (iobj == OIL)
		msg = 308 + k;
	    if (iobj == WINE)
		msg = 310 + k;
	    g.prop[object] = (int) g.loc_attrib[g.loc] & 14;
	    g.place[iobj + k] = -1;
	    insert(iobj + k, object);
	} else if (object == VASE) {
	    if (iobj == 0 || !holding(VASE)) {
		rspeak(144);
		return;
	    }
	    msg = 145;
	    g.prop[VASE] = 2;
	    g.fixed[VASE] = -1;
	    if (enclosed(object))
		extract(object);
	    drop(object, g.loc);
	} else if (object == GRAIL)
	    msg = 298;
	else
	    msg = 339;
    }
    rspeak(msg);
}

/*
  READ. Magazine in dwarvish, message we've seen, and ... oyster?
*/
void vread()
{
    int msg;

    if (blind()) {
	actspk(verb);
	return;
    }
    if (object && iobj) {
	rspeak(confuz());
	return;
    }
    msg = confuz();
    if (!object)
	object = iobj;
    switch (object) {
    case BOOK:
    case BOOK2:
	msg = 142;
	break;
    case BILLBD:
	msg = 361;
	break;
    case CARVNG:
	msg = 372;
	break;
    case MAGAZINE:
	msg = 190;
	break;
    case MESSAGE:
	msg = 191;
	break;
    case POSTER:
	msg = 370;
	break;
    case TABLET:
	msg = 196;
	break;
    case OYSTER:
	if (g.hinted[2] && holding(OYSTER))
	    msg = 194;
	if (!g.hinted[2] && holding(OYSTER) && g.closed) {
	    g.hinted[2] = yes(192, 193, 54);
	    return;
	}
	break;
    }
    rspeak(msg);
    return;
}

/*
  BREAK. works for mirror in repository and, of course the
  vase and bottle.  Also the sword is more brittle than it appears.
*/
void vbreak()
{
    int msg, k;
    boolean it_broke;

    it_broke = FALSE;
    msg = 146;
    switch (object) {
    case MIRROR:
	msg = 148;
	if (g.closed) {
	    rspeak(197);
	    dwarfend();
	    return;
	}
	break;
    case VASE:
	if (g.prop[VASE] == 0) {
	    it_broke = TRUE;
	    msg = 198;
	    g.prop[VASE] = 2;
	}
	break;
    case BOTTLE:
	if (g.prop[BOTTLE] != 3) {
	    it_broke = TRUE;
	    k = liq(BOTTLE);
	    msg = 231;
	    g.prop[BOTTLE] = 3;
	    if (k) {
		extract(k);
		g.place[k] = 0;
	    }
	}
	break;
    case SWORD:
	msg = 29;
	if (holding(SWORD)) {
	    msg = 279;
	    g.prop[SWORD] = 4;
	    it_broke = TRUE;
	}
	break;
    }
    if (it_broke) {
	if (enclosed(object))
	    extract(object);
	if (holding(object))
	    drop(object, g.loc);
	g.fixed[object] = -1;
    }
    rspeak(msg);
    return;
}

/*
  WAKE. only use is to disturb the dwarves or the Wumpus.
  Other wumpus-wakers link here.
*/
void vwake()
{
    int msg;

    msg = actmsg[verb];
    if (at(WUMPUS)) {
	g.chase = TRUE;
	g.prop[WUMPUS] = 1;
	msg = 276;
    }
    if (at(DOG) && g.prop[DOG] == 1)
	msg = 291;
    if (object == DWARF && g.closed) {
	rspeak(199);
	dwarfend();
	return;
    }
    rspeak(msg);
    return;
}

/*
   YANK. A variant of 'CARRY'.  In general, not a good idea.
   At most, it gets the cloak or a couple of snide comments.
 */
void vyank()
{
    if (toting(object))
	vdrop();
    else if (object == BEAR && g.prop[CHAIN])
	rspeak(205);
    else if (object == CLOAK && g.prop[CLOAK] == 2) {
	/* Cloak. big trouble ahead. */
	g.prop[ROCKS] = 1;
	g.prop[CLOAK] = 0;
	g.fixed[CLOAK] = 0;
	carry(CLOAK, g.loc);
	rspeak(241);
	if (at(WUMPUS) && g.prop[WUMPUS] == 0) {
	    g.chase = 1;
	    g.prop[WUMPUS] = 1;
	    rspeak(276);
	}
    } else
	vtake();
    return;
}

/*
   WEAR.  Only good for jewels, ruby slippers, cloak & crown.
   But he might try the sword.  Anything else is ridiculous.
   Another variant of 'CARRY'.
 */
void vwear()
{
    int msg;

    if (object == SWORD && g.prop[SWORD] != 3)
	msg = 209;
    else if (worn(object)) {
	if (object == CLOAK && g.prop[CLOAK] == 2)
	    msg = 242;
	else if (wearng(object))
	    msg = (object == SHOES) ? 227 : 210;
	else {
	    g.prop[object] = 1;
	    biton(object, WEARBT);
	    if (enclosed(object))
		extract(object);
	    if (holding(object))
		msg = 54;
	    else {
		vtake();
		return;
	    }
	}
    } else {
	printf("Just exactly how does one wear a %s\n", otxt[objx]);
	return;
    }
    rspeak(msg);
    return;
}

/*
   HIT. If not punching out telephone, assume attack.
 */
void vhit()
{
    if (at(WUMPUS) && g.prop[WUMPUS] == 0) {
	vwake();
	return;
    }
    if (object != PHONE) {
	vkill();
	return;
    } else {
	if (g.closed) {
	    rspeak(282);
	    dwarfend();
	    return;
	}
	if (g.prop[PHONE] == 2)
	    rspeak(256);
	else {
	    drop(SLUGS, g.loc);
	    g.prop[PHONE] = 2;
	    g.prop[BOOTH] = 2;
	    rspeak(257);
	}
    }
    return;
}

/*
   ANSWER (telephone). Smartass for anything else.
 */
void vanswer()
{
    int msg;

    switch (object) {
    case DWARF:
    case WUMPUS:
    case SNAKE:
    case BEAR:
    case DRAGON:
	msg = 259;
	break;
    case TROLL:
	msg = 258;
	break;
    case BIRD:
	msg = 260;
	break;
    case PHONE:
	if (g.prop[PHONE] != 0)
	    msg = 269;
	else if (g.closed) {
	    rspeak(283);
	    normend();
	    return;
	} else {
	    msg = 261;
	    g.prop[PHONE] = 1;
	    g.prop[BOOTH] = 2;
	}
	break;
    default:
	msg = actmsg[verb];
	break;
    }
    rspeak(msg);
    return;
}

/*
   BLOW. Joshua fit de battle of Jericho, and de walls ...
 */
void vblow()
{
    int msg, i, k;

    msg = actmsg[verb];
    if (object != 0 && iobj != 0) {
	rspeak(msg);
	return;
    }
    if (object == 0)
	object = iobj;
    iobj = 0;
    if (object == 0)
	msg = 268;
    if (object == HORN) {
	msg = outside(g.loc) ? 277 : 266;
	if (at(WUMPUS)) {
	    rspeak(msg);
	    if (g.prop[WUMPUS] == 0)
		vwake();
	    return;
	} else if (g.prop[WALL] != 1 && (g.loc == 102 || g.loc == 194)) {
	    k = g.loc == 194 ? 195 : 196;
	    msg = 265;
	    g.prop[WALL] = 1;
	    for (i = 1; i < MAXOBJ; i++)
		if (g.place[i] == g.loc || g.fixed[i] == g.loc)
		    move(i, k);
	    g.newloc = k;
	}
    }
    rspeak(msg);
    return;
}

/*
   DIAL. No effect unless at phone.
 */
void vdial()
{
    if (object != PHONE)
	actspk(verb);
    else if (g.closed) {
	rspeak(283);
	normend();
    } else
	rspeak(271);
    return;
}

/*
   PLAY.  Only for horn or lyre.
 */
void vplay()
{
    int msg;

    msg = actmsg[verb];
    if (object != 0 && iobj != 0) {
	rspeak(confuz());
	return;
    }
    if (object == 0)
	object = iobj;
    if (object == HORN) {
	vblow();
	return;
    }
    if (object == LYRE) {
	msg = 287;
	if (here(DOG) && !dead(DOG)) {
	    g.prop[DOG] = 1;
	    biton(DOG, DEADBT);
	    g.fixed[AXE] = 0;
	    g.prop[AXE] = 0;
	    msg = 288;
	}
    }
    rspeak(msg);
    return;
}

/*
   PICK/ PICK UP.  Can pick flower & mushrooms,
   But must 'PICK UP' everything else.
 */
void vpick()
{
    if (object == 0)
	object = iobj;
    iobj = 0;
    if (object == FLOWER || object == MUSHRM || prep != 0)
	vtake();
    else
	rspeak(confuz());
    return;
}

/*
   PUT DOWN: equivalent to drop
   PUT IN: if liquid, means fill
   PUT ON: wear of drop
 */
void vput()
{
    if (prep == 0) {
	printf("Where do you want to put the %s\n", otxt[objx]);
	return;
    }
    if (prep == PREPIN)
	vinsert();
    else {
	/* PUT ON: wear or put object on iobj */
	if (prep == PREPON) {
	    if (object == 0) {
		object = iobj;
		otxt[objx] = iotxt[iobx];
		iobj = 0;
	    }
	    if (worn(object) || object == 0)
		vwear();
	    else
		vdrop();
	} else {
	    /* PUT DOWN: "drop" */
	    if (object == 0 && iobj == 0) {
		if (object == 0)
		    object = iobj;
		iobj = 0;
		vdrop();
	    } else
		rspeak(noway());
	}
    }
    return;
}

/* turn on/off */
void vturn()
{
    if (!prep)
	rspeak(confuz());
    else {
	if (!object && iobj == LAMP)
	    object = LAMP;
	if (object != LAMP)
	    rspeak(noway());
	else if (prep == PREPON)
	    von();
	else
	    voff();
    }
    return;
}

/*
   GET (no prep): "take"
   GET IN: "enter"
   GET OUT: "leave"
 */
void vget()
{
    if (prep == 0 || prep == PREPFR)
	vtake();
    else if (object == 0) {
	object = iobj;
	iobj = 0;
	prep = 0;
	vtake();
    }
    return;
}

/*
   INSERT/PUT IN
 */
void vinsert()
{
    int msg;

    if (iobj == 0) {
	printf("Where do you want to %s it?\n", vtxt[vrbx]);
	return;
    }
    msg = noway();
    if (object == SWORD && iobj == ANVIL && g.prop[SWORD] == 0)
	msg = 350;
    if (!vessel(iobj)) {
	rspeak(msg);
	return;
    }
    msg = ck_obj();
    if (g.fixed[object]) {
	rspeak(msg);
	return;
    }
    if (object == iobj) {
	rspeak(252);
	return;
    }
    if (iobj == BOTTLE || iobj == CASK || iobj == VASE
	|| iobj == GRAIL || (object >= WATER && object <= WINE + 1)) {
	object = iobj;
	iobj = objs[objx];
	vfill();
	return;
    }
    if (!ajar(iobj)) {
	rspeak(358);
	return;
    }
    if (iobj == CHEST) {
	if (object == BOAT)
	    msg = noway();
	else {
	    if (wearng(object))
		bitoff(object, WEARBT);
	    if (worn(object))
		g.prop[object] = 0;
	    if (enclosed(object))
		extract(object);
	    insert(object, iobj);
	    msg = 54;
	}
	rspeak(msg);
	return;
    }
    /* Bird goes into cage and only cage */
    if (object == BIRD && iobj != CAGE) {
	rspeak(351);
	return;
    }
    if (object != BIRD && iobj == CAGE) {
	rspeak(329);
	return;
    }
    if (object == BIRD) {
	prep = 0;
	vtake();
	return;
    }
    /* Bar vase & pillow from safe, to force putting down on florr */
    if ((object == VASE || object == PILLOW) && iobj == SAFE) {
	rspeak(329);
	return;
    }
    if (object != RADIUM && iobj == SHIELD) {
	rspeak(329);
	return;
    }
    if (iobj == PHONE) {
	if (object == COINS || object == SLUGS) {
	    destroy(object);
	    msg = 330;
	} else
	    msg = 329;
	rspeak(msg);
	return;
    }
    if (iobj == VEND) {
	if (object == COINS || object == SLUGS) {
	    destroy(object);
	    move(BATTERIES, g.loc);
	    if (g.prop[BATTERIES] == 1) {
		rspeak(317);
		g.prop[VEND] = 1;
	    }
	    g.prop[BATTERIES] = 0;
	    pspeak(BATTERIES, 0);
	} else
	    rspeak(noway());
	return;
    }
    /* Put batteries in lamp. There is a glitch here, in that if he
       tries to get a third set of batteries before the second set has
       been inserted, the second set disappears!
       ***fix this some time ***
     */
    if (iobj == LAMP) {
	if (object != BATTERIES || g.prop[BATTERIES] != 0)
	    msg = noway();
	else {
	    g.prop[BATTERIES] = 1;
	    if (enclosed(BATTERIES))
		extract(BATTERIES);
	    if (holding(BATTERIES))
		drop(BATTERIES, g.loc);
	    g.limit = 400;
	    g.prop[LAMP] = 1;
	    g.lmwarn = FALSE;
	    msg = 188;
	}
	rspeak(msg);
	return;
    }
    if (!small(object))
	msg = 329;
    else {
	if (wearng(object))
	    bitoff(object, WEARBT);
	if (worn(object))
	    g.prop[object] = 0;
	if (enclosed(object))
	    extract(object);
	insert(object, iobj);
	msg = 54;
    }
    rspeak(msg);
    return;

}

/* Remove or take from */
void vextract()
{
    int msg;

    if (object == RING && g.prop[RING] == 2) {
	prep = 0;
	iobj = 0;
	vtake();
	return;
    }
    msg = 343;
    if (iobj == 0) {
	if (!enclosed(object))
	    msg = 340;
	iobj = -g.place[object];
    }
    if (g.place[object] != -iobj)
	msg = 341;
    if (!ajar(iobj))
	msg = 335;
    if (object == WATER || object == OIL || object == WINE)
	msg = 342;
    if (!toting(object) && ((burden(0) + burden(object)) > 15))
	msg = 92;
    if (msg == 343) {
	if (object == BIRD) {
	    vdrop();
	    return;
	}
	extract(object);
    }
    rspeak(msg);
    return;
}

/*
   lock. chain, grate, chest, elfin door
   Here are the current lock/unlock messages & numbers:
   31	you have no keys.
   32	it has no lock.
   34	it's already locked.
   35	the grate is now locked.
   36	the grate is now unlocked.
   37	it was allready unlocked.
   55	you can't unlock the keys.
   171	The chain is now unlocked.
   172	The chain is now locked.
   173	There is nothing here to which the chain can be locked.
   224	Your keys are all too large.
   234	The wrought-iron door is now locked.
   235	The tiny door is now locked.
   236	The wrought-iron door is now unlocked.
   237	The tiny door is now unlocked.
   375	You don't have the right key.
   333	the chest is now locked.
   334	the chest is now unlocked.
   367	The safe's door swings shut.
*/
void vlock()
{
    int msg, k;

    if (!hinged(object))
    {
	printf("I don't know how to lock or unlock the %s\n",
	       otxt[objx]);
	return;
    }
    else if (!locks(object))
	msg = 32;
    else if (locked(object))
	msg = 34;
    else if (!athand(KEYS) && !athand(SKEY) && object != SAFE)
	msg = 31;
    else {
	msg = 375;
	switch (object) {
	case CHAIN:
	    if (!athand(KEYS))
		break;
	    msg = 173;
	    if (g.loc != plac[CHAIN])
		break;
	    msg = 172;
	    g.prop[CHAIN] = 2;
	    if (enclosed(CHAIN))
		extract(CHAIN);
	    if (holding(CHAIN))
		drop(CHAIN, g.loc);
	    g.fixed[CHAIN] = -1;
	    biton(CHAIN, LOCKBT);
	    bitoff(CHAIN, OPENBT);
	    break;

	case CHEST:
	    if (!athand(KEYS))
		break;
	    msg = 334;
	    biton(CHEST, LOCKBT);
	    bitoff(CHEST, OPENBT);
	    break;

	case TDOOR:
	case TDOOR2:
	    msg = 224;
	    if (!toting(SKEY))
		break;
	    g.prop[TDOOR] = 0;
	    g.prop[TDOOR2] = 0;
	    msg = 234 + (TDOOR2 - object);
	    k = TDOOR + TDOOR2 - object;
	    biton(k, LOCKBT);
	    bitoff(k, OPENBT);
	    biton(object, LOCKBT);
	    bitoff(object, OPENBT);
	    break;

	case GRATE:
	    if (!athand(KEYS))
		break;
	    g.prop[GRATE] = 0;
	    msg = 35;
	    biton(GRATE, LOCKBT);
	    bitoff(GRATE, OPENBT);
	    break;

	case SAFE:
	    g.prop[SAFE] = 0;
	    msg = 367;
	    biton(SAFE, LOCKBT);
	    bitoff(SAFE, OPENBT);
	    break;

	}
    }
    rspeak(msg);
}

/*
   UNLOCK. chain, grate, chest, elfin door.
*/
void vunlock()
{
    int msg, k;

    if (object == KEYS || object == SKEY)
	msg = 55;
    else if (!hinged(object))
    {
	printf("I don't know how to lock or unlock the %s\n",
	       otxt[objx]);
	return;
    }
    else if (!locked(object))
	msg = 37;
    else if (!locks(object))
	msg = 32;
    else if (object == SAFE) {
	if (iobj == KEYS || iobj == SKEY)
	    msg = 368;
	else
	    msg = 342;
    } else if (!athand(KEYS) && !athand(SKEY))
	msg = 31;
    else {
	msg = 375;
	switch (object) {
	case CHAIN:
	    if (!athand(KEYS))
		break;
	    if (g.prop[BEAR] == 0)
		msg = 41;
	    else {
		msg = 171;
		g.prop[CHAIN] = 0;
		g.fixed[CHAIN] = 0;
		if (g.prop[BEAR] != 3)
		    g.prop[BEAR] = 2;
		g.fixed[BEAR] = 2 - g.prop[BEAR];
		bitoff(CHAIN, LOCKBT);
		biton(CHAIN, OPENBT);
	    }
	    break;
	case CHEST:
	    if (athand(KEYS)) {
		msg = 333;
		bitoff(CHEST, LOCKBT);
		biton(CHEST, OPENBT);
	    }
	    break;
	case TDOOR:
	case TDOOR2:
	    /* Elvin door stuff to lock/unlock tiny door w/special key.
	       the damn thing is really at four places, and we want the
	       right messages if he only has 'BIG'keys (or no keys).
	       Also, he can unlock it either while he is big or small. */
	    msg = 224;
	    if (!athand(SKEY))
		break;
	    if (g.closing) {
		msg = 130;
		if (!g.panic)
		    g.clock2 = 15;
		g.panic = TRUE;
	    } else {
		g.prop[TDOOR] = 1;
		g.prop[TDOOR2] = 1;
		msg = 234 + 2 + (TDOOR2 - object);
		k = TDOOR + (TDOOR2 - object);
		bitoff(k, LOCKBT);
		biton(k, OPENBT);
		bitoff(object, LOCKBT);
		biton(object, OPENBT);
	    }
	    break;
	case GRATE:
	    if (!athand(KEYS))
		break;
	    if (g.closing) {
		msg = 130;
		if (!g.panic)
		    g.clock2 = 15;
		g.panic = TRUE;
	    } else {
		g.prop[GRATE] = 1;
		msg = 36;
		bitoff(GRATE, LOCKBT);
		biton(GRATE, OPENBT);
	    }
	    break;
	default:
	    msg = 33;
	}
    }
    rspeak(msg);
}

/*
   LOOK.
*/
void vlook()
{
    int sloc;

    if (object != 0) {
	rspeak(confuz());
	return;
    }
    /* Look into something (a container). */
    if (vessel(iobj)) {
	if (!ajar(iobj) && opaque(iobj))
	    rspeak(actmsg[verb]);
	else if (g.holder[iobj] == 0)
	    rspeak(359);
	else {
	    putchar(' ');
	    lookin(iobj);
	}

	/* Look at something. If written, read it. */
    } else if (printed(iobj)) {
	object = iobj;
	iobj = 0;
	vread();
    } else if (iobj == SPHERE) {
	if (!inside(g.loc) || athand(SAPPHIRE))
	    rspeak(42);
	else {
	    rspeak(400);
	    printf("  ");
	    sloc = g.place[SAPPHIRE];
	    if ((g.loc_attrib[sloc] % 2 == 0 || enclosed(SAPPHIRE))
		&& sloc != 200
		&& !g.place[LAMP] == sloc && g.prop[LAMP] != 0)
		rspeak(401);
	    else
		desclg(sloc);
	    if (sloc == 239 && !g.flg239) {
		rspeak(403);
		g.flg239 = TRUE;
	    }
	    printf("  ");
	    rspeak(402);
	}
    } else
	printf("I see nothing special about the %s?\n", iotxt[iobx]);
    return;
}
