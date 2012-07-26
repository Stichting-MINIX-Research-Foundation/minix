/*****************************************************************
**
**	@(#) debug.h -- macros for debug messages
**
**	compile with cc -DDBG to activate
**
**	Copyright (c) Jan 2005, Holger Zuleger HZnet. All rights reserved.
**
**	This software is open source.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	Redistributions of source code must retain the above copyright notice,
**	this list of conditions and the following disclaimer.
**
**	Redistributions in binary form must reproduce the above copyright notice,
**	this list of conditions and the following disclaimer in the documentation
**	and/or other materials provided with the distribution.
**
**	Neither the name of Holger Zuleger HZnet nor the names of its contributors may
**	be used to endorse or promote products derived from this software without
**	specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
**	TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
**	PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
**	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
**	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
**	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
**	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
**	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
**	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************/
#ifndef DEBUG_H
# define DEBUG_H

# ifdef DBG
#  define	dbg_line()	fprintf (stderr, "DBG: %s(%d) reached\n", __FILE__, __LINE__)
#  define	dbg_msg(msg)	fprintf (stderr, "DBG: %s(%d) %s\n", __FILE__, __LINE__, msg)
#  define	dbg_val0(text)	fprintf (stderr, "DBG: %s(%d) %s", __FILE__, __LINE__, text)
#  define	dbg_val1(fmt, var)	dbg_val (fmt, var)
#  define	dbg_val(fmt, var)	fprintf (stderr, "DBG: %s(%d) " fmt, __FILE__, __LINE__, var)
#  define	dbg_val2(fmt, v1, v2)	fprintf (stderr, "DBG: %s(%d) " fmt, __FILE__, __LINE__, v1, v2)
#  define	dbg_val3(fmt, v1, v2, v3)	fprintf (stderr, "DBG: %s(%d) " fmt, __FILE__, __LINE__, v1, v2, v3)
#  define	dbg_val4(fmt, v1, v2, v3, v4)	fprintf (stderr, "DBG: %s(%d) " fmt, __FILE__, __LINE__, v1, v2, v3, v4)
#  define	dbg_val5(fmt, v1, v2, v3, v4, v5)	fprintf (stderr, "DBG: %s(%d) " fmt, __FILE__, __LINE__, v1, v2, v3, v4, v5)
#  define	dbg_val6(fmt, v1, v2, v3, v4, v5, v6)	fprintf (stderr, "DBG: %s(%d) " fmt, __FILE__, __LINE__, v1, v2, v3, v4, v5, v6)
# else
#  define	dbg_line()
#  define	dbg_msg(msg)
#  define	dbg_val0(text)
#  define	dbg_val1(fmt, var)
#  define	dbg_val(fmt, str)
#  define	dbg_val2(fmt, v1, v2)
#  define	dbg_val3(fmt, v1, v2, v3)
#  define	dbg_val4(fmt, v1, v2, v3, v4)
#  define	dbg_val5(fmt, v1, v2, v3, v4, v5)
#  define	dbg_val6(fmt, v1, v2, v3, v4, v5, v6)
# endif

#endif
