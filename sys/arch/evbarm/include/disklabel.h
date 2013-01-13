/*	$NetBSD: disklabel.h,v 1.4 2011/08/30 12:39:54 bouyer Exp $	*/

#define LABELUSESMBR	1
#if HAVE_NBTOOL_CONFIG_H
#include <nbinclude/arm/disklabel.h>
#else
#include <arm/disklabel.h>
#endif /* HAVE_NBTOOL_CONFIG_H */
