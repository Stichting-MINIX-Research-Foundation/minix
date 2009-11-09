/*	program ENGLISH.C					*/


#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>
#include	<stdlib.h>
#include	"advent.h"
#include	"advdec.h"

#define ALL	109

#define ENTER	 3
#define CRAWL	17
#define JUMP	39
#define CLIMB	56
#define XYZZY	62
#define PLUGH	65
#define PLOVER	71
#define PHUCE	82

_PROTOTYPE(static void getwords, (void));
_PROTOTYPE(static void clrlin, (void));
_PROTOTYPE(static void doobj, (int *));
_PROTOTYPE(static boolean doiobj, (void));
_PROTOTYPE(static boolean do_scoop_up, (void));
_PROTOTYPE(static boolean check_next, (void));

static char buffer[INPUTBUFLEN] = {'\0', '\0', '\0', '\0'};
static char *txt[MAXWORDS] = {buffer, buffer, buffer, buffer};
static char *cindex = buffer;
static boolean pflag;
static int vrbkey, words[MAXWORDS] = {0, 0, 0, 0}, word, wdx = 0;
static int takdir[20] = {2,  6,  9, 10, 11, 13, 14, 17, 23, 25,
			33, 34, 36, 37, 39, 78, 79, 80, 89, -1};

static int vkey[60] = {
     0,   199,     9,     0,   130,     0,   197,     0,     0,   243,
     0,     0,    89,   140,     0,     5,     0,   227,     0,     0,
     0,    31,    42,     0,     0,     0,     0,   172,     1,     0,
     0,     0,   254,     0,    69,     0,     0,    92,     0,     0,
   138,   137,   149,   239,    45,    74,   183,     0,     0,   112,
   241,     0,   114,     0,    30,     0,     0,     0,     0,     0
};

static int ptab[260] = {
     0,  3028,  3065,  3009, -3005,  5071,  5070,  5058, -5020, 19055,
 19108, 19038, 19020, 19071, 19070, 19058, 19004, 19048, 19091, 19094,
 19112, 19002, 19118,  2062,  2066,  2047,  2067,  2053,  2065, -2010,
 -3114,  4034,  4011,  4101,  4035,  4099,  4098,  4017,  4104,  4014,
  4015, -4087,  3083,  3085, -3081,  5055,  5108,  5020,  5071,  5070,
  5058,  5004,  5048,  5091,  5112,  5099,  5118, 19055, 19108, 19020,
 19071, 19070, 19058, 19004, 19048, 19091, 19112, 19099,-19118,  3028,
  3065,  3009,  3005, -3018, 19055, 19108, 19038, 19020, 19071, 19070,
 19058, 19004, 19004, 19048, 19091, 19094, 19112, 19002,-19118,  3028,
  3065, -3018, 19055, 19108, 19038, 19020, 19071, 19070, 19058, 19004,
 19048, 19091, 19094, 19112, 19118,  2062,  2066,  2047,  2067,  2053,
  2065, -2010,  3102, -3090, 19055, 19108, 19020, 19071, 19070, 19058,
 19004, 19048, 19091, 19014, 19015, 19112, 19118, 19120, 19120, -9999,
  3090,  3102,  3028,  3057,  3065,  3009, -3005,-29999,  2052, -2068,
  2024,  2065,  2091,  2042,  2073,  5071,  5070,  5058, -5020, 30999,
  2062,  2066,  2047,  2067,  2053,  2065,  2010,  2073, 19055, 19108,
 19038, 19020, 19071, 19070, 19058, 19004, 19048, 19091, 19094, 19112,
 19002,-19118,  2014,  2015,  2013,  2999,  5014,  5015,  5013,  5999,
  5110,  5113, -5999,  5055,  5108,  5020,  5071,  5070,  5058,  5004,
  5048,  5091,  5014,  5015,  5112,  5099, -5118,  3102, -3090,  6066,
  6047,  6067,  6053,  6072,  6073,  5055,  5108,  5020,  5071,  5070,
  5004,  5004,  5048,  5091,  5112,  5099,  5118, 19055, 19108, 19020,
 19071, 19070, 19058, 19004, 19048, 19091,-19118,  4034,  4011,  4101,
  4035,  4099,  4098,  4017,  4104,  4027,  4087,  9999,-30999,  2002,
 -6002,  3102, -3090,  9999,  4034,  4011,  4101,  4035,  4099,  4087,
  4098,  4017,  4104, -4027, -5999,     0,     0,     0,     0,     0,
};

