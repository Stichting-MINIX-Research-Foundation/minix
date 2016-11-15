/*	$NetBSD: i128var.h,v 1.3 2012/10/20 13:31:09 macallan Exp $ */

/*-
 * Copyright (c) 2007 Michael Lorenz
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
__KERNEL_RCSID(0, "$NetBSD: i128var.h,v 1.3 2012/10/20 13:31:09 macallan Exp $");

/* 
 * register definition for Number Nine Imagine 128 graphics controllers
 *
 * adapted from XFree86's i128 driver source
 */

#ifndef I128VAR_H
#define I128VAR_H

/* tag, handle, stride, colour depth */
void i128_init(bus_space_tag_t, bus_space_handle_t, int, int);
void i128_bitblt(bus_space_tag_t, bus_space_handle_t, int, int, int, int, int,
    int, int);
void i128_rectfill(bus_space_tag_t, bus_space_handle_t, int, int, int, int,
    uint32_t);
void i128_ready(bus_space_tag_t, bus_space_handle_t);
void i128_sync(bus_space_tag_t, bus_space_handle_t);


#endif /* I128VAR_H */
