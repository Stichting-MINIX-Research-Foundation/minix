/*	$NetBSD: save.c,v 1.14 2014/03/22 22:04:40 dholland Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The game adventure was originally written in Fortran by Will Crowther
 * and Don Woods.  It was later translated to C and enhanced by Jim
 * Gillogly.  This code is derived from software contributed to Berkeley
 * by Jim Gillogly at The Rand Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)save.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: save.c,v 1.14 2014/03/22 22:04:40 dholland Exp $");
#endif
#endif				/* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <assert.h>

#include "hdr.h"
#include "extern.h"

struct savefile {
	FILE *f;
	const char *name;
	bool warned;
	size_t bintextpos;
	uint32_t key;
	struct crcstate crc;
	unsigned char pad[8];
	unsigned padpos;
};

#define BINTEXT_WIDTH 60
#define FORMAT_VERSION 2
#define FORMAT_VERSION_NOSUM 1
static const char header[] = "Adventure save file\n";

////////////////////////////////////////////////////////////
// base16 output encoding

/*
 * Map 16 plain values into 90 coded values and back.
 */

static const char coding[90] =
	"Db.GOyT]7a6zpF(c*5H9oK~0[WVAg&kR)ml,2^q-1Y3v+"
	"X/=JirZL$C>_N?:}B{dfnsxU<@MQ%8|P!4h`ESt;euwIj"
;

static int
readletter(char letter, unsigned char *ret)
{
	const char *s;

	s = strchr(coding, letter);
	if (s == NULL) {
		return 1;
	}
	*ret = (s - coding) % 16;
	return 0;
}

static char
writeletter(unsigned char nibble)
{
	unsigned code;

	assert(nibble < 16);
	do {
		code = (16 * (random() % 6)) + nibble;
	} while (code >= 90);
	return coding[code];
}

////////////////////////////////////////////////////////////
// savefile

/*
 * Open a savefile.
 */
static struct savefile *
savefile_open(const char *name, bool forwrite)
{
	struct savefile *sf;

	sf = malloc(sizeof(*sf));
	if (sf == NULL) {
		return NULL;
	}
	sf->f = fopen(name, forwrite ? "w" : "r");
	if (sf->f == NULL) {
		free(sf);
		fprintf(stderr,
		    "Hmm.  The name \"%s\" appears to be magically blocked.\n",
		    name);
		return NULL;
	}
	sf->name = name;
	sf->warned = false;
	sf->bintextpos = 0;
	sf->key = 0;
	crc_start(&sf->crc);
	memset(sf->pad, 0, sizeof(sf->pad));
	sf->padpos = 0;
	return sf;
}

/*
 * Raw read.
 */
static int
savefile_rawread(struct savefile *sf, void *data, size_t len)
{
	size_t result;

	result = fread(data, 1, len, sf->f);
	if (result != len || ferror(sf->f)) {
		fprintf(stderr, "Oops: error reading %s.\n", sf->name);
		sf->warned = true;
		return 1;
	}
	return 0;
}

/*
 * Raw write.
 */
static int
savefile_rawwrite(struct savefile *sf, const void *data, size_t len)
{
	size_t result;

	result = fwrite(data, 1, len, sf->f);
	if (result != len || ferror(sf->f)) {
		fprintf(stderr, "Oops: error writing %s.\n", sf->name);
		sf->warned = true;
		return 1;
	}
	return 0;
}

/*
 * Close a savefile.
 */
static int
savefile_close(struct savefile *sf)
{
	int ret;

	if (sf->bintextpos > 0) {
		savefile_rawwrite(sf, "\n", 1);
	}

	ret = 0;
	if (fclose(sf->f)) {
		if (!sf->warned) {
			fprintf(stderr, "Oops: error on %s.\n", sf->name);
		}
		ret = 1;
	}
	free(sf);
	return ret;
}

/*
 * Read encoded binary data, discarding any whitespace that appears.
 */
static int
savefile_bintextread(struct savefile *sf, void *data, size_t len)
{
	size_t pos;
	unsigned char *udata;
	int ch;

	udata = data;
	pos = 0;
	while (pos < len) {
		ch = fgetc(sf->f);
		if (ch == EOF || ferror(sf->f)) {
			fprintf(stderr, "Oops: error reading %s.\n", sf->name);
			sf->warned = true;
			return 1;
		}
		if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
			continue;
		}
		udata[pos++] = ch;
	}
	return 0;
}

