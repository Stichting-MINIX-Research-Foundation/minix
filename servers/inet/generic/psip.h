/*
generic/psip.h

Public interface to the pseudo IP module

Created:	Apr 22, 1993 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#ifndef PSIP_H
#define PSIP_H

void psip_prep ARGS(( void ));
void psip_init ARGS(( void ));
int psip_enable ARGS(( int port_nr, int ip_port_nr ));
int psip_send ARGS(( int port_nr, ipaddr_t dest, acc_t *pack ));

#endif /* PSIP_H */

/*
 * $PchId: psip.h,v 1.6 2001/04/19 21:16:22 philip Exp $
 */
