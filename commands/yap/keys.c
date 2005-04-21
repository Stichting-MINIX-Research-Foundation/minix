/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _KEYS_

# include <ctype.h>
# include "in_all.h"
# include "machine.h"
# include "keys.h"
# include "commands.h"
# include "prompt.h"
# include "assert.h"

char defaultmap[] = "\
bf=P:bl=k:bl=^K:bl=^[[A:bot=l:bot=$:bot=^[[Y:bp=-:bp=^[[V:bs=^B:bse=?:bsl=S:\
bsp=F:chm=X:exg=x:ff=N:fl=^J:fl=^M:fl=j:fl=^[[B:fp= :fp=^[[U:fs=^D:fse=/:\
fsl=s:fsp=f:hlp=h:nse=n:nsr=r:red=^L:rep=.:bps=Z:bss=b:fps=z:fss=d:shl=!:\
tom=':top=\\^:top=^[[H:vis=e:wrf=w:qui=q:qui=Q:mar=m:pip=|";

char *strcpy();
char *strcat();
char *getenv();

/*
 * Construct an error message and return it
 */

STATIC char *
kerror(key, emess) char *key, *emess; {
	static char ebuf[80];	/* Room for the error message */

	(VOID) strcpy(ebuf, key);
	(VOID) strcat(ebuf, emess);
	return ebuf;
}

/*
 * Compile a keymap into commtable. Returns an error message if there
 * is one
 */

STATIC char *
compile(map, commtable)
  register char *map; register struct keymap *commtable; {
	register char *mark;	/* Indicates start of mnemonic */
	register char *c;	/* Runs through buf */
	register int temp;
	char *escapes = commtable->k_esc;
	char buf[10];		/* Will hold key sequence */

	(VOID) strcpy(commtable->k_help,"Illegal command");
	while (*map) {
		c = buf;
		mark = map;	/* Start of mnemonic */
		while (*map && *map != '=') {
			map++;
		}
		if (!*map) {
			/*
			 * Mnemonic should end with '='
			 */
			return kerror(mark, ": Syntax error");
		}
		*map++ = 0;
		while (*map) {
			/*
			 * Get key sequence
			 */
			if (*map == ':') {
				/*
				 * end of key sequence
				 */
				map++;
				break;
			}
			*c = *map++ & 0177;
			if (*c == '^' || *c == '\\') {
				if (!(temp = *map++)) {
					/*
					 * Escape not followed by a character
					 */
					return kerror(mark, ": Syntax error");
				}
				if (*c == '^') {
					if (temp == '?') *c = 0177;
					else *c = temp & 037;
				}
				else *c = temp & 0177;
			}
			setused(*c);
			c++;
			if (c >= &buf[9]) {
				return kerror(mark,": Key sequence too long");
			}
		}
		*c = 0;
		if (!(temp = lookup(mark))) {
			return kerror(mark,": Nonexistent function");
		}
		if (c == &buf[1] && (commands[temp].c_flags & ESC) &&
		    escapes < &(commtable->k_esc[sizeof(commtable->k_esc)-1])) {
			*escapes++ = buf[0] & 0177;
		}
		temp = addstring(buf, temp, &(commtable->k_mach));
		if (temp == FSM_ISPREFIX) {
			return kerror(mark,": Prefix of other key sequence");
		}
		if (temp == FSM_HASPREFIX) {
			return kerror(mark,": Other key sequence is prefix");
		}
		assert(temp == FSM_OKE);
		if (!strcmp(mark, "hlp")) {
			/*
			 * Create an error message to be given when the user
			 * types an illegal command
			 */
			(VOID) strcpy(commtable->k_help, "Type ");
			(VOID) strcat(commtable->k_help, buf);
			(VOID) strcat(commtable->k_help, " for help");
		}
	}
	*escapes = 0;
	return (char *) 0;
}

/*
 * Initialize the keymaps
 */

VOID
initkeys() {
	register char *p;
	static struct keymap xx[2];

	currmap = &xx[0];
	othermap = &xx[1];
	p = compile(defaultmap, currmap);	/* Compile default map */
	assert(p == (char *) 0);
	p = getenv("YAPKEYS");
	if (p) {
		if (!(p = compile(p, othermap))) {
			/*
			 * No errors in user defined keymap. So, use it
			 */
			do_chkm(0L);
			return;
		}
		error(p);
	}
	othermap = 0;		       /* No other keymap */
}

int
is_escape(c)
{
	register char *p = currmap->k_esc;

	while (*p) {
		if (c == *p++) return 1;
	}
	return 0;
}

static char keyset[16];		/* bitset indicating which keys are
				 * used
				 */
/*
 * Mark key "key" as used
 */

VOID
setused(key) int key; {

	keyset[(key & 0177) >> 3] |= (1 << (key & 07));
}

/*
 * return non-zero if key "key" is used in a keymap
 */

int
isused(key) int key; {

	return keyset[(key & 0177) >> 3] & (1 << (key & 07));
}
