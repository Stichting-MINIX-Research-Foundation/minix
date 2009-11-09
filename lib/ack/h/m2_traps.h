/* $Header$ */
/*
 * (c) copyright 1990 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

/* Modula-2 runtime errors */

#define M2_TOOLARGE	64	/* stack of process too large */
#define M2_TOOMANY	65	/* too many nested traps & handlers */
#define M2_NORESULT	66	/* no RETURN from procedure function */
#define M2_UOVFL	67	/* cardinal overflow */
#define M2_FORCH	68	/* FOR-loop control variable changed */
#define M2_UUVFL	69	/* cardinal underflow */
#define M2_INTERNAL	70	/* internal error, should not happen */
#define M2_UNIXSIG	71	/* unix signal */
