/*	header ADVENT.H						*
 *	WARNING: HEADER file for all adventure modules		*/

#ifndef EXIT_FAILURE
#define EXIT_FAILURE	1
#define EXIT_SUCCESS	(!(EXIT_FAILURE))
#endif

#define INPUTBUFLEN	80	/* Max input line length	 */

typedef int boolean;
#define FALSE	(0)
#define TRUE	(!FALSE)

#define	MAXOBJ	123		/* max # of objects in cave	 */
#define	MAXLOC	248		/* max # of cave locations	 */
#define	WORDSIZE	20	/* max # of chars in commands	 */
#define	MAXMSG	408		/* max # of long location descr	 */
#define HNTMAX	 18		/* max # of hints		 */
#define HNTMIN	  7		/* hints starting count		 */

#define MAXWORDS 25
#define MAXITEMS 45

#define CLASS(word) ((word)<0 ? -((-(word)) / 1000) : (word) / 1000)
#define VAL(word) ((word)<0 ? -((-(word)) % 1000) : (word) % 1000)
#define	MAXTRAV	(23+1)		/* max # of travel directions from loc	 */
 /* +1 for terminator travel[x].tdest=-1	 */
#define	DWARFMAX	6	/* max # of nasty dwarves	 */
#define	MAXDIE	3		/* max # of deaths before close	 */
#define	MAXTRS	79		/* max # of			 */

#define Y2	33
/*
  Object definitions
*/
#define ANVIL	91
#define AXE	28
#define BATTERIES 39
#define BEAR	35
#define BEES	87
#define BILLBD	116
#define BIRD	101
#define BOAT	48
#define BOOK	110
#define BOOK2	BOOK + 1
#define BOOTH	93
#define BOTTLE	20
#define BRUSH	114
#define	CAGE	4
#define CAKES	107
#define CARVNG	115
#define CASK	71
#define CHAIN	64
#define CHASM	21
#define CHASM2	CHASM + 1
#define CHEST	55
#define	CLAM	14
#define CLOAK	47
#define COINS	54
#define CROWN	66
#define DOG	98
#define	DOOR	41		/* giant door */
#define DRAGON	31
#define DWARF	17
#define EGGS	56
#define EMERALD 59
#define FISSURE 12
#define FLOWER	46
#define FLY	69
#define FOOD	19
#define GNOME	105
#define GRAIL	70
#define GRATE	3
#define HIVE	97
#define HONEY	96
#define HORN	52
#define JEWELS	53
#define KEYS	102
#define KNIFE	18
#define LAMP	2
#define LYRE	68
#define MAGAZINE 16
#define MESSAGE 36
#define MIRROR	23
#define MUSHRM	106
#define NUGGET	50
#define OIL	83
#define OIL2	OIL + 1
#define OYSTER	15
#define PLAGUE	125
#define PEARL	61
#define PHONE	94
#define PILLOW	10
#define PLANT	24
#define PLANT2	PLANT + 1
#define POLE	9
#define POSTER	113
#define PYRAMID 60
#define RADIUM	119
#define RING	72
#define ROCKS	92
#define ROD	5
#define ROD2	ROD + 1
#define RUG	62
#define	SAFE	112
#define SAPPHIRE 69
#define SHIELD	118
#define SHOES	67
#define SKEY	90
#define SLUGS	95
#define SNAKE	11
#define SPHERE	120
#define SPICES	63
#define SPIDER	121
#define STEPS	7
#define STICKS	49
#define SWORD	65
#define TABLET	13
#define TDOOR	42		/* tiny door */
#define TDOOR2	TDOOR + 1	/* wrought-iron door */
#define PDOOR	TDOOR2 + 1	/* door to phone booth */
#define TRIDENT	57
#define TROLL	33
#define TROLL2	TROLL + 1
#define VASE	58
#define VEND	38
#define WALL	88
#define WALL2	WALL + 1
#define WATER	81		/* in bottle */
#define WATER2	WATER + 1	/* in cask */
#define	WINE	85		/* in bottle */
#define WINE2	WINE + 1	/* in cask */
#define WUMPUS	99

/*
  Verb definitions
*/
#define	BACK	8
#define	CAVE	67
#define	DEPRESSION	63
#define	ENTRANCE	64
#define EXIT	11
#define	NULLX	21

