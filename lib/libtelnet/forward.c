/*
 * appl/telnet/libtelnet/forward.c
 */

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


/* General-purpose forwarding routines. These routines may be put into */
/* libkrb5.a to allow widespread use */ 

#if defined(KERBEROS) || defined(KRB5)
#include <stdio.h>
#include <netdb.h>
 
#include "k5-int.h"
 
extern char *line;		/* see sys_term.c */

krb5_error_code rd_and_store_for_creds(krb5_context, krb5_auth_context, krb5_data *, krb5_ticket *);

/* Decode, decrypt and store the forwarded creds in the local ccache. */
krb5_error_code
rd_and_store_for_creds(context, auth_context, inbuf, ticket)
    krb5_context context;
    krb5_auth_context auth_context;
    krb5_data *inbuf;
    krb5_ticket *ticket;
{
    krb5_creds **creds;
    krb5_error_code retval;
    char ccname[35];
    krb5_ccache ccache = NULL;

    if ((retval = krb5_rd_cred(context, auth_context, inbuf, &creds, NULL)) != 0) 
	return(retval);

    snprintf(ccname, sizeof(ccname), "FILE:/tmp/krb5cc_p%d", getpid());
    setenv(KRB5_ENV_CCNAME, ccname, 1);

    if ((retval = krb5_cc_resolve(context, ccname, &ccache)) != 0)
	goto cleanup;

    if ((retval = krb5_cc_initialize(context, ccache, ticket->enc_part2->client)) != 0)
	goto cleanup;

    if ((retval = krb5_cc_store_cred(context, ccache, *creds)) != 0) 
	goto cleanup;

cleanup:
    krb5_free_creds(context, *creds);
    return retval;
}

#endif /* defined(KRB5) && defined(FORWARD) */
