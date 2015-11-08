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
 * Top - a top users display for Unix
 *
 * Definition of the color interface.
 */

#ifndef _COLOR_H_
#define _COLOR_H_

#define COLOR_ANSI_SLOTS 20

int color_env_parse(char *env);
int color_tag(const char *tag);
int color_test(int tagidx, int value);
char *color_setstr(int color);
void color_dump(FILE *f);
int color_activate(int i);


/*
 * These color tag names are currently in use
 * (or reserved for future use):
 *
 * cpu, size, res, time, 1min, 5min, 15min, host
 */

/*
 * Valid ANSI values for colors are:
 *
 * 0	Reset all attributes
 * 1	Bright
 * 2	Dim
 * 4	Underscore	
 * 5	Blink
 * 7	Reverse
 * 8	Hidden
 * 
 * 	Foreground Colours
 * 30	Black
 * 31	Red
 * 32	Green
 * 33	Yellow
 * 34	Blue
 * 35	Magenta
 * 36	Cyan
 * 37	White
 * 
 * 	Background Colours
 * 40	Black
 * 41	Red
 * 42	Green
 * 43	Yellow
 * 44	Blue
 * 45	Magenta
 * 46	Cyan
 * 47	White
 */

#endif /*_COLOR_H_ */
