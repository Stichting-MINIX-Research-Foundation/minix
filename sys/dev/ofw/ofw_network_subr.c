/*	$NetBSD: ofw_network_subr.c,v 1.7 2009/03/14 21:04:21 dsl Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ofw_network_subr.c,v 1.7 2009/03/14 21:04:21 dsl Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/ofw/openfirm.h>

#define	OFW_MAX_STACK_BUF_SIZE	256
#define	OFW_PATH_BUF_SIZE	512

struct table_entry {
	const char *t_string;
	int t_value;
};

int	of_network_parse_network_type(const char *);

/*
 * int of_network_decode_media(phandle, nmediap, defmediap)
 *
 * This routine decodes the OFW properties `supported-network-types'
 * and `chosen-network-type'.
 *
 * Arguments:
 *	phandle		OFW phandle of device whos network media properties
 *			are to be decoded.
 *	nmediap		Pointer to an integer which will be initialized
 *			with the number of returned media words.
 *	defmediap	Pointer to an integer which will be initialized
 *			with the default network media.
 *
 * Return Values:
 *	An array of integers, allocated with malloc(), containing the
 *	decoded media values.  The number of elements in the array will
 *	be stored in the location pointed to by the `nmediap' argument.
 *	The default media will be stored in the location pointed to by
 *	the `defmediap' argument.
 *
 * Side Effects:
 *	None.
 */
int *
of_network_decode_media(int phandle, int *nmediap, int *defmediap)
{
	int i, len, count, med, *rv = NULL;
	char *buf = NULL, *cp, *ncp;

	len = OF_getproplen(phandle, "supported-network-types");
	if (len <= 0)
		return (NULL);

	buf = malloc(len, M_TEMP, M_WAITOK);

	/* `supported-network-types' should not change. */
	if (OF_getprop(phandle, "supported-network-types", buf, len) != len)
		goto bad;

	/*
	 * Count the number of entries in the array.  This is kind of tricky,
	 * because they're variable-length strings, yuck.
	 */
	for (count = 0, cp = buf; cp <= (buf + len); cp++) {
		/*
		 * If we encounter nul, that marks the end of a string,
		 * and thus one complete media description.
		 */
		if (*cp == '\0')
			count++;
	}

	/* Sanity. */
	if (count == 0)
		goto bad;

	/* Allocate the return value array. */
	rv = malloc(count * sizeof(int), M_DEVBUF, M_WAITOK);

	/*
	 * Parse each media string.  If we get -1 back from the parser,
	 * back off the count by one, to skip the bad entry.
	 */
	for (i = 0, cp = buf; cp <= (buf + len) && i < count; ) {
		/*
		 * Find the next string now, as we may chop
		 * the current one up in the parser.
		 */
		for (ncp = cp; *ncp != '\0'; ncp++)
			/* ...skip to the nul... */ ;
		ncp++;	/* ...and now past it. */

		med = of_network_parse_network_type(cp);
		if (med == -1)
			count--;
		else {
			rv[i] = med;
			i++;
		}
		cp = ncp;
	}

	/* Sanity... */
	if (count == 0)
		goto bad;

	/*
	 * We now have the `supported-media-types' property decoded.
	 * Next step is to decode the `chosen-media-type' property,
	 * if it exists.
	 */
	free(buf, M_TEMP);
	buf = NULL;
	len = OF_getproplen(phandle, "chosen-network-type");
	if (len <= 0) {
		/* Property does not exist. */
		*defmediap = -1;
		goto done;
	}

	buf = malloc(len, M_TEMP, M_WAITOK);
	if (OF_getprop(phandle, "chosen-network-type", buf, len) != len) {
		/* Something went wrong... */
		*defmediap = -1;
		goto done;
	}

	*defmediap = of_network_parse_network_type(buf);

 done:
	if (buf != NULL)
		free(buf, M_TEMP);
	*nmediap = count;
	return (rv);

 bad:
	if (rv != NULL)
		free(rv, M_DEVBUF);
	if (buf != NULL)
		free(buf, M_TEMP);
	return (NULL);
}

int
of_network_parse_network_type(const char *cp)
{
	/*
	 * We could tokenize this, but that would be a pain in
	 * the neck given how the media are described.  If this
	 * table grows any larger, we may want to consider doing
	 * that.
	 *
	 * Oh yes, we also only support combinations that actually
	 * make sense.
	 */
	static const struct table_entry mediatab[] = {
		{ "ethernet,10,rj45,half",
		  IFM_ETHER|IFM_10_T },
		{ "ethernet,10,rj45,full",
		  IFM_ETHER|IFM_10_T|IFM_FDX },
		{ "ethernet,10,aui,half",
		  IFM_ETHER|IFM_10_5, },
		{ "ethernet,10,bnc,half",
		  IFM_ETHER|IFM_10_2, },
		{ "ethernet,100,rj45,half",
		  IFM_ETHER|IFM_100_TX },
		{ "ethernet,100,rj45,full",
		  IFM_ETHER|IFM_100_TX|IFM_FDX },
		{ NULL, -1 },
	};
	int i;

	for (i = 0; mediatab[i].t_string != NULL; i++) {
		if (strcmp(cp, mediatab[i].t_string) == 0)
			return (mediatab[i].t_value);
	}

	/* Not found. */
	return (-1);
}
