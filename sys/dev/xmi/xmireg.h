/*	$NetBSD: xmireg.h,v 1.1 2000/07/06 17:45:53 ragge Exp $	*/

/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * XMI node definitions.
 */

/*
 * XMI node addresses
 */
#define	XMI_NODESIZE	0x80000		/* Size of one XMI node (512k) */
#define	XMI_NODE(node)	(XMI_NODESIZE * (node))
#define	NNODEXMI	16	/* 16 nodes per BI */

/* XMI generic register offsets */
#define	XMI_TYPE	0
#define	XMI_BUSERR	4
#define	XMI_FAIL	8

/* Types of devices that may exist */
#define	XMIDT_KA62	0x8001		/* VAX 6200 CPU */
#define	XMIDT_KA64	0x8082		/* VAX 6400 CPU */
#define	XMIDT_KA65	0x8080		/* VAX 6500 CPU */
#define	XMIDT_KA66	0x8083		/* VAX 6600 CPU */
#define	XMIDT_ISIS	0x8081		/* MIPS R3000 CPU */
#define	XMIDT_MS62	0x4001		/* Memory */
#define	XMIDT_DWMBA	0x2001		/* XMI to BI adapter */
