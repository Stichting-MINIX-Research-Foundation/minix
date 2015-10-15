/*	$NetBSD: dev_verbose.c,v 1.1 2014/09/21 14:30:22 christos Exp $	*/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dev_verbose.c,v 1.1 2014/09/21 14:30:22 christos Exp $");

#include <sys/param.h>

#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <stdio.h>
#include <string.h>
#endif

#include <dev/dev_verbose.h>

static const char *
dev_untokenstring(const char *words, const uint16_t *token, char *buf,
    size_t len)
{
	char *cp = buf;

	buf[0] = '\0';
	for (; *token != 0; token++) {
		cp = buf + strlcat(buf, words + *token, len - 2);
		cp[0] = ' ';
		cp[1] = '\0';
	}
	*cp = '\0';
	return cp != buf ? buf : NULL;
}

const char *
dev_findvendor(char *buf, size_t len, const char *words, size_t nwords,
    const uint16_t *vendors, size_t nvendors, uint16_t vendor)
{
	size_t n;

	for (n = 0; n < nvendors; n++) {
		if (vendors[n] == vendor)
			return dev_untokenstring(words, &vendors[n + 1],
			    buf, len);

		/* Skip Tokens */
		n++;
		while (vendors[n] != 0 && n < nvendors)
			n++;
	}
	snprintf(buf, len, "vendor %4.4x", vendor);
	return NULL;
}

const char *
dev_findproduct(char *buf, size_t len, const char *words, size_t nwords,
    const uint16_t *products, size_t nproducts, uint16_t vendor,
    uint16_t product)
{
	size_t n;

	for (n = 0; n < nproducts; n++) {
		if (products[n] == vendor && products[n + 1] == product)
			return dev_untokenstring(words, &products[n + 2],
			    buf, len);

		/* Skip Tokens */
		n += 2;
		while (products[n] != 0 && n < nproducts)
			n++;
	}
	snprintf(buf, len, "product %4.4x", product);
	return NULL;
}