static int adjkey[40] = {
     0,    15,    38,    64,     4,    63,     1,    61,    62,    67,
     9,    27,    53,    46,    47,    60,    31,    39,    40,     6,
    43,    26,    32,    28,    34,    50,    49,    45,    44,    10,
    20,    25,    21,    36,    37,    30,    33,     0,     0,     0
};

static int adjtab[70] = {
     0,     5,    98,   -83,     2,   -90,    66,    41,   -90,   -39,
    41,    14,    15,    50,   -11,    50,    64,    56,    72,   -74,
   -19,   119,    59,    73,  -118,  -119,   -70,   -41,    95,  -118,
  -118,   -58,   -71,  -120,   110,  -108,  -120,   -73,   -62,   -60,
   110,    54,   -63,   -67,   -41,   -27,   -47,    52,   -75,   -69,
    65,   112,    -3,    41,    72,    90,    20,   101,   107,  -118,
   -55,   -10,   -38,    -4,    48,     9,   -71,   -39,     0,     0
};

/*
  Analyze a two word sentence
*/
int english()
{

    char *ch_ptr, *word1, *word2;
    int type, val, type2, val2, adj, k, kk;
    static int iwest = 0;

    if (!(words[++wdx])) {
	getwords();
	wdx = 0;
    }
    pflag = FALSE;
    word = words[wdx];
    if (word < 0) {			/* check first word	 */
	printf("I didn't understand the word \"%s\"\n", txt[wdx]);
	words[wdx+1] = 0;
	return (FALSE);			/* didn't know it	 */
    }
    type2 = val2 = -1;
    type = CLASS(word);
    clrlin();
    val = VAL(word);
    if (words[wdx + 1] && CLASS(words[wdx + 1]) != CONJUNCTION) {

	/* 'SAY' or 'CALL'.  If no next word, pass on to higher powers. */
	if (type == ACTION && (val == SAY || val == YELL)) {
	    word = words[++wdx];
	    if (!(word == XYZZY || word == PLUGH
		  || word == PLOVER || word == PHUCE)) {
		if (val == SAY)
		    printf("Okay, \"%s\".\n", txt[wdx]);
		else {
		    for (ch_ptr = txt[wdx]; *ch_ptr; ch_ptr++)
			if (islower(*ch_ptr))
			    *ch_ptr = toupper(*ch_ptr);
		    printf("Okay, \"%s\"!!!!!\n", txt[wdx]);
		}
		return (FALSE);
	    }
	} else {
	    word1 = txt[wdx];
	    word2 = txt[wdx + 1];

	    /* Special stuff for 'ENTER'.  Can't go into water. 'ENTER
	       BOAT' means 'TAKE BOAT' */
	    if (word == ENTER) {
		if (CLASS(words[wdx + 1]) == NOUN && VAL(words[wdx + 1]) == BOAT)
		    word = TAKE + 2000;
		else if ((strcmp(word2, "stream") == 0)
			 || (strcmp(word2, "water") == 0)
			 || (strcmp(word2, "reservoir") == 0)
			 || (strcmp(word2, "ocean") == 0)
			 || (strcmp(word2, "sea") == 0)
			 || (strcmp(word2, "pool") == 0)) {
		    rspeak(liqloc(g.loc) == WATER ? 70 : 43);
		    wdx++;
		    return (FALSE);
		}
	    } else {
		type2 = CLASS(words[wdx + 1]);
		val2 = VAL(words[wdx + 1]);

		/* 'LEAVE' is motion verb, unsless leaving an object.
		   E.G., 'LEAVE BOAT' or 'LEAVE BOTTLE'.  BUt make sure
		   to leave ('DROP') only totable objects. */
		if (strcmp(word1, "leave") == 0 && type2 == NOUN) {
		    if (!hinged(val2) || g.fixed[val2])
			word = LEAVE + 2000;

		    /* IF 'LIGHT LAMP', Light must be taken as an
		       action verb, not a noun. */
		} else if (strcmp(word1, "light") == 0
			   && VAL(words[wdx + 1]) == LAMP) {
		    word = ON + 2000;

		    /* 'WATER PLANT' becomes 'POUR WATER', If we are at
		       plant. 'OIL DOOR' becomes 'POUR OIL', etc., etc. */
		} else if ((strcmp(word1, "water") == 0 || strcmp(word1, "oil") == 0)
			   && (strcmp(word2, "plant") == 0 || strcmp(word2, "door") == 0
			       || strcmp(word2, "sword") == 0 || strcmp(word2, "anvil") == 0)
			   && at(val2)) {
		    words[wdx + 1] = word;
		    txt[wdx + 1] = txt[wdx];
		    word = POUR + 2000;
		}
	    }
	}

    }
    /* This is the 'inner' loop.  Dispatching of all word in a clause
       after the first comes through here. */
    do {
	switch (CLASS(word)) {
	case MOTION:
	    {
		boolean do_part2;
		int i;

		do_part2 = FALSE;
		type = CLASS(verbs[vrbx]);
		val = VAL(verbs[vrbx]);
		if (!vrbx)
		    do_part2 = TRUE;
		else {
		    if (type > ACTION) {
			rspeak(confuz());
			return (FALSE);
		    }
		}
		if (type == ACTION) {
		    if (val == GO)
			do_part2 = TRUE;
		    else {
			if (val == TAKE) {
			    for (i = 0; i < 20; i++)
				if (takdir[i] == val)
				    do_part2 = TRUE;
			}
			if (!do_part2) {
			    word = vocab(txt[wdx], 1);
			    if (word)
				words[wdx--] = word;
			}
		    }
		} else if (type != CRAWL && type != JUMP
			   && type != CLIMB)
		    do_part2 = TRUE;
		if (do_part2) {
		    verbs[1] = word;
		    vrbx = 1;
		    if (strcmp(txt[wdx], "west") == 0) {
			iwest++;
			if (iwest == 10)
			    rspeak(17);
		    }
		}
		break;
	    }
	case NOUN:
	    if (pflag) {
		if (!doiobj())
		    return (FALSE);
	    } else {
		word = VAL(word);
		if (word == ALL) {
		    if (!do_scoop_up())
			return (FALSE);
		} else {
		    doobj(&word);
		    if (word > 0) {
			objs[++objx] = word;
			otxt[objx] = txt[wdx];
		    } else {
			clrlin();
			pflag = FALSE;
			wdx++;
			while (words[wdx]) {
			    if (CLASS(words[wdx]) == CONJUNCTION)
				break;
			    wdx++;
			}
			if (words[wdx] == 0)
			    return (FALSE);
		    }
		}
	    }
	    break;
	case ACTION:
	    if (vrbx == 0)
		vrbx++;
	    else {
		if (VAL(verbs[vrbx]) == TAKE) {
		    val = VAL(word);
		    if (val == DRINK || val == INVENTORY
			|| val == SCORE || val == NOTHING
			|| val == LOOK);
		    else if (val == GO && (
					 strcmp(txt[wdx], "walk") == 0
				       || strcmp(txt[wdx], "run") == 0
				   || strcmp(txt[wdx], "hike") == 0));
		    else {
			rspeak(confuz());
			return (FALSE);
		    }
		} else if (objx || CLASS(words[wdx - 1]) == CONJUNCTION) {
		    rspeak(confuz());
		    return (FALSE);
		}
	    }
	    verbs[vrbx] = word;
	    vtxt[vrbx] = txt[wdx];
	    break;
	case MISC:
	    if (vrbx) {
		rspeak(confuz());
		return (FALSE);
	    }
	    verbs[1] = word;
	    vrbx = 1;
	    break;
	case PREPOSITION:
	    if (CLASS(verbs[vrbx]) != ACTION || iobx) {
		rspeak(confuz());
		return (FALSE);
	    }
	    vrbkey = vkey[VAL(verbs[vrbx])];
	    if (!vrbkey) {
		rspeak(confuz());
		return (FALSE);
	    }
	    prep = VAL(word);
	    pflag = TRUE;
	    break;
	case ADJACTIVE:
	    /* Adjective handler. Scarf the next word, make sure it is
	       a valid object for this object.  Then call getobj to see
	       if it is really there, Then link into object code. */
	    adj = VAL(word);
	    if (!check_next())
		return (FALSE);
	    else if (CLASS(word) == CONJUNCTION) {
		printf("%s what?\n", txt[wdx - 1]);
		return (FALSE);
	    } else {
		if (CLASS(word) != NOUN)
		    word = vocab(txt[wdx], NOUN);
		if (word == -1 || CLASS(word) != NOUN || VAL(word) == ALL) {
		    rspeak(confuz());
		    return (FALSE);
		}
		words[wdx] = word;
		kk = VAL(word);
		for (k = adjkey[adj]; adjtab[k] >= 0; k++) {
		    if (kk == abs(adjtab[k]))
			break;
		}
		if (adjtab[k] < 0) {
		    rspeak(confuz());
		    return (FALSE);
		}
	    }
	    break;
	case CONJUNCTION:
	    if (!check_next())
		return (FALSE);
	    switch (CLASS(word)) {
	    case MOTION:
	    case ACTION:
	    case MISC:
		words[wdx--] = 0;
		break;
	    case NOUN:
	    case ADJACTIVE:
		break;
	    case PREPOSITION:
	    case CONJUNCTION:
		rspeak(confuz());
		return (FALSE);
	    default:
		bug(33);
	    }
	    break;
	default:
	    bug(33);
	}
	word = words[++wdx];
	if (word < 0) {
	    if (pct(50))
		printf("I don't understand the word %s?\n", txt[wdx]);
	    else
		printf("Mumble ?  %s\n", txt[wdx]);

	    words[wdx+1] = 0;
	    return (FALSE);
	}
	type = CLASS(word);
	if (type == NOUN) {
	    /* It's not the first:  Make sure he included a comma or
	       'and'. Differenctiate between direct & indirect objects.
	       Check for special case of multiple ofjects: 'feed bear
	       honey' or 'throw troll nugget'. */
	    if ((pflag ? iobx : objx)
		&& CLASS(words[wdx - 1]) != CONJUNCTION) {
		val = VAL(verbs[vrbx]);
		if (!living(objs[objx]) || (val != THROW && val != FEED)) {
		    rspeak(confuz());
		    return (FALSE);
		}
		iobx++;
		iobjs[iobx] = objs[objx];
		objs[objx] = 0;
		objx++;
	    }
	}
    } while (word);

    if (verbs[1] == 0) {
	if (objs[1] == 0) {
	    rspeak(confuz());
	    clrlin();
	} else if (objs[2])
	    printf("What do you want to do with them?\n");
	else
	    printf("What do you want to do with %s?\n", otxt[1]);
	return (FALSE);
    } else if (objx > 1 && iobx > 1) {
	rspeak(confuz());
	return (FALSE);
    }
    return (TRUE);

}

