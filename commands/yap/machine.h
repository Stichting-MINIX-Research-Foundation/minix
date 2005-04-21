/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

# ifndef _MACHINE_
# define PUBLIC extern
# else
# define PUBLIC
# endif

/*
 * Simple minded finite state machine implementation to recognize
 * strings.
 */

struct state {
    char s_char;		/* character to match with */
    char s_endstate;		/* flag, 1 if this state is an endstate */
    struct state *s_match;	/* new state if matched */
    struct state *s_next;	/* other characters to match with */
    short s_cnt;		/* if an endstate, this field is filled with
				 * some info, dependant on the machine.
				 */
};

# define FSM_OKE	0
# define FSM_ISPREFIX	-1	/* Must be < 0 */
# define FSM_HASPREFIX	1

int	addstring();
/*
 * int addstring(str,cnt,mach)
 * char *str;			The string to be recognized
 * int cnt;			Attribute of the string.
 * struct state **mach;		The finite state machine
 *
 * This routine adds a string to a finite state automaton.
 * It returns FSM_ISPREFIX if the added string is a prefix of a string already
 * in the automaton, FSM_HASPREFIX if a string, already recognized by the
 * automaton, is a prefix of the added string.
 * Otherwise it returns FSM_OKE.
 */

int	match();
/*
 * int match(str,p_int,mach)
 * char *str;			pointer to string
 * int *p_int;			Pointer to an integer
 * struct state *mach;		The finite state machine
 *
 * A match of the string indicated by "str" is tried. If a head of "str"
 * is recognized by the finite state automaton, a machine dependant number
 * is put in the integer pointed to by "p_int".
 * The number of characters that match is returned, so a return value of 0
 * means no match.
 * A return value of FSM_PREFIX means that the string "str" was a prefix of a
 * matched string.
 */

# undef PUBLIC
