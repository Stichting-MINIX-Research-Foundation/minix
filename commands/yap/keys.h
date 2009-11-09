/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

# ifndef _KEYS_
# define PUBLIC extern
# else
# define PUBLIC
# endif

PUBLIC struct keymap {
    char k_help[80];		/* To be printed on illegal command */
    struct state *k_mach;	/* Finite state machine */
    char k_esc[10];		/* escape chars */
} *currmap,			/* pointer to current key map */
  *othermap;			/* pointer to other keymap */

VOID initkeys();
/*
 * void initkeys();
 *
 * Initializes the keymap(s).
 */

VOID setused();
/*
 * void setused(key);
 * int key;
 *
 * Marks the key "key" as used.
 */

int isused();
/*
 * int isused(key);
 * int key;
 *
 * returns 0 if the key "key" is not used.
 * Otherwise it returns non-zero.
 */

int is_escape();
/*
 * int is_escape(c);
 * int c;
 *
 * Returns 1 if "c" is an escape char (shell or pipe) in the current
 * keymap.
 */
# undef PUBLIC
