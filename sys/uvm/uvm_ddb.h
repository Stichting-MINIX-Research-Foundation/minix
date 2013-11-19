/*	$NetBSD: uvm_ddb.h,v 1.15 2011/05/17 04:18:07 mrg Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 *
 * from: Id: uvm_extern.h,v 1.1.2.21 1998/02/07 01:16:53 chs Exp
 */

#ifndef _UVM_UVM_DDB_H_
#define _UVM_UVM_DDB_H_

#ifdef _KERNEL

#if defined(DDB) || defined(DEBUGPRINT)
void	uvm_map_printit(struct vm_map *, bool,
	    void (*)(const char *, ...));
void	uvm_object_printit(struct uvm_object *, bool,
	    void (*)(const char *, ...));
void	uvm_page_printit(struct vm_page *, bool,
	    void (*)(const char *, ...));
void	uvm_page_printall(void (*)(const char *, ...));
void	uvmexp_print(void (*)(const char *, ...));
#endif /* DDB || DEBUGPRINT */

#endif /* _KERNEL */

#endif /* _UVM_UVM_DDB_H_ */
