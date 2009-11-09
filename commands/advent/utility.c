/*
  Utility Routines
  the next logical funtions describe attributes of objects.
  (ajar, hinged, opaque, printd, treasr, vessel, wearng)
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<string.h>
#include	"advent.h"
#include	"advdec.h"

/*
  ajar .TRUE. if item is container and is open or unhinged
*/
boolean ajar(item)
int item;
{
    return ((bitset(g.obj_state[item], OPENBT))
	    || (vessel(item) && !hinged(item)));
}

/*
  at .TRUE. To tell if player is on either side of a two sided object.
*/
boolean at(item)
int item;
{
    if (item < 1 || item > MAXOBJ)
	return (FALSE);
    else
	return (g.place[item] == g.loc || g.fixed[item] == g.loc);
}

/*
  athand .TRUE. if item readily reachable
  it can be lying here, in hand or in open container.
*/
boolean athand(item)
int item;
{
    int contnr;
    boolean aaa;

    contnr = -g.place[item];
    aaa = enclosed(item) && ajar(contnr);

    return ((g.place[item] == g.loc) || holding(item)
	    || (aaa && ((g.place[contnr] == g.loc)
			|| (toting(item) && holding(contnr)))));
}

/*
  bitoff turns off (sets to 0) a bit in obj_state word
*/
void bitoff(obj, bit)
int obj, bit;
{
    long val;

    val = 1L << bit;
    g.obj_state[obj] &= ~val;
}

/*
  biton turns on (sets to 1) a bit in obj_state word
*/
void biton(obj, bit)
int obj, bit;
{
    long val;

    val = 1L << bit;
    g.obj_state[obj] |= val;
}

/*
   bitset .TRUE. if object_state has bit N set
*/
boolean bitset(state, bit)
long state;
int bit;
{
    return (((state >> bit) & 1) == 1);
}

/*
  blind .TRUE. if you can't see at this loc, (darkness of glare)
*/
boolean blind()
{
    return (dark() || (g.loc == 200
		       && athand(LAMP) && (g.prop[LAMP] == 1)));
}

/*
   burden .. returns weight of items being carried

   if obj=0, burden calculates the total weight of the adventurer's burden
   including everything in all containers (except the boat) that he is
   carring.

   if object is a container, calculate the weight of everything inside
   the container (including the container itself). Since donkey FORTRAN
   isn't recursive, we will only calculate weight of contained containers
   one level down.  The only serious contained container would be the sack
   The only thing we'll miss will be filled VS empty bottle or cage.

   If object isn't a container, return its weight.
*/
int burden(obj)
int obj;
{
    int i, sum, temp;

    sum = 0;
    if (obj == 0) {
	for (i = 1; i < MAXOBJ; i++) {
	    if (toting(i) && (g.place[i] != -BOAT))
		sum += g.weight[i];
	}
    } else {
	if (obj != BOAT) {
	    sum = g.weight[obj];
	    temp = g.holder[obj];
	    while (temp != 0) {
		sum += g.weight[temp];
		temp = g.hlink[temp];
	    }
	}
    }
    return (sum);
}

/*
  Routine to carry an object
  start toting an object, removing it from the list of things
  at its former location.  If object > MAXOBJ ( moving "FIXED"
  or second loc), then don't change place.
*/
void carry(obj, where)
int obj, where;
{
    int temp;

    if (obj < MAXOBJ) {
	if (g.place[obj] == -1)
	    return;
	g.place[obj] = -1;
    }
    if (g.atloc[where] == obj)
	g.atloc[where] = g.link[obj];
    else {
	temp = g.atloc[where];
	while (g.link[temp] != obj) {
	    temp = g.link[temp];
	    if (temp == 0)
		bug(35);
	}
	g.link[temp] = g.link[obj];
    }
    return;
}

/*
  confuz generates some variant of "Don't understand that" message.
*/
int confuz()
{
    int msg;

    msg = 60;
    if (pct(50))
	msg = 61;
    if (pct(33))
	msg = 13;
    if (pct(25))
	msg = 347;
    if (pct(20))
	msg = 195;
    return (msg);
}

