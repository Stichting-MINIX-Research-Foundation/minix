/*	program ITVERB.C					*/


#include	<stdio.h>
#include	"advent.h"
#include	"advdec.h"

_PROTOTYPE(void needobj, (void));
_PROTOTYPE(void ivtake, (void));
_PROTOTYPE(void ivopen, (void));
_PROTOTYPE(void ivkill, (void));
_PROTOTYPE(void ivdrink, (void));
_PROTOTYPE(void ivquit, (void));
_PROTOTYPE(void ivfoo, (void));
_PROTOTYPE(void inventory, (void));
_PROTOTYPE(void addobj, (int obj));
_PROTOTYPE(void ivpour, (void));
_PROTOTYPE(void ivfill, (void));
_PROTOTYPE(void ivbrief, (void));
_PROTOTYPE(void ivread, (void));
_PROTOTYPE(void ivcombo, (void));
_PROTOTYPE(void iveat, (void));
/*
  Routines to process intransitive verbs
*/
void itverb()
{
    int i;

    newtravel = FALSE;
    switch (verb) {
    case DROP:
    case SAY:
    case WAVE:
    case CALM:
    case RUB:
    case THROW:
    case FIND:
    case FEED:
    case BREAK:
    case WAKE:
    case WEAR:
    case HIT:
    case DIAL:
    case PLAY:
    case PICK:
    case PUT:
    case TURN:		needobj();	break;
    case TAKE:
    case YANK:
    case GET:
    case INSRT:
    case REMOVE:
    case BURN:		ivtake();	break;
    case OPEN:
    case CLOSE:
    case LOCK:
    case UNLOCK:	ivopen();	break;
    case NOTHING:	rspeak(54);	break;
    case ON:
    case OFF:		trverb();	break;
    case WALK:		actspk(verb);	break;
    case KILL:		ivkill();	break;
    case POUR:		ivpour();	break;
    case EAT:		iveat();	break;
    case DRINK:		ivdrink();	break;
    case QUIT:		ivquit();	break;
    case INVENTORY:	inventory();	break;
    case FILL:		ivfill();	break;
    case BLAST:		ivblast();	break;
    case SCORE:		score(TRUE);	break;
    case FOO:		ivfoo();	break;
    case BRIEF:		ivbrief();	break;
    case READ:		ivread();	break;
    case SUSPEND:
	if (g.closing)
	    rspeak(378);
	else
	    saveadv("advent.sav");
	break;
    case RESTORE:	restore("advent.sav");	break;
    case ANSWER:
	if ((g.loc != 189) || (g.prop[PHONE] != 0))
	    needobj();
	else {
	    object = PHONE;
	    itverb();
	}
	break;
    case BLOW:		rspeak(268);	break;
	/* Action verb 'LEAVE' has no object */
    case LEAVE:		bug(29);	break;
	/* Call if no phone is handy, yell. */
    case YELL:
	if (!here(PHONE))
	    needobj();
	else if (!g.closed)
	    rspeak(271);
	else {
	    rspeak(283);
	    normend();
	}
	break;
	/* Health. give him a diagnosis. */
    case HEALTH:
	if (g.numdie)
	    fprintf(stdout, "You have been killed %d times otherwise\n",
		    g.numdie);
	if (g.health >= 95) {
	    if (pct(50))
		rspeak(348);
	    else
		rspeak(349);
	} else {
	    fprintf(stdout,
	       "Your health rating is %2d out of a possible 100.\n",
		    g.health);
	    rspeak(381 + (100 - g.health) / 20);
	}
	break;
    case LOOK:		ivlook();	break;
    case COMBO:
	if (at(SAFE))
	    ivcombo();
	break;
    case SWEEP:
	/* Dust/sweep */
	if (!at(CARVNG) || !athand(BRUSH) || (g.prop[CARVNG] == 1))
	    rspeak(342);
	else {
	    g.prop[CARVNG] = 1;
	    rspeak(363);
	    rspeak(372);
	}
	break;
    case TERSE:
	/* Terse/unterse. supress all long_form descriptions. */
	g.terse = !g.terse;
	g.detail = 3;
	rspeak(54);
	break;
    case WIZ:
	is_wiz = !is_wiz;
    case MAP:
	rspeak(54);
	break;
    case GATE:
	if (is_wiz) {
	    static char buf[INPUTBUFLEN];
	    sscanf(ask("Location ? ", buf, sizeof(buf)), "%d", &g.loc);
	}
	rspeak(54);
	break;
    case PIRLOC:
	if (is_wiz) {
	    fprintf(stdout, "The dwarfs are at locations:\n");
	    for (i = 1; i < DWARFMAX; i++)
		fprintf(stdout, "  %4d", g.dloc[i]);
	    fprintf(stdout, "\nThe pirate is at location %4d\n",
		    g.dloc[DWARFMAX]);
	}
	rspeak(54);
	break;
    default:
	printf("This intransitive not implemented yet\n");
    }
    return;
}