/*
  retrieve input line (max INPUTBUFLEN chars), convert to lower case
   & rescan for first two words (max. WORDSIZE-1 chars).
*/
static void getwords()
{
    static int wdx = 0;
    int i, term_loc;
    char terminator;

    if (*cindex == '\0') {
	while (!*ask("\n> ", buffer, sizeof(buffer))) ;
	for (cindex = buffer; *cindex; cindex++)
	    if (isupper(*cindex))
		*cindex = tolower(*cindex);
	cindex = buffer;
    }
    wdx = 0;
    buffer[sizeof(buffer)-1] = '\0';
    for (i = 0; i < MAXWORDS; i++) {
	txt[i] = &buffer[sizeof(buffer)-1];
	words[i] = 0;
    }
    do {
	while (*cindex == ' ')
	    cindex++;
	txt[wdx] = cindex;
	term_loc = strcspn(cindex, " ,.;\n");
	cindex += term_loc;
	terminator = *cindex;
	*cindex++ = '\0';
	if ((strcmp(txt[wdx], "a") != 0)
	    && (strcmp(txt[wdx], "the") != 0)
	    && (strcmp(txt[wdx], "an") != 0)) {
	    words[wdx] = vocab(txt[wdx], 0);
	    wdx++;
	}
	if (terminator == ',') {
	    txt[wdx] = "and";
	    words[wdx] = vocab(txt[wdx], 0);
	    wdx++;
	}
    }
    while ((terminator != ';') && (terminator != '.')
	   && (terminator != '\0') && (terminator != '\n'));
    if (terminator == '\0')
	cindex--;
    return;
}