/*
  dark .TRUE. if there is no light here
*/
boolean dark()
{
    return (!(g.loc_attrib[g.loc] & LIGHT) &&
	    (!g.prop[LAMP] || !athand(LAMP)));
}

/*
  Routine to check for presence
  of dwarves..
*/
int dcheck()
{
    int i;

    for (i = 1; i < (DWARFMAX); ++i)
	if (g.dloc[i] == g.loc)
	    return (i);
    return (0);
}

/*
   dead .TRUE. if object is now dead
*/
boolean dead(obj)
int obj;
{
    return (bitset(g.obj_state[obj], 10));
}

/*
  drop Place an object at a given loc, prefixing it onto the atloc list.
*/
void drop(obj, where)
int obj, where;
{
    if (obj > MAXOBJ)
	g.fixed[obj - MAXOBJ] = where;
    else
	g.place[obj] = where;
    if (where > 0) {
	g.link[obj] = g.atloc[where];
	g.atloc[where] = obj;
    }
    return;
}

/*
  destroy Permanently eliminate "object" by moving it to
  a non-existent location.
*/
void destroy(obj)
int obj;
{
    move(obj, 0);
    return;
}

/*
   edible .TRUE. if obj can be eaten.
*/
boolean edible(obj)
int obj;
{
    return (bitset(g.obj_state[obj], 7));
}

/*
  enclosed .TRUE. If object is inside a container.
*/
boolean enclosed(item)
int item;
{
    if (item < 1 || item > MAXOBJ)
	return (FALSE);
    else
	return (g.place[item] < -1);
}

/*
   extract remove "object" from a container.
   origionally name "remove" but rename to avoid conflict with stdio.h
*/
void extract(obj)
int obj;
{
    int contnr, temp;

    contnr = -g.place[obj];
    g.place[obj] = -1;
    if (g.holder[contnr] == obj)
	g.holder[contnr] = g.hlink[obj];
    else {
	temp = g.holder[contnr];
	while (g.hlink[temp] != obj) {
	    temp = g.hlink[temp];
	    if (temp == 0)
		bug(35);
	}
	g.hlink[temp] = g.hlink[obj];
    }
    return;
}

/*
  forced To tell if a location will causes a forced move.
  A forced location is one from which he is immediately bounced
  to another.  Normal use is for death (forced to location zero)
  and for description of journey from on place to another.
*/
int forced(at_loc)
int at_loc;
{
    return ((g.loc_attrib[at_loc] & 10) == 2);
}

/*
  here .TRUE. If an item is at location or is being carried.
*/
boolean here(item)
int item;
{
    return (g.place[item] == g.loc || toting(item));
}

/*
  hinged .TRUE. If object can be opened or shut.
*/
boolean hinged(object)
int object;
{
    return (bitset(g.obj_state[object], 1));
}

/*
  holding .TRUE. If the object is being carried in hand.
*/
boolean holding(item)
int item;
{
    if (item < 1 || item > MAXOBJ)
	return (FALSE);
    else
	return (g.place[item] == -1);
}

/*
  insert
*/
void insert(obj, contnr)
int obj, contnr;
{
    int temp;

    if (contnr == obj)
	bug(32);
    carry(obj, g.loc);

    temp = g.holder[contnr];
    g.holder[contnr] = obj;
    g.hlink[obj] = temp;
    g.place[obj] = -contnr;
}

/*
  inside = .TRUE. If location is well within cave
*/
boolean inside(loc)
int loc;
{
    return (!outside(loc) && !portal(loc));
}

/*
  Juggle an object by picking it up and putting it down again,
  The purpose being to get the object to the front of the chain
  at its loc.
*/
void juggle(obj)
int obj;
{
    int i, j;

    i = g.place[obj];
    j = g.fixed[obj];
    move(obj, i);
    move(obj + MAXOBJ, j);
    return;
}

/*
  Determine liquid in the vessel
*/
int liq(item)
int item;
{
    int liquid;

    if ((item == BOTTLE) || (item == CASK))
	liquid = liq2(((int) g.prop[item] >> 1) & 7);
    else
	liquid = 0;

    return (liquid);
}

