/*	$NetBSD: mcp980xreg.h,v 1.2 2013/10/15 13:43:51 rkujawa Exp $ */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Radoslaw Kujawa.
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

#ifndef _MCP980XREG_H_
#define _MCP980XREG_H_

#define MCP980X_ADDR_CONST		0x48 /* 1001___ */
#define MCP980X_ADDR_VAR		0x7  /*     XXX */

#define MCP980X_AMBIENT_TEMP		0x0

#define MCP980X_AMBIENT_TEMP_00625DEGREE	__BIT(4)
#define MCP980X_AMBIENT_TEMP_0125DEGREE		__BIT(5)
#define MCP980X_AMBIENT_TEMP_025DEGREE		__BIT(6)
#define MCP980X_AMBIENT_TEMP_05DEGREE		__BIT(7)
#define MCP980X_AMBIENT_TEMP_DEGREES		__BITS(8,14)
#define MCP980X_AMBIENT_TEMP_DEGREES_SHIFT	8
#define MCP980X_AMBIENT_TEMP_SIGN		__BIT(15)

#define MCP980X_CONFIG			0x1

#define MCP980X_CONFIG_ADC_RES			__BITS(5,6)
#define MCP980X_CONFIG_ADC_RES_SHIFT		5
#define MCP980X_CONFIG_ADC_RES_9BIT		0	
#define MCP980X_CONFIG_ADC_RES_10BIT		1	
#define MCP980X_CONFIG_ADC_RES_11BIT		2	
#define MCP980X_CONFIG_ADC_RES_12BIT		3

#define MCP980X_TEMP_HYSTERESIS		0x2
#define MCP980X_TEMP_LIMIT		0x3
#define MCP980X_TEMP_HYSTLIMIT_INT_SHIFT	8

#endif /* _MCP980XREG_H_ */