/* CLRIN, clears out all surrent syntax args in preparation for
 * new input line
 */

static void clrlin()
{
    int i;

    for (i = 0; i < MAXWORDS; i++) {
	verbs[i] = 0;
	vtxt[i] = &buffer[sizeof(buffer)-1];
    }

    for (i = 0; i < MAXITEMS; i++) {
	objs[i] = 0;
	otxt[i] = &buffer[sizeof(buffer)-1];
	iobjs[i] = 0;
	iotxt[i] = &buffer[sizeof(buffer)-1];
    }
    vrbx = 0;
    objx = 0;
    iobx = 0;
    prep = 0;
}

/*
  Routine to process an object.
*/
static void doobj(object)
int *object;
{
    int msg;

    if (holding(*object))
	return;
    if (blind()) {
	printf("I see no %s here.\n", txt[wdx]);
	*object = 0;
	return;
    }
    /* Is object here?  if so, transitive */
    if (g.fixed[*object] == g.loc || athand(*object))
	return;
    else if (here(*object)) {
	msg = plural(*object) ? 373 : 335;
	*object = 0;
	rspeak(msg);
    }
    /* Did he give grate as destination? */
    else if (*object == GRATE) {
	if (g.loc == 1 || g.loc == 4 || g.loc == 7) {
	    verbs[1] = DEPRESSION;
	    vrbx = 1;
	    return;
	} else if (g.loc > 9 && g.loc < 15) {
	    verbs[1] = ENTRANCE;
	    vrbx = 1;
	    return;
	}
    }
    /* Is it a dwarf he is after? */
    else if (dcheck() && g.dflag >= 2) {
	*object = DWARF;
    }
    /* Is he trying to get/use a liquid? */
    else if (liqloc(g.loc) == *object
	     || (liq(BOTTLE) == *object && athand(BOTTLE))
	     || (liq(CASK) == *object && athand(CASK)));
    else if (*object == PLANT && at(PLANT2) &&
	     g.prop[PLANT2] == 0) {
	*object = PLANT2;
    } else if (*object == ROCKS && at(CARVNG)) {
	*object = CARVNG;
    }
    /* Is he trying to grab a knife? */
    else if (*object == KNIFE && g.knfloc == g.loc) {
	rspeak(116);
	g.knfloc = -1;
    }
    /* Is he trying to get at dynamite? */
    else if (*object == ROD && athand(ROD2)) {
	*object = ROD2;
    } else if (*object == DOOR && (at(SAFE) || at(TDOOR)
				   || at(TDOOR2) || at(PDOOR))) {
	if (at(TDOOR2))
	    *object = TDOOR2;
	else if (at(PDOOR))
	    *object = PDOOR;
	else if (at(SAFE))
	    *object = SAFE;
	else
	    *object = TDOOR;
    } else if (*object == BOOK && athand(BOOK2)) {
	*object = BOOK2;
    } else if (!(verbs[vrbx] == FIND || verbs[vrbx] == INVENTORY)) {
	*object = 0;
	printf("I see no %s here.\n", txt[wdx]);
    }
    return;
}