/*
 * Read binary data, decoding from text using readletter().
 */
static int
savefile_binread(struct savefile *sf, void *data, size_t len)
{
	unsigned char buf[64];
	unsigned char *udata;
	unsigned char val1, val2;
	size_t pos, amt, i;

	udata = data;
	pos = 0;
	while (pos < len) {
		amt = len - pos;
		if (amt > sizeof(buf) / 2) {
			amt = sizeof(buf) / 2;
		}
		if (savefile_bintextread(sf, buf, amt*2)) {
			return 1;
		}
		for (i=0; i<amt; i++) {
			if (readletter(buf[i*2], &val1)) {
				return 1;
			}
			if (readletter(buf[i*2 + 1], &val2)) {
				return 1;
			}
			udata[pos++] = val1 * 16 + val2;
		}
	}
	return 0;
}

/*
 * Write encoded binary data, inserting newlines to get a neatly
 * formatted block.
 */
static int
savefile_bintextwrite(struct savefile *sf, const void *data, size_t len)
{
	size_t pos, amt;
	const unsigned char *udata;

	udata = data;
	pos = 0;
	while (pos < len) {
		amt = BINTEXT_WIDTH - sf->bintextpos;
		if (amt > len - pos) {
			amt = len - pos;
		}
		if (savefile_rawwrite(sf, udata + pos, amt)) {
			return 1;
		}
		pos += amt;
		sf->bintextpos += amt;
		if (sf->bintextpos >= BINTEXT_WIDTH) {
			savefile_rawwrite(sf, "\n", 1);
			sf->bintextpos = 0;
		}
	}
	return 0;
}

/*
 * Write binary data, encoding as text using writeletter().
 */
static int
savefile_binwrite(struct savefile *sf, const void *data, size_t len)
{
	unsigned char buf[64];
	const unsigned char *udata;
	size_t pos, bpos;
	unsigned char byte;

	udata = data;
	pos = 0;
	bpos = 0;
	while (pos < len) {
		byte = udata[pos++];
		buf[bpos++] = writeletter(byte >> 4);
		buf[bpos++] = writeletter(byte & 0xf);
		if (bpos >= sizeof(buf)) {
			if (savefile_bintextwrite(sf, buf, bpos)) {
				return 1;
			}
			bpos = 0;
		}
	}
	if (savefile_bintextwrite(sf, buf, bpos)) {
		return 1;
	}
	return 0;
}

/*
 * Lightweight "encryption" for save files. This is not meant to
 * be secure and wouldn't be even if we didn't write the decrypt
 * key to the beginning of the save file; it's just meant to be
 * enough to discourage casual cheating.
 */

/*
 * Make cheesy hash of buf[0..buflen]. Note: buf and outhash may overlap.
 */
static void
hash(const void *data, size_t datalen, unsigned char *out, size_t outlen)
{
	const unsigned char *udata;
	size_t i;
	uint64_t val;
	const unsigned char *uval;
	size_t valpos;

	udata = data;
	val = 0;
	for (i=0; i<datalen; i++) {
		val = val ^ 0xbadc0ffee;
		val = (val << 4) | (val >> 60);
		val += udata[i] ^ 0xbeefU;
	}

	uval = (unsigned char *)&val;
	valpos = 0;
	for (i=0; i<outlen; i++) {
		out[i] = uval[valpos++];
		if (valpos >= sizeof(val)) {
			valpos = 0;
		}
	}
}

/*
 * Set the "encryption" key.
 */
static void
savefile_key(struct savefile *sf, uint32_t key)
{
	sf->key = 0;
	crc_start(&sf->crc);
	hash(&sf->key, sizeof(sf->key), sf->pad, sizeof(sf->pad));
	sf->padpos = 0;
}

/*
 * Get an "encryption" pad byte. This forms a stream "cipher" that we
 * xor with the plaintext save data.
 */
static unsigned char
savefile_getpad(struct savefile *sf)
{
	unsigned char ret;

	ret = sf->pad[sf->padpos++];
	if (sf->padpos >= sizeof(sf->pad)) {
		hash(sf->pad, sizeof(sf->pad), sf->pad, sizeof(sf->pad));
		sf->padpos = 0;
	}
	return ret;
}

