/* $NetBSD: pckbdvar.h,v 1.4 2008/01/10 07:58:39 dyoung Exp $ */

#include <dev/pckbport/pckbportvar.h>

int	pckbd_cnattach(pckbport_tag_t, int);
void	pckbd_hookup_bell(void (*fn)(void *, u_int, u_int, u_int, int), void *);
void	pckbd_unhook_bell(void (*fn)(void *, u_int, u_int, u_int, int), void *);