static boolean doiobj()
{
    char dk[INPUTBUFLEN], dkk[INPUTBUFLEN];
    int kk;
    boolean ok;

    /* checks object is valid for this preposition */
    ok = TRUE;
    word = VAL(word);
    if (word != ALL) {
	doobj(&word);
	if (word > 0) {
	    iobjs[++iobx] = word;
	    iotxt[iobx] = txt[wdx];
	} else
	    ok = FALSE;
    }
    kk = abs(ptab[vrbkey]) / 1000;
    if (kk == prep) {
	/* preprosition is valid with this verb now check object of
	   preprosition */

	if (word == 0 || CLASS(word) == CONJUNCTION) {
	    /* no object following prepresition: check special cases */

	    pflag = FALSE;
	    strcpy(dk, txt[--wdx]);
	    strcpy(dkk, vtxt[vrbx]);
	    ok = FALSE;
	    if ((strcmp(dk, "on") == 0
		 || strcmp(dk, "off") == 0)
		&& (strcmp(dkk, "turn") == 0
		    || objs[objx] == LAMP))
		ok = TRUE;
	    if (strcmp(dkk, "take") == 0
		|| strcmp(dkk, "put") == 0)
		ok = TRUE;
	    if (strcmp(dk, "up") == 0
		&& strcmp(dkk, "pick") == 0)
		ok = TRUE;
	    if (strcmp(dk, "down") == 0
		 && (strcmp(dkk, "put") == 0 || verbs[vrbx] == THROW) )
		ok = TRUE;
	} else {
	    /* object follows preposition See if it's plausible. */

	    kk = abs(ptab[vrbkey]) % 1000;
	    if (kk == word && kk == ALL) {
		if (!do_scoop_up())
		    return (FALSE);
	    } else if (!(kk == word || kk == 999)) {
		vrbkey++;
		ok = ptab[vrbkey - 1] < 0 ? FALSE : TRUE;
	    }
	}
    }
    return (ok);
}