/*
  Action verb definitions
*/
#define TAKE	1
#define DROP	2
#define SAY	3
#define OPEN	4
#define NOTHING 5
#define CLOSE   6
#define ON	7
#define OFF	8
#define WAVE	9
#define CALM	10
#define WALK	11
#define KILL	12
#define POUR	13
#define EAT	14
#define DRINK	15
#define RUB	16
#define	THROW	17
#define QUIT	18
#define FIND	19
#define INVENTORY 20
#define FEED	21
#define FILL	22
#define BLAST	23
#define SCORE	24
#define FOO	25
#define BRIEF	26
#define READ	27
#define BREAK	28
#define WAKE	29
#define SUSPEND 30
#define RESTORE	31
#define YANK	32
#define WEAR	33
#define HIT	34
#define ANSWER 35
#define BLOW	36
#define LEAVE	37
#define YELL	38
#define DIAL	39
#define PLAY	40
#define PICK	41
#define PUT	42
#define TURN	43
#define GET	44
#define INSRT	45
#define REMOVE  46
#define BURN	47
#define GRIPE	48
#define LOCK	49
#define UNLOCK	50
#define HEALTH	51
#define LOOK	52
#define COMBO	53
#define SWEEP	54
#define TERSE	55
#define	WIZ	56
#define MAP	57
#define GATE	58
#define PIRLOC	59

#define GO	11
#define SHUT    6
#define LOG     33

#define MOTION	0			/* CLASSD */
#define NOUN	1			/* CLASSN */
#define ACTION	2			/* CLASSA */
#define MISC	3			/* CLASSM */
#define PREPOSITION 4			/* CLASSP */
#define ADJACTIVE   5			/* CLASSJ */
#define CONJUNCTION 6			/* CLASSC */

/*
   and a few preposition.  prefix PREP to distinguish them from
   verbs or nouns
 */
#define PREPAT	9
#define PREPDN	8
#define PREPIN	1
#define PREPFR  5
#define PREPOF  6
#define PREPOFF 6
#define PREPON	2

/*
  BIT mapping of "cond" array which indicates location status
*/
#define	LIGHT	1
#define	WATOIL	2
#define	LIQUID	4
#define	NOPIRAT	16

/* Object condition bit functions */
#define OPENBT 2
#define LOCKBT 4
#define BURNBT 6
#define DEADBT 10
#define WEARBT 12
/*
  Structure definitions
*/
struct wac {
  char *aword;
  int acode;
};

struct trav {
  int tdest;
  int tverb;
  int tcond;
};

/* Function prototypes.
   "#if (__STDC__)" should have been be enough,
   but some compilers are stupid, so allow Makefile to say -DHAS_STDC=whatever.
*/
#if defined(HAS_STDC) ? (HAS_STDC) : (__STDC__)
#undef	HAS_STDC
#define HAS_STDC 1
#define	_PROTOTYPE(function, params)	function params
#define _CONST				const
#else
#define	_PROTOTYPE(function, params)	function ()
#define _CONST
#endif

/* Advent.c */

_PROTOTYPE(void saveadv, (char *username));
_PROTOTYPE(void restore, (char *username));

/* Initialize.c */

_PROTOTYPE(void initialize, (void));

/* Database.c */

_PROTOTYPE(int yes, (int msg1, int msg2, int msg3));
_PROTOTYPE(void rspeak, (int msg));
_PROTOTYPE(void pspeak, (int item, int state));
_PROTOTYPE(void desclg, (int loc));
_PROTOTYPE(void descsh, (int loc));

/* English.c */

_PROTOTYPE(int english, (void));
_PROTOTYPE(int analyze, (char *word, int *type, int *value));

/* Itverb.c */

_PROTOTYPE(void itverb, (void));
_PROTOTYPE(void ivblast, (void));
_PROTOTYPE(void ivlook, (void));

/* Turn.c */

_PROTOTYPE(void turn, (void));
_PROTOTYPE(void describe, (void));
_PROTOTYPE(void descitem, (void));
_PROTOTYPE(void dwarfend, (void));
_PROTOTYPE(void normend, (void));
_PROTOTYPE(void score, (int));
_PROTOTYPE(void death, (void));
_PROTOTYPE(char *probj, (void));
_PROTOTYPE(void trobj, (void));
_PROTOTYPE(void dwarves, (void));
_PROTOTYPE(void dopirate, (void));
_PROTOTYPE(int stimer, (void));

