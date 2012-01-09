/*	$NetBSD: label.S,v 1.3 2005/12/11 12:17:48 christos Exp $	*/

#include <machine/asm.h>

/*
 * This fills in the space taken by the NetBSD disklabel in the first
 * NetBSD partition on the disk.
 * However it is possible that we are booting from a subsequent
 * NetBSD partition, so must not access the disklabel in this space.
 *
 * Recent kernels (but not the disklabel program) write the label to
 * all NetBSD MBR partitions (including extended ones).
 * So we make it available to boot1().
 */

	.text
	.global	_C_LABEL(ptn_disklabel)
_C_LABEL(ptn_disklabel):
	.fill	512