static boolean do_scoop_up()
{
    int i, val;

    val = VAL(verbs[vrbx]);
    if (val == DROP || val == PUT || val == LEAVE) {
	for (i = 1; i < MAXOBJ; i++) {
	    if (!athand(i) || g.fixed[i])
		continue;
	    if (i > WATER && i <= WINE + 1)
		continue;
	    if (toting(i)) {
		objs[++objx] = i;
		otxt[objx] = "BUG???";
		if (objx >= 44)
		    break;
	    }
	}
    }
    if (val == TAKE || val == PICK || val == GET) {
	if (blind()) {
	    rspeak(357);
	    return (FALSE);
	} else {
	    for (i = 1; i < MAXOBJ; i++) {
		if (!athand(i) || g.fixed[i])
		    continue;
		if (i > WATER && i <= WINE + 1)
		    continue;
		if (!toting(i)) {
		    objs[++objx] = i;
		    otxt[objx] = "BUG???";
		    if (objx >= 44)
			break;
		}
	    }
	}
    }
    return (TRUE);
}

static boolean check_next()
{

    word = words[wdx + 1];
    if (word > 0)
	return (TRUE);
    else if (word == 0)
	rspeak(confuz());
    else {
	if (pct(50))
	    printf("I don't understand the word %s?\n", txt[wdx]);
	else
	    printf("Mumble ?  %s\n", txt[wdx]);
	words[wdx+1] = 0;
    }

    return (FALSE);
}
