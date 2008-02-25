/* IBM device driver definitions			Author: Kees J. Bot
 *								7 Dec 1995
 */

#include <ibm/partition.h>

_PROTOTYPE( void partition, (struct driver *dr, int device, int style, int atapi) );

/* BIOS parameter table layout. */
#define bp_cylinders(t)		(* (u16_t *) (&(t)[0]))
#define bp_heads(t)		(* (u8_t *)  (&(t)[2]))
#define bp_reduced_wr(t)	(* (u16_t *) (&(t)[3]))
#define bp_precomp(t)		(* (u16_t *) (&(t)[5]))
#define bp_max_ecc(t)		(* (u8_t *)  (&(t)[7]))
#define bp_ctlbyte(t)		(* (u8_t *)  (&(t)[8]))
#define bp_landingzone(t)	(* (u16_t *) (&(t)[12]))
#define bp_sectors(t)		(* (u8_t *)  (&(t)[14]))

/* Miscellaneous. */
#define DEV_PER_DRIVE	(1 + NR_PARTITIONS)
#define MINOR_t0	64
#define MINOR_r0	120
#define MINOR_d0p0s0	128
#define MINOR_fd0p0	(28<<2)
#define P_FLOPPY	0
#define P_PRIMARY	1
#define P_SUB		2
