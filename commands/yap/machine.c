/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _MACHINE_

# include <ctype.h>
# include "in_all.h"
# include "machine.h"
# include "getline.h"
# include "assert.h"

/*
 * Add part of finite state machine to recognize the string s.
 */

STATIC int
addtomach(s, cnt, list) char *s; struct state **list; {

	register struct state *l;
	register int i = FSM_OKE;	/* Return value */
	register int j;

	for (;;) {
		l = *list;
		if (!l) {
			/*
			 * Create new list element
			 */
			*list = l = (struct state *) alloc(sizeof(*l), 0);
			l->s_char = *s;
			l->s_endstate = 0;
			l->s_match = 0;
			l->s_next = 0;
		}
		if (l->s_char == *s) {
			/*
			 * Continue with next character
			 */
			if (!*++s) {
				/*
				 * No next character
				 */
				j = l->s_endstate;
				l->s_endstate = 1;
				if (l->s_match || j) {
					/*
					 * If the state already was an endstate,
					 * or has a successor, the currently
					 * added string is a prefix of an
					 * already recognized string
					 */
					return FSM_ISPREFIX;
				}
				l->s_cnt = cnt;
				return i;
			}
			if (l->s_endstate) {
				/*
				 * In this case, the currently added string has
				 * a prefix that is an already recognized
				 * string.
				 */
				i = FSM_HASPREFIX;
			}
			list = &(l->s_match);
			continue;
		}
		list = &(l->s_next);
	}
	/* NOTREACHED */
}

/*
 * Add a string to the FSM.
 */

int
addstring(s,cnt,machine) register char *s; struct state **machine; {

	if (!s || !*s) {
		return FSM_ISPREFIX;
	}
	return addtomach(s,cnt,machine);
}

/*
 * Match string s with the finite state machine.
 * If it matches, the number of characters actually matched is returned,
 * and the count is put in the word pointed to by i.
 * If the string is a prefix of a string that could be matched,
 * FSM_ISPREFIX is returned. Otherwise, 0 is returned.
 */

int
match(s,i,mach) char *s; int *i; register struct state *mach; {

	register char *s1 = s;	/* Walk through string */
	register struct state *mach1 = 0;
				/* Keep track of previous state */

	while (mach && *s1) {
		if (mach->s_char == *s1) {
			/*
			 * Current character matches. Carry on with next
			 * character and next state
			 */
			mach1 = mach;
			mach = mach->s_match;
			s1++;
			continue;
		}
		mach = mach->s_next;
	}
	if (!mach1) {
		/*
		 * No characters matched
		 */
		return 0;
	}
	if (mach1->s_endstate) {
		/*
		 * The string matched
		 */
		*i = mach1->s_cnt;
		return s1 - s;
	}
	if (!*s1) {
		/*
		 * The string matched a prefix
		 */
		return FSM_ISPREFIX;
	}
	return 0;
}