/*
  Determine type of liquid in vessel
*/
int liq2(liquid)
int liquid;
{
    switch (liquid) {
    case 4:
	return (WATER);
    case 5:
	return (OIL);
    case 6:
	return (WINE);
    default:
	return (0);			/* empty */
    }
}

/*
  Determine liquid at a location
*/
int liqloc(loc)
int loc;
{
    return (liq2((int) ((g.loc_attrib[loc] >> 1) & 7)));
}

/*
   living .TRUE. If object is living, bear for example
*/
boolean living(obj)
int obj;
{
    return (bitset(g.obj_state[obj], 9));
}

/*
   locked .TRUE. if lockable object is locked
*/
boolean locked(item)
int item;
{
    return (bitset(g.obj_state[item], 4));
}

/*
   locks .TRUE. if you can lock this object
*/
boolean locks(item)
int item;
{
    return (bitset(g.obj_state[item], 3));
}

/*
  LOOKIN list contents if obj is a container and is open or transparent.
*/
void lookin(contnr)
int contnr;
{
    int temp;
    boolean first_time;

    if (vessel(contnr) && (ajar(contnr) || !opaque(contnr))) {
	temp = g.holder[contnr];
	first_time = TRUE;
	while (temp != 0) {
	    if (first_time)
		rspeak(360);
	    printf("     ");
	    pspeak(temp, -1);
	    temp = g.hlink[temp];
	    first_time = FALSE;
	}
    }
    return;
}

/*
  Routine to move an object
*/
void move(obj, where)
int obj, where;
{
    int from;

    if (obj > MAXOBJ)
	from = g.fixed[obj - MAXOBJ];
    else {
	if (enclosed(obj))
	    extract(obj);
	from = g.place[obj];
    }
    if ((from > 0) && (from < MAXOBJ * 2))
	carry(obj, from);
    drop(obj, where);
    return;
}

/*
  noway, generate's some variant of "can't do that" message.
*/
int noway()
{
    int msg;

    msg = 14;
    if (pct(50))
	msg = 110;
    if (pct(33))
	msg = 147;
    if (pct(25))
	msg = 250;
    if (pct(20))
	msg = 262;
    if (pct(17))
	msg = 25;
    if (pct(14))
	msg = 345;
    if (pct(12))
	msg = 346;
    return (msg);
}

/*
  opaque .TRUE. If obj is non-transparent container
*/
boolean opaque(obj)
int obj;
{
    return (bitset(g.obj_state[obj], 6));
}

/*
   outsid .TRUE. If location is outside the cave
*/
boolean outside(loc)
int loc;
{
    return (bitset(g.loc_attrib[loc], 6));
}

/*
  Routine true x% of the time. (x an integer from 0 to 100)
*/
int pct(x)
int x;
{
    return (ranz(100) < x);
}

/*
   plural .TRUE. if object is multiple objects
*/
boolean plural(obj)
int obj;
{
    return (bitset(g.obj_state[obj], 13));
}

/*
   portal .TRUE. If location is a cave entrance
*/
boolean portal(loc)
int loc;
{
    return (bitset(g.loc_attrib[loc], 5));
}

/*
   printed .TRUE. If object can be read.
*/
boolean printed(obj)
int obj;
{
    return (bitset(g.obj_state[obj], 8));
}

/*
  put is the same as move, except it returns a
  value used to set the negated prop values
  for the repository objects.
*/
int put(obj, where, pval)
int obj, where, pval;
{
    move(obj, where);
    return ((-1) - pval);
}

/*
  RANZ
*/
int ranz(range)
int range;
{
    return (rand() % range);
}

/*
   small .TRUE. If object fits in sack or small container
*/
boolean small(obj)
int obj;
{
    return (bitset(g.obj_state[obj], 5));
}

/*
  toting .TRUE. If an item is being caried.
*/
int toting(item)
int item;
{
    boolean aaa, bbb, ccc;
    int contnr, outer, outer2;

    contnr = -g.place[item];
    outer = -g.place[contnr];
    outer2 = -g.place[outer];

    aaa = holding(contnr);
    bbb = enclosed(contnr) && holding(outer);
    ccc = enclosed(outer) && holding(outer2);

    return (holding(item) || (enclosed(item) && (aaa || bbb || ccc)));
}