/*
  Routine to indicate no reasonable
  object for verb found.  Used mostly by
  intransitive verbs.
*/
void needobj()
{
    printf("%s what?\n", vtxt[vrbx]);
    return;
}

/*
  CARRY, TAKE etc.
*/
void ivtake()
{
    int anobj, item;

    anobj = 0;
    for (item = 1; item < MAXOBJ; ++item)
	if (g.place[item] == g.loc)
	    if (anobj == 0)
		anobj = item;
	    else {
		needobj();
		return;
	    }

    if (anobj == 0 || (dcheck() && g.dflag >= 2) || blind())
	needobj();
    else {
	object = anobj;
	if (verb == YANK)
	    vyank();
	else if (verb == WEAR)
	    vwear();
	else
	    vtake();
    }
    return;
}

/*
  OPEN, LOCK, UNLOCK
*/
void ivopen()
{
    int obj_cnt, item;

    for (item = 1, obj_cnt = 0; item < MAXOBJ; item++) {
	if ((g.place[item] == g.loc) && (hinged(item))) {
	    object = item;
	    obj_cnt++;
	}
    }
    if (obj_cnt != 1)
	needobj();
    else if (verb == LOCK)
	vlock();
    else if (verb == UNLOCK)
	vunlock();
    else if (verb == SHUT)
	vclose();
    else
	vopen();
}

/*
  ATTACK, KILL etc
*/
boolean previous_obj;

void ivkill()
{
    previous_obj = FALSE;
    if (dcheck() && g.dflag >= 2)
	object = DWARF;
    if (here(SNAKE))
	addobj(SNAKE);
    if (at(DRAGON) && g.prop[DRAGON] == 0)
	addobj(DRAGON);
    if (at(TROLL))
	addobj(TROLL);
    if (here(GNOME))
	addobj(GNOME);
    if (here(BEAR) && g.prop[BEAR] == 0)
	addobj(BEAR);
    if (here(WUMPUS) && g.prop[WUMPUS] == 0)
	addobj(WUMPUS);
    /* Can't attack bird by throwing axe */
    if (here(BIRD) && verb != THROW)
	addobj(BIRD);
    /* Clam and oyster both treated as clam for intransitive case; no
       harm done. */
    if (here(CLAM) || here(OYSTER))
	addobj(CLAM);

    if ((previous_obj) || (object == 0))
	rspeak(44);
    else
	vkill();
    return;
}

/*
  POUR if no object, assume liq in container, if holding one.
*/
void ivpour()
{
    if ((holding(BOTTLE)) && (liq(BOTTLE) != 0) && !holding(CASK))
	object = BOTTLE;
    if ((holding(CASK)) && (liq(CASK) != 0) && !holding(BOTTLE))
	object = CASK;

    if (object == 0)
	needobj();
    else
	trverb();
}

/*
  EAT. intransitive: assume edible if present, else ask what.
  If he as more than one edible, or none, 'EAT' is ambiguous
  without an explicit object.
*/
void iveat()
{
    int i;

    previous_obj = FALSE;
    for (i = 1; i < MAXOBJ; i++) {
	if ((here(i)) && (edible(i)))
	    addobj(i);
    }
    if ((previous_obj) || (object == 0))
	needobj();
    else
	trverb();
}

/*
  DRINK.  If no object, assume water or wine and look for them here.
  If potable is in bottle or cask, drink that.  If not, see if there
  is something drinkable nearby (stream, lake, wine fountain, etc.),
  and drink that.  If he has stuff in both containers, ask which.
*/
void ivdrink()
{
    int ll;

    previous_obj = FALSE;
    ll = liqloc(g.loc);
    if ((ll == WATER) || (ll == WINE)) {
	object = ll;
	iobj = -1;
    }
    ll = liq(BOTTLE);
    if ((athand(BOTTLE)) && ((ll == WATER) || (ll == WINE))) {
	object = ll;
	iobj = BOTTLE;
    }
    ll = liq(CASK);
    if ((athand(CASK)) && ((ll == WATER) || (ll == WINE))
	&& iobj != BOTTLE) {
	object = ll;
	iobj = CASK;
    } else
	object = 0;

    if (object == 0)
	needobj();
    else
	trverb();
}

/*
  QUIT intransitive only. Verify intent and exit if that's what he wants
*/
void ivquit()
{
    gaveup = yes(22, 54, 54);
    if (gaveup)
	normend();
    return;
}

/*
  INVENTORY
*/
void inventory()
{
    int i, msg;
    boolean init_msg;

    init_msg = TRUE;
    msg = 98;
    for (i = 1; i < MAXOBJ; i++) {
	if (!holding(i) || wearng(i) || i == BEAR || i == BOAT)
	    continue;
	if (init_msg)
	    rspeak(99);
	pspeak(i, -1);
	init_msg = FALSE;
	msg = 0;
	lookin(i);
    }

    /* Tell him what he is wearing */
    init_msg = TRUE;
    for (i = 1; i < MAXOBJ; i++) {
	if (wearng(i)) {
	    if (init_msg)
		fprintf(stdout, "\nYou are wearing:\n");
	    fprintf(stdout, "     ");
	    pspeak(i, -1);
	    msg = 0;
	    init_msg = FALSE;
	}
    }

    if (holding(BOAT)) {
	rspeak(221);
	lookin(BOAT);
    }
    if (holding(BEAR))
	msg = 141;

    if (msg)
	rspeak(msg);
    return;
}