/*
 * Read "encrypted" data.
 */
static int
savefile_cread(struct savefile *sf, void *data, size_t len)
{
	char buf[64];
	unsigned char *udata;
	size_t pos, amt, i;
	unsigned char ch;

	udata = data;
	pos = 0;
	while (pos < len) {
		amt = len - pos;
		if (amt > sizeof(buf)) {
			amt = sizeof(buf);
		}
		if (savefile_binread(sf, buf, amt)) {
			return 1;
		}
		for (i=0; i<amt; i++) {
			ch = buf[i];
			ch ^= savefile_getpad(sf);
			udata[pos + i] = ch;
		}
		pos += amt;
	}
	crc_add(&sf->crc, data, len);
	return 0;
}

/*
 * Write "encrypted" data.
 */
static int
savefile_cwrite(struct savefile *sf, const void *data, size_t len)
{
	char buf[64];
	const unsigned char *udata;
	size_t pos, amt, i;
	unsigned char ch;

	udata = data;
	pos = 0;
	while (pos < len) {
		amt = len - pos;
		if (amt > sizeof(buf)) {
			amt = sizeof(buf);
		}
		for (i=0; i<amt; i++) {
			ch = udata[pos + i];
			ch ^= savefile_getpad(sf);
			buf[i] = ch;
		}
		if (savefile_binwrite(sf, buf, amt)) {
			return 1;
		}
		pos += amt;
	}
	crc_add(&sf->crc, data, len);
	return 0;
}

////////////////////////////////////////////////////////////
// compat for old save files

struct compat_saveinfo {
	void   *address;
	size_t  width;
};

static const struct compat_saveinfo compat_savearray[] =
{
	{&abbnum, sizeof(abbnum)},
	{&attack, sizeof(attack)},
	{&blklin, sizeof(blklin)},
	{&bonus, sizeof(bonus)},
	{&chloc, sizeof(chloc)},
	{&chloc2, sizeof(chloc2)},
	{&clock1, sizeof(clock1)},
	{&clock2, sizeof(clock2)},
	{&closed, sizeof(closed)},
	{&isclosing, sizeof(isclosing)},
	{&daltloc, sizeof(daltloc)},
	{&demo, sizeof(demo)},
	{&detail, sizeof(detail)},
	{&dflag, sizeof(dflag)},
	{&dkill, sizeof(dkill)},
	{&dtotal, sizeof(dtotal)},
	{&foobar, sizeof(foobar)},
	{&gaveup, sizeof(gaveup)},
	{&holding, sizeof(holding)},
	{&iwest, sizeof(iwest)},
	{&k, sizeof(k)},
	{&k2, sizeof(k2)},
	{&knfloc, sizeof(knfloc)},
	{&kq, sizeof(kq)},
	{&latency, sizeof(latency)},
	{&limit, sizeof(limit)},
	{&lmwarn, sizeof(lmwarn)},
	{&loc, sizeof(loc)},
	{&maxdie, sizeof(maxdie)},
	{&maxscore, sizeof(maxscore)},
	{&newloc, sizeof(newloc)},
	{&numdie, sizeof(numdie)},
	{&obj, sizeof(obj)},
	{&oldloc2, sizeof(oldloc2)},
	{&oldloc, sizeof(oldloc)},
	{&panic, sizeof(panic)},
	{&saveday, sizeof(saveday)},
	{&savet, sizeof(savet)},
	{&scoring, sizeof(scoring)},
	{&spk, sizeof(spk)},
	{&stick, sizeof(stick)},
	{&tally, sizeof(tally)},
	{&tally2, sizeof(tally2)},
	{&tkk, sizeof(tkk)},
	{&turns, sizeof(turns)},
	{&verb, sizeof(verb)},
	{&wd1, sizeof(wd1)},
	{&wd2, sizeof(wd2)},
	{&wasdark, sizeof(wasdark)},
	{&yea, sizeof(yea)},
	{atloc, sizeof(atloc)},
	{dloc, sizeof(dloc)},
	{dseen, sizeof(dseen)},
	{fixed, sizeof(fixed)},
	{hinted, sizeof(hinted)},
	{links, sizeof(links)},
	{odloc, sizeof(odloc)},
	{place, sizeof(place)},
	{prop, sizeof(prop)},
	{tk, sizeof(tk)},

	{NULL, 0}
};

