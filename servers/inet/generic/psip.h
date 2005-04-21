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
int psip_send ARGS(( int port_nr, acc_t *pack ));

#endif /* PSIP_H */

/*
 * $PchId: psip.h,v 1.4 1995/11/21 06:45:27 philip Exp $
 */
