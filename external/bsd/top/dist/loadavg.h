/*
 * Copyright (c) 1984 through 2008, William LeFebvre
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * 
 *     * Neither the name of William LeFebvre nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Top - a top users display for Berkeley Unix
 *
 *  Defines required to access load average figures.
 *
 *  This include file sets up everything we need to access the load average
 *  values in the kernel in a machine independent way.  First, it sets the
 *  typedef "load_avg" to be either double or long (depending on what is
 *  needed), then it defines these macros appropriately:
 *
 *	loaddouble(la) - convert load_avg to double.
 *	intload(i)     - convert integer to load_avg.
 */

/*
 * We assume that if FSCALE is defined, then avenrun and ccpu are type long.
 * If your machine is an exception (mips, perhaps?) then make adjustments
 * here.
 *
 * Defined types:  load_avg for load averages, pctcpu for cpu percentages.
 */
#if defined(mips) && !defined(NetBSD)
# include <sys/fixpoint.h>
# if defined(FBITS) && !defined(FSCALE)
#  define FSCALE (1 << FBITS)	/* mips */
# endif
#endif

#ifdef FSCALE
# define FIXED_LOADAVG FSCALE
# define FIXED_PCTCPU FSCALE
#endif

#ifdef ibm032
# undef FIXED_LOADAVG
# undef FIXED_PCTCPU
# define FIXED_PCTCPU PCT_SCALE
#endif


#ifdef FIXED_PCTCPU
  typedef long pctcpu;
# define pctdouble(p) ((double)(p) / FIXED_PCTCPU)
#else
typedef double pctcpu;
# define pctdouble(p) (p)
#endif

#ifdef FIXED_LOADAVG
  typedef long load_avg;
# define loaddouble(la) ((double)(la) / FIXED_LOADAVG)
# define intload(i) ((int)((i) * FIXED_LOADAVG))
#else
  typedef double load_avg;
# define loaddouble(la) (la)
# define intload(i) ((double)(i))
#endif