static int
compat_restore(const char *infile)
{
	FILE   *in;
	const struct compat_saveinfo *p;
	char   *s;
	long    sum, cksum = 0;
	size_t  i;
	struct crcstate crc;

	if ((in = fopen(infile, "rb")) == NULL) {
		fprintf(stderr,
		    "Hmm.  The file \"%s\" appears to be magically blocked.\n",
		    infile);
		return 1;
	}
	fread(&sum, sizeof(sum), 1, in);	/* Get the seed */
	srandom((int) sum);
	for (p = compat_savearray; p->address != NULL; p++) {
		fread(p->address, p->width, 1, in);
		for (s = p->address, i = 0; i < p->width; i++, s++)
			*s = (*s ^ random()) & 0xFF;	/* Lightly decrypt */
	}
	fclose(in);

	crc_start(&crc);		/* See if she cheated */
	for (p = compat_savearray; p->address != NULL; p++)
		crc_add(&crc, p->address, p->width);
	cksum = crc_get(&crc);
	if (sum != cksum)	/* Tsk tsk */
		return 2;	/* Altered the file */
	/* We successfully restored, so this really was a save file */

	/*
	 * The above code loads these from disk even though they're
	 * pointers. Null them out and hope we don't crash on them
	 * later; that's better than having them be garbage.
	 */
	tkk = NULL;
	wd1 = NULL;
	wd2 = NULL;

	return 0;
}

////////////////////////////////////////////////////////////
// save + restore

static int *const save_ints[] = {
	&abbnum,
	&attack,
	&blklin,
	&bonus,
	&chloc,
	&chloc2,
	&clock1,
	&clock2,
	&closed,
	&isclosing,
	&daltloc,
	&demo,
	&detail,
	&dflag,
	&dkill,
	&dtotal,
	&foobar,
	&gaveup,
	&holding,
	&iwest,
	&k,
	&k2,
	&knfloc,
	&kq,
	&latency,
	&limit,
	&lmwarn,
	&loc,
	&maxdie,
	&maxscore,
	&newloc,
	&numdie,
	&obj,
	&oldloc2,
	&oldloc,
	&panic,
	&saveday,
	&savet,
	&scoring,
	&spk,
	&stick,
	&tally,
	&tally2,
	&turns,
	&verb,
	&wasdark,
	&yea,
};
static const unsigned num_save_ints = __arraycount(save_ints);

#define INTARRAY(sym) { sym, __arraycount(sym) }

static const struct {
	int *ptr;
	unsigned num;
} save_intarrays[] = {
	INTARRAY(atloc),
	INTARRAY(dseen),
	INTARRAY(dloc),
	INTARRAY(odloc),
	INTARRAY(fixed),
	INTARRAY(hinted),
	INTARRAY(links),
	INTARRAY(place),
	INTARRAY(prop),
	INTARRAY(tk),
};
static const unsigned num_save_intarrays = __arraycount(save_intarrays);

#undef INTARRAY

#if 0
static const struct {
	void *ptr;
	size_t len;
} save_blobs[] = {
	{ &wd1, sizeof(wd1) },
	{ &wd2, sizeof(wd2) },
	{ &tkk, sizeof(tkk) },
};
static const unsigned num_save_blobs = __arraycount(save_blobs);
#endif

/*
 * Write out a save file. Returns nonzero on error.
 */
