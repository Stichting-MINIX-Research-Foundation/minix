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
 *  Top - a top users display for Unix
 *
 *  This file defines the default locations on the screen for various parts
 *  of the display.  These definitions are used by the routines in "display.c"
 *  for cursor addressing.
 */

#define  X_LASTPID	10
#define  Y_LASTPID	0
#define  X_LASTPIDWIDTH 13
#define  X_LOADAVE	27
#define  Y_LOADAVE	0
#define  X_LOADAVEWIDTH 7
#define  X_MINIBAR      50
#define  Y_MINIBAR      0
#define  X_UPTIME       48
#define  Y_UPTIME       0
#define  X_PROCSTATE	15
#define  Y_PROCSTATE	1
#define  X_BRKDN	15
#define  Y_BRKDN	1
#define  X_CPUSTATES	0
#define  Y_CPUSTATES	2
#define  X_KERNEL       8
#define  Y_KERNEL       3
#define  X_MEM		8
#define  Y_MEM		3
#define  X_SWAP		6
#define  Y_SWAP		4
#define  Y_MESSAGE	4
#define  X_HEADER	0
#define  Y_HEADER	5
#define  X_IDLECURSOR	0
#define  Y_IDLECURSOR	4
#define  Y_PROCS	6

