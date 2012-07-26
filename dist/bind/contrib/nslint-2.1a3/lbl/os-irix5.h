/*
 * Copyright (c) 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Header: /proj/cvs/prod/bind9/contrib/nslint-2.1a3/lbl/os-irix5.h,v 1.1 2001-12-21 04:12:05 marka Exp $ (LBL)
 */

/* Prototypes missing in IRIX 5 */
#ifdef __STDC__
struct	ether_addr;
#endif
int	ether_hostton(char *, struct ether_addr *);
char	*ether_ntoa(struct ether_addr *);
#ifdef __STDC__
struct	utmp;
#endif
void	login(struct utmp *);
int	setenv(const char *, const char *, int);
int	sigblock(int);
int	sigsetmask(int);
int	snprintf(char *, size_t, const char *, ...);
time_t	time(time_t *);