int
save(const char *outfile)
{
	struct savefile *sf;
	struct timespec now;
	uint32_t key, writeable_key;
	uint32_t version;
	unsigned i, j, n;
	uint32_t val, sum;

	sf = savefile_open(outfile, true);
	if (sf == NULL) {
		return 1;
	}

	if (savefile_rawwrite(sf, header, strlen(header))) {
		savefile_close(sf);
		return 1;
	}

	version = htonl(FORMAT_VERSION);
	if (savefile_binwrite(sf, &version, sizeof(version))) {
		savefile_close(sf);
		return 1;
	}

	clock_gettime(CLOCK_REALTIME, &now);
	key = (uint32_t)(now.tv_sec & 0xffffffff) ^ (uint32_t)(now.tv_nsec);

	writeable_key = htonl(key);
	if (savefile_binwrite(sf, &writeable_key, sizeof(writeable_key))) {
		savefile_close(sf);
		return 1;
	}

	/* other parts of the code may depend on us doing this here */
	srandom(key);

	savefile_key(sf, key);

	/*
	 * Integers
	 */
	for (i=0; i<num_save_ints; i++) {
		val = *(save_ints[i]);
		val = htonl(val);
		if (savefile_cwrite(sf, &val, sizeof(val))) {
			savefile_close(sf);
			return 1;
		}
	}

	/*
	 * Arrays of integers
	 */
	for (i=0; i<num_save_intarrays; i++) {
		n = save_intarrays[i].num;
		for (j=0; j<n; j++) {
			val = save_intarrays[i].ptr[j];
			val = htonl(val);
			if (savefile_cwrite(sf, &val, sizeof(val))) {
				savefile_close(sf);
				return 1;
			}
		}
	}

#if 0
	/*
	 * Blobs
	 */
	for (i=0; i<num_save_blobs; i++) {
		if (savefile_cwrite(sf, save_blobs[i].ptr, save_blobs[i].len)) {
			savefile_close(sf);
			return 1;
		}
	}
#endif

	sum = htonl(crc_get(&sf->crc));
	if (savefile_binwrite(sf, &sum, sizeof(&sum))) {
		savefile_close(sf);
		return 1;
	}
	savefile_close(sf);
	return 0;
}

/*
 * Read in a save file. Returns nonzero on error.
 */
int
restore(const char *infile)
{
	struct savefile *sf;
	char buf[sizeof(header)];
	size_t headersize = strlen(header);
	uint32_t version, key, sum;
	unsigned i, j, n;
	uint32_t val;
	bool skipsum = false;

	sf = savefile_open(infile, false);
	if (sf == NULL) {
		return 1;
	}

	if (savefile_rawread(sf, buf, headersize)) {
		savefile_close(sf);
		return 1;
	}
	buf[headersize] = 0;
	if (strcmp(buf, header) != 0) {
		savefile_close(sf);
		fprintf(stderr, "Oh dear, that isn't one of my save files.\n");
		fprintf(stderr,
		    "Trying the Olde Waye; this myte notte Worke.\n");
		return compat_restore(infile);
	}

	if (savefile_binread(sf, &version, sizeof(version))) {
		savefile_close(sf);
		return 1;
	}
	version = ntohl(version);
	switch (version) {
	    case FORMAT_VERSION:
		break;
	    case FORMAT_VERSION_NOSUM:
		skipsum = true;
		break;
	    default:
		savefile_close(sf);
		fprintf(stderr,
		    "Oh dear, that file must be from the future. I don't know"
		    " how to read it!\n");
		return 1;
	}

	if (savefile_binread(sf, &key, sizeof(key))) {
		savefile_close(sf);
		return 1;
	}
	key = ntohl(key);
	savefile_key(sf, key);

	/* other parts of the code may depend on us doing this here */
	srandom(key);

	/*
	 * Integers
	 */
	for (i=0; i<num_save_ints; i++) {
		if (savefile_cread(sf, &val, sizeof(val))) {
			savefile_close(sf);
			return 1;
		}
		val = ntohl(val);
		*(save_ints[i]) = val;
	}

	/*
	 * Arrays of integers
	 */
	for (i=0; i<num_save_intarrays; i++) {
		n = save_intarrays[i].num;
		for (j=0; j<n; j++) {
			if (savefile_cread(sf, &val, sizeof(val))) {
				savefile_close(sf);
				return 1;
			}
			val = ntohl(val);
			save_intarrays[i].ptr[j] = val;
		}
	}

#if 0
	/*
	 * Blobs
	 */
	for (i=0; i<num_save_blobs; i++) {
		if (savefile_cread(sf, save_blobs[i].ptr, save_blobs[i].len)) {
			savefile_close(sf);
			return 1;
		}
	}
#endif

	if (savefile_binread(sf, &sum, sizeof(&sum))) {
		savefile_close(sf);
		return 1;
	}
	sum = ntohl(sum);
	/* See if she cheated */
	if (!skipsum && sum != crc_get(&sf->crc)) {
		/* Tsk tsk, altered the file */
		savefile_close(sf);
		return 2;
	}
	savefile_close(sf);

	/* Load theoretically invalidates these */
	tkk = NULL;
	wd1 = NULL;
	wd2 = NULL;

	return 0;
}