/*
  treasr .TRUE. If object is valuable for points
*/
boolean treasr(obj)
int obj;
{
    return (bitset(g.obj_state[obj], 14));
}

/*
  vessel .TRUE. if object can hold a liquid
*/
boolean vessel(obj)
int obj;
{
    return (bitset(g.obj_state[obj], 15));
}

/*
  wearng .TRUE. If wearing obj
*/
boolean wearng(item)
int item;
{
    return (bitset(g.obj_state[item], WEARBT));
}

/*
   worn .TRUE. if object is being worn
*/
boolean worn(obj)
int obj;
{
    return (bitset(g.obj_state[obj], 11));
}

static char *e_msg[] = {
			"message line > 70 characters",	/* 00 */
			"null line in message",	/* 01 */
			"too many words of messages",	/* 02 */
			"too many travel options",	/* 03 */
			"too many vocabulary words",	/* 04 */
			"required vocabulary word not found",	/* 05 */
			"too many rtext or mtext messages",	/* 06 */
			"too many hints",	/* 07 */
			"location has loc_attrib bit being set twice",	/* 08 */
			"invalid section number in database",	/* 09 */
			"out of order locs or rspeak entries.",	/* 10 */
			"illegal motion word in travel table",	/* 11 */
			"** unused **.",/* 12 */
			"unknown or illegal word in adjective table.",	/* 13 */
			"illegal word in prep/obj table",	/* 14 */
			"too many entries in prep/obj table",	/* 15 */
			"object has condition bit set twice",	/* 16 */
			"object number too large",	/* 17 */
			"too many entries in adjective/noun table.",	/* 18 */
			"** unused **.",/* 19 */
			"special travel (500>l>300) exceeds goto list",	/* 20 */
			"ran off end of vocabulary table",	/* 21 */
			"verb class (n/1000) not between 1 and 3",	/* 22 */
			"intransitive action verb exceeds goto list",	/* 23 */
			"transitive action verb exceeds goto list",	/* 24 */
			"conditional travel entry with no alternative",	/* 25 */
			"location has no travel entries",	/* 26 */
			"hint number exceeds goto list",	/* 27 */
			"invalid month returned by date function",	/* 28 */
			"action verb 'leave' has no object.",	/* 29 */
			"preposition found in unexpected table",	/* 30 */
		 "received an unexpected word terminator from a1toa5",	/* 31 */
		    "trying to put a container into itself (tricky!)",	/* 32 */
			"unknown word class in getwds",	/* 33 */
			"** unused **.",/* 34 */
			"trying to carry a non-existent object"};	/* 35 */

/*
  Fatal error routine
*/
void bug(n)
unsigned int n;
{
    if (n < 36 && *e_msg[n] != '*')
	fprintf(stderr, "Fatal error, probable cause: %s\n", e_msg[n]);
    else
	fprintf(stderr, "Fatal error number %d - Unused error number!\n", n);
    panic((char *) 0, TRUE);
}

/*
  Prompt for input, strip leading and trailing spaces,
  return &buf[first non-whitespace].
  Does not return if end of input.
*/
char *
 ask(prompt, buf, buflen)
char *prompt, *buf;
int buflen;
{
    fputs(prompt, stdout);
    fflush(stdout);
    if (!fgets(buf, buflen, stdin))
	panic("end of input", FALSE);
    if (*buf) {
	int c;
	char *end = buf + strlen(buf);
	if (end[-1] != '\n')
	    /* Skip to end of line */
	    while ((c = getchar()) != '\n' && c != EOF);
	while (*buf && isspace(*buf))
	    buf++;
	while (buf <= --end && isspace(*end))
	    *end = '\0';
    }
    return buf;
}

/*
  save and abort
*/

void panic(msg, save)
char *msg;
boolean save;
{
    fprintf(stderr, "\nPANIC: %s%s\n",
	 msg ? msg : "", save ? ". Save..." : msg ? "" : "aborting.");
    if (save)
	saveadv("advpanic.sav");
    exit(EXIT_FAILURE);
}