/* Verb.c */

_PROTOTYPE(void trverb, (void));
_PROTOTYPE(void vtake, (void));
_PROTOTYPE(void vdrop, (void));
_PROTOTYPE(void vopen, (void));
_PROTOTYPE(void vsay, (void));
_PROTOTYPE(void von, (void));
_PROTOTYPE(void voff, (void));
_PROTOTYPE(void vwave, (void));
_PROTOTYPE(void vkill, (void));
_PROTOTYPE(void vpour, (void));
_PROTOTYPE(void veat, (void));
_PROTOTYPE(void vdrink, (void));
_PROTOTYPE(void vthrow, (void));
_PROTOTYPE(void vfind, (void));
_PROTOTYPE(void vfill, (void));
_PROTOTYPE(void vfeed, (void));
_PROTOTYPE(void vread, (void));
_PROTOTYPE(void vbreak, (void));
_PROTOTYPE(void vwake, (void));
_PROTOTYPE(void actspk, (int verb));
_PROTOTYPE(void vyank, (void));
_PROTOTYPE(void vwear, (void));
_PROTOTYPE(void vlock, (void));
_PROTOTYPE(void vunlock, (void));
_PROTOTYPE(void vclose, (void));

/* Utility.c */

_PROTOTYPE(boolean ajar, (int));
_PROTOTYPE(boolean at, (int item));
_PROTOTYPE(boolean athand, (int));
_PROTOTYPE(void bitoff, (int, int));
_PROTOTYPE(void biton, (int, int));
_PROTOTYPE(boolean bitset, (long, int));
_PROTOTYPE(boolean blind, (void));
_PROTOTYPE(int burden, (int));
_PROTOTYPE(void carry, (int obj, int where));
_PROTOTYPE(int confuz, (void));
_PROTOTYPE(boolean dark, (void));
_PROTOTYPE(boolean dcheck, (void));
_PROTOTYPE(boolean dead, (int));
_PROTOTYPE(void drop, (int obj, int where));
_PROTOTYPE(void destroy, (int obj));
_PROTOTYPE(boolean edible, (int));
_PROTOTYPE(boolean enclosed, (int));
_PROTOTYPE(void extract, (int));
_PROTOTYPE(boolean forced, (int atloc));
_PROTOTYPE(boolean here, (int item));
_PROTOTYPE(boolean hinged, (int));
_PROTOTYPE(boolean holding, (int));
_PROTOTYPE(void insert, (int, int));
_PROTOTYPE(boolean inside, (int));
_PROTOTYPE(void juggle, (int loc));
_PROTOTYPE(int liq, (int));
_PROTOTYPE(int liqloc, (int loc));
_PROTOTYPE(int liq2, (int pbottle));
_PROTOTYPE(boolean living, (int));
_PROTOTYPE(boolean locked, (int));
_PROTOTYPE(boolean locks, (int));
_PROTOTYPE(void lookin, (int));
_PROTOTYPE(void move, (int obj, int where));
_PROTOTYPE(int noway, (void));
_PROTOTYPE(boolean opaque, (int));
_PROTOTYPE(boolean outside, (int));
_PROTOTYPE(boolean pct, (int x));
_PROTOTYPE(boolean plural, (int));
_PROTOTYPE(boolean portal, (int));
_PROTOTYPE(boolean printed, (int));
_PROTOTYPE(int put, (int obj, int where, int pval));
_PROTOTYPE(int ranz, (int));
_PROTOTYPE(boolean small, (int));
_PROTOTYPE(boolean toting, (int item));
_PROTOTYPE(boolean treasr, (int));
_PROTOTYPE(boolean vessel, (int));
_PROTOTYPE(boolean wearng, (int));
_PROTOTYPE(boolean worn, (int));
_PROTOTYPE(void bug, (unsigned int n));
_PROTOTYPE(char *ask, (char *prompt, char *buf, int buflen));
_PROTOTYPE(void panic, (char *msg, boolean save));

/* travel.c */
_PROTOTYPE(void domove, (void));
_PROTOTYPE(void gettrav, (int loc, struct trav *travel));

/* vocab.c */
_PROTOTYPE(int vocab, (char *word, int val));
