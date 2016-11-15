/*	$NetBSD: wsdisplay_glyphcachevar.h,v 1.4 2012/10/04 10:26:32 macallan Exp $	*/

/*
 * Copyright (c) 2012 Michael Lorenz
 * All rights reserved.
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

/* a simple glyph cache in offscreen memory */

#ifndef WSDISPLAY_GLYPHCACHEVAR_H
#define WSDISPLAY_GLYPHCACHEVAR_H

#include <sys/time.h>

typedef struct _gc_bucket {
	int gb_firstcell;
	int gb_numcells;
	volatile unsigned int gb_usedcells;
	int gb_index;		/* -1 for unused buckets */
	uint32_t gb_map[223];	/* we only care about char codes 0x21 and up */
	time_t gb_lastread;
} gc_bucket;

typedef struct _glyphcache {
	/* geometry */
	int gc_numcells;
	int gc_cellwidth;
	int gc_cellheight;
	int gc_cellsperline;
	int gc_firstline;	/* first line in vram to use for glyphs */
	/* buckets */
	int gc_numbuckets;
	gc_bucket *gc_buckets;	/* we allocate as many as we can get into vram */
	gc_bucket *gc_next;	/* bucket the next glyph goes into */
	long gc_underline;	/* draw an underline in glyphcache_add() */
	int gc_attrmap[256];	/* mapping a colour attribute to a bucket */
	/*
	 * method to copy glyphs within vram,
	 * to be initialized before calling glyphcache_init()
	 */
	void (*gc_bitblt)(void *, int, int, int, int, int, int, int);
	void (*gc_rectfill)(void *, int, int, int, int, long);
	void *gc_blitcookie;
	int gc_rop;
} glyphcache;

/* first line, lines, width, cellwidth, cellheight, attr */
int glyphcache_init(glyphcache *, int, int, int, int, int, long);
/* clear out the cache, for example when returning from X */
void glyphcache_wipe(glyphcache *);
/* add a glyph to the cache */
int glyphcache_add(glyphcache *, int, int, int); /* char code, x, y */
/* try to draw a glyph from the cache */
int glyphcache_try(glyphcache *, int, int, int, long); /* char code, x, y, attr */
#define	GC_OK	0 /* glyph was in cache and has been drawn */
#define GC_ADD	1 /* glyph is not in cache but can be added */
#define GC_NOPE 2 /* glyph is not in cache and can't be added */
/*
 * draw an underline in the given attribute
 * must be called by the driver if glyphcache_try() returns GC_NOPE and for
 * blanks
 */
void glyphcache_underline(glyphcache *, int, int, long);

#endif /* WSDISPLAY_GLYPHCACHEVAR_H */
