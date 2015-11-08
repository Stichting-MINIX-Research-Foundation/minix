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
 *  top - a top users display for Unix 4.2
 *
 *  This file contains all the definitions necessary to use the hand-written
 *  screen package in "screen.c"
 */

#ifndef _SCREEN_H_
#define _SCREEN_H_

extern char ch_erase;		/* set to the user's erase character */
extern char ch_kill;		/* set to the user's kill character */
extern char ch_werase;		/* set to the user's werase character */
extern char smart_terminal;     /* set if the terminal has sufficient termcap
				   capabilities for normal operation */

/* rows and columns on the screen according to termcap */
extern int  screen_length;
extern int  screen_width;

void screen_getsize(void);
int screen_readtermcap(int interactive);
void screen_init(void);
void screen_end(void);
void screen_reinit(void);
void screen_move(int x, int y);
void screen_standout(const char *msg);
void screen_clear(void);
int screen_cte(void);
void screen_cleareol(int len);
void screen_home(void);

#endif /* _SCREEN_H_ */
