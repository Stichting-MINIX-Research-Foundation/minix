/* $Id: unicode_template.c,v 1.1.1.1 2003-06-04 00:26:16 marka Exp $ */

/*
 * Copyright (c) 2000,2001 Japan Network Information Center.
 * All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef UNICODE_TEMPLATE_INIT
#define UNICODE_TEMPLATE_INIT

/*
 * Macro for multi-level index table.
 */
#define LOOKUPTBL(vprefix, mprefix, v) \
	DMAP(vprefix)[\
		IMAP(vprefix)[\
			IMAP(vprefix)[IDX0(mprefix, v)] + IDX1(mprefix, v)\
		]\
	].tbl[IDX2(mprefix, v)]

#define IDX0(mprefix, v) IDX_0(v, BITS1(mprefix), BITS2(mprefix))
#define IDX1(mprefix, v) IDX_1(v, BITS1(mprefix), BITS2(mprefix))
#define IDX2(mprefix, v) IDX_2(v, BITS1(mprefix), BITS2(mprefix))

#define IDX_0(v, bits1, bits2)	((v) >> ((bits1) + (bits2)))
#define IDX_1(v, bits1, bits2)	(((v) >> (bits2)) & ((1 << (bits1)) - 1))
#define IDX_2(v, bits1, bits2)	((v) & ((1 << (bits2)) - 1))

#define BITS1(mprefix)	mprefix ## _BITS_1
#define BITS2(mprefix)	mprefix ## _BITS_2

#define IMAP(vprefix)	concat4(VERSION, _, vprefix, _imap)
#define DMAP(vprefix)	concat4(VERSION, _, vprefix, _table)
#define SEQ(vprefix)	concat4(VERSION, _, vprefix, _seq)
#define concat4(a,b,c,d)	concat4X(a, b, c, d)
#define concat4X(a,b,c,d)	a ## b ## c ## d

#endif /* UNICODE_TEMPLATE_INIT */

static int
compose_sym(canonclass_, VERSION) (unsigned long c) {
	/* Look up canonicalclass table. */
	return (LOOKUPTBL(canon_class, CANON_CLASS, c));
}

static int
compose_sym(decompose_, VERSION) (unsigned long c, const unsigned long **seqp)
{
	/* Look up decomposition table. */
	int seqidx = LOOKUPTBL(decompose, DECOMP, c);
	*seqp = SEQ(decompose) + (seqidx & ~DECOMP_COMPAT);
	return (seqidx);
}

static int
compose_sym(compose_, VERSION) (unsigned long c,
				const struct composition **compp)
{
	/* Look up composition table. */
	int seqidx = LOOKUPTBL(compose, CANON_COMPOSE, c);
	*compp = SEQ(compose) + (seqidx & 0xffff);
	return (seqidx >> 16);
}