/*
  FILL bottle or cask must be empty, and some liquid avaible
*/
void ivfill()
{
    if ((g.prop[CASK] == 1) && !here(CASK))
	object = CASK;
    if ((g.prop[BOTTLE] == 1) && !here(BOTTLE))
	object = BOTTLE;

    if ((here(BOTTLE) && here(CASK)) || (object == 0))
	needobj();
    else
	trverb();
}

/*
  BLAST etc.
*/
void ivblast()
{
    if (!g.closed)
	actspk(verb);
    else {
	g.bonus = 135;
	if (g.place[ROD2] == 212 && g.loc == 116)
	    g.bonus = 133;
	if (g.place[ROD2] == 116 && g.loc != 116)
	    g.bonus = 134;
	rspeak(g.bonus);
	normend();
    }
    return;
}

/*
  Handle fee fie foe foo...
*/
void ivfoo()
{
    int k;
    int msg;

    k = VAL(vocab(vtxt[vrbx], MISC));
    if (g.foobar != 1 - k) {
	if (g.foobar == 0)
	    msg = 42;
	else
	    msg = 151;
	rspeak(msg);
	return;
    }
    g.foobar = k;
    if (k != 4)
	return;
    g.foobar = 0;
    if (g.place[EGGS] == plac[EGGS] ||
	(toting(EGGS) && g.loc == plac[EGGS])) {
	rspeak(42);
	return;
    }
    /* Bring back troll if we steal the eggs back from him before
       crossing */
    if (g.place[EGGS] == 0 && g.place[TROLL] == 0 && g.prop[TROLL] == 0)
	g.prop[TROLL] = 1;

    if (here(EGGS))
	k = 1;
    else if (g.loc == plac[EGGS])
	k = 0;
    else
	k = 2;
    move(EGGS, plac[EGGS]);
    pspeak(EGGS, k);
    return;
}

/*
  brief/unbrief. intransitive only.
  suppress long descriptions after first time.
*/
void ivbrief()
{
    int msg;

    g.detail = 3;
    g.terse = FALSE;
    if (g.abbnum != 10000) {
	msg = 156;
	g.abbnum = 10000;
    } else {
	msg = 374;
	g.abbnum = 5;
    }
    rspeak(msg);
}

/*
  read etc...
*/
void ivread()
{
    previous_obj = FALSE;
    if (here(BOOK))
	object = BOOK;
    if (here(BOOK2))
	addobj(BOOK2);
    if (here(BILLBD))
	addobj(BILLBD);
    if (here(CARVNG))
	addobj(CARVNG);
    if (here(MAGAZINE))
	addobj(MAGAZINE);
    if (here(MESSAGE))
	addobj(MESSAGE);
    if (here(OYSTER))
	addobj(OYSTER);
    if (here(POSTER))
	addobj(POSTER);
    if (here(TABLET))
	addobj(TABLET);

    if (previous_obj || object == 0 || dark())
	needobj();
    else
	vread();
    return;
}

/*
   LOOK. can't give more detail. Pretend it wasn't dark (though it may "now"
   be dark) so he won't fall into a pit staring into the gloom.
*/
void ivlook()
{
    if (g.detail++ < 3)
	rspeak(15);
    g.wzdark = FALSE;
    g.visited[g.loc] = 0;
    g.newloc = g.loc;
    newtravel = TRUE;
    return;
}

/*
  COMBO: trying to open safe. (see comments for fee fie foe foo)
*/
void ivcombo()
{
    int k, msg;

    k = VAL(vocab(vtxt[vrbx], MISC)) - 10;
    msg = 42;
    if (g.combo != 1 - k) {
	if (g.combo != 0)
	    msg = 366;
	rspeak(msg);
	return;
    }
    g.combo = k;
    if (k != 3)
	rspeak(371);
    else {
	g.combo = 0;
	bitoff(SAFE, LOCKBT);
	biton(SAFE, OPENBT);
	g.prop[SAFE] = 1;
	if (g.prop[BOOK] < 0) {
	    g.tally--;
	    g.prop[BOOK] = 0;
	    /* If remaining treasures too elusive, zap his lamp. this
	       duplicates some code, must be done here since book is
	       contained ins safe & tally stuff only works for thing
	       deposited at a location. */
	    if ((g.tally == g.tally2) && (g.tally != 0))
		g.limit = (g.limit < 35) ? g.limit : 35;
	}
	rspeak(365);
    }
}

/*
  ensure uniqueness as objects are searched
  out for an intransitive verb
*/
void addobj(obj)
int obj;
{
    if (!previous_obj) {
	if (object != 0)
	    previous_obj = TRUE;
	else
	    object = obj;
    }
    return;
}
