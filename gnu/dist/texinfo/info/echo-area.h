/*	$NetBSD: echo-area.h,v 1.1.1.4 2008/09/02 07:49:37 christos Exp $	*/

/* echo-area.h -- Functions used in reading information from the echo area.
   Id: echo-area.h,v 1.4 2004/08/07 22:03:08 karl Exp

   This file is part of GNU Info, a program for reading online documentation
   stored in Info format.

   Copyright (C) 1993, 1997, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#ifndef INFO_ECHO_AREA_H
#define INFO_ECHO_AREA_H

#define EA_MAX_INPUT 256

extern int echo_area_is_active, info_aborted_echo_area;

/* Non-zero means that the last command executed while reading input
   killed some text. */
extern int echo_area_last_command_was_kill;

extern void inform_in_echo_area (const char *message);
extern void echo_area_inform_of_deleted_window (WINDOW *window);
extern void echo_area_prep_read (void);
extern VFunction *ea_last_executed_command;
extern char * info_read_completing_internal (WINDOW *window, char *prompt,
    REFERENCE **completions, int force);

/* Read a line of text in the echo area.  Return a malloc ()'ed string,
   or NULL if the user aborted out of this read.  WINDOW is the currently
   active window, so that we can restore it when we need to.  PROMPT, if
   non-null, is a prompt to print before reading the line. */
extern char *info_read_in_echo_area (WINDOW *window, char *prompt);

/* Read a line in the echo area with completion over COMPLETIONS.
   Takes arguments of WINDOW, PROMPT, and COMPLETIONS, a REFERENCE **. */
char *info_read_completing_in_echo_area (WINDOW *window,
    char *prompt, REFERENCE **completions);

/* Read a line in the echo area allowing completion over COMPLETIONS, but
   not requiring it.  Takes arguments of WINDOW, PROMPT, and COMPLETIONS,
   a REFERENCE **. */
extern char *info_read_maybe_completing (WINDOW *window,
    char *prompt, REFERENCE **completions);

extern void ea_insert (WINDOW *window, int count, unsigned char key);
extern void ea_quoted_insert (WINDOW *window, int count, unsigned char key);
extern void ea_beg_of_line (WINDOW *window, int count, unsigned char key);
extern void ea_backward (WINDOW *window, int count, unsigned char key);
extern void ea_delete (WINDOW *window, int count, unsigned char key);
extern void ea_end_of_line (WINDOW *window, int count, unsigned char key);
extern void ea_forward (WINDOW *window, int count, unsigned char key);
extern void ea_abort (WINDOW *window, int count, unsigned char key);
extern void ea_rubout (WINDOW *window, int count, unsigned char key);
extern void ea_complete (WINDOW *window, int count, unsigned char key);
extern void ea_newline (WINDOW *window, int count, unsigned char key);
extern void ea_kill_line (WINDOW *window, int count, unsigned char key);
extern void ea_backward_kill_line (WINDOW *window, int count, unsigned char key);
extern void ea_transpose_chars (WINDOW *window, int count, unsigned char key);
extern void ea_yank (WINDOW *window, int count, unsigned char key);
extern void ea_tab_insert (WINDOW *window, int count, unsigned char key);
extern void ea_possible_completions (WINDOW *window, int count, unsigned char key);
extern void ea_backward_word (WINDOW *window, int count, unsigned char key);
extern void ea_kill_word (WINDOW *window, int count, unsigned char key);
extern void ea_forward_word (WINDOW *window, int count, unsigned char key);
extern void ea_yank_pop (WINDOW *window, int count, unsigned char key);
extern void ea_backward_kill_word (WINDOW *window, int count, unsigned char key);
extern void ea_scroll_completions_window (WINDOW *window, int count,
    unsigned char key);

#endif /* not INFO_ECHO_AREA_H */
