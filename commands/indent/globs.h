/**
 * Copyright (c) 1985 Sun Microsystems, Inc.
 * Copyright (c) 1980 The Regents of the University of California.
 * Copyright (c) 1976 Board of Trustees of the University of Illinois.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley, the University of Illinois,
 * Urbana, and Sun Microsystems, Inc.  The name of either University
 * or Sun Microsystems may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#)indent_globs.h	5.7 (Berkeley) 9/15/88
 */

#include <stdio.h>

#define BACKSLASH '\\'
#define bufsize 200			/* size of internal buffers */
#define inp_bufs 600			/* size of input buffer */
#define sc_size 5000			/* size of save_com buffer */
#define label_offset 2			/* number of levels a label is
					   placed to left of code */

#define tabsize 8			/* the size of a tab */
#define tabmask 0177770			/* mask used when figuring
					   length of lines with tabs */


#define false 0
#define true  !false


PUBLIC FILE    *input;			/* the fid for the input file */
PUBLIC FILE    *output;			/* the output file */

PUBLIC char    *labbuf;			/* buffer for label */
PUBLIC char    *s_lab;			/* start ... */
PUBLIC char    *e_lab;			/* .. and end of stored label */
PUBLIC char    *l_lab;			/* limit of label buffer */

PUBLIC char    *codebuf;		/* buffer for code section */
PUBLIC char    *s_code;			/* start ... */
PUBLIC char    *e_code;			/* .. and end of stored code */
PUBLIC char    *l_code;			/* limit of code section */

PUBLIC char    *combuf;			/* buffer for comments */
PUBLIC char    *s_com;			/* start ... */
PUBLIC char    *e_com;			/* ... and end of stored
					   comments */
PUBLIC char    *l_com;			/* limit of comment buffer */

PUBLIC char     in_buffer[inp_bufs];	/* input buffer */
PUBLIC char    *buf_ptr;		/* ptr to next character to be
					   taken from in_buffer */
PUBLIC char    *buf_end;		/* ptr to first after last char
					   in in_buffer */

PUBLIC char     save_com[sc_size];	/* input text is saved here
					   when looking for the brace
					   after an if, while, etc */
PUBLIC char    *sc_end;			/* pointer into save_com buffer */

PUBLIC char    *bp_save;		/* saved value of buf_ptr when
					   taking input from save_com */
PUBLIC char    *be_save;		/* similarly saved value of
					   buf_end */

PUBLIC char     token[bufsize];		/* the last token scanned */


PUBLIC int      ptr_binop;		/* pointer as binop */
PUBLIC int      bl_aft_decl;		/* blanklines after
					   declarations */
PUBLIC int      bl_bef_bk;		/* blanklines before
					   blockcomments */
PUBLIC int      bl_a_procs;		/* blanklines after procs */
PUBLIC int      bl_around;		/* blanklines around
					   conditional compilation */
PUBLIC int      swallow_opt_bl;		/* swallow optional blanklines */
PUBLIC int      n_real_blanklines;
PUBLIC int      prefix_blankline_requested;
PUBLIC int      postfix_blankline_requested;
PUBLIC int      break_comma;		/* when true and not in parens,
					   break after a comma */
PUBLIC int      btype_2;		/* when true, brace should be
					   on same line as if, while,
					   etc */
PUBLIC long     case_ind;		/* indentation level to be used
					   for a "case n:" */
PUBLIC int      code_lines;		/* count of lines with code */
PUBLIC int      had_eof;		/* set to true when input is
					   exhausted */
PUBLIC int      line_no;		/* the current line number. */
PUBLIC int      max_col;		/* the maximum allowable line
					   length */
PUBLIC int      verbose;		/* when true, non-essential
					   error messages are printed */
PUBLIC int      cuddle_else;		/* true if else should cuddle
					   up to '}' */
PUBLIC int      star_comment_cont;	/* true iff comment
					   continuation lines should
					   have stars at the beginning
					   of each line. */
PUBLIC int      del_on_bl;		/* comment_delimiter_on_blanklin
					    e */
PUBLIC int      troff;			/* true iff were generating
					   troff input */
PUBLIC int      proc_str_line;		/* if true, the names of
					   procedures being defined get
					   placed in column 1 (ie. a
					   newline is placed between
					   the type of the procedure
					   and its name) */
PUBLIC int      proc_calls_space;	/* If true, procedure calls
					   look like: foo(bar) rather
					   than foo (bar) */
PUBLIC int      format_col1_comments;	/* If comments which start in
					   column 1 are to be magically
					   reformatted (just like
					   comments that begin in later
					   columns) */
PUBLIC int      inhibit_formatting;	/* true if INDENT OFF is in
					   effect */
PUBLIC int      suppress_blanklines;	/* set iff following blanklines
					   should be suppressed */
PUBLIC int      continuation_indent;	/* set to the indentation
					   between the edge of code and
					   continuation lines */
PUBLIC int      lineup_to_parens;	/* if true, continued code
					   within parens will be lined
					   up to the open paren */
PUBLIC int      Bill_Shannon;		/* true iff a blank should
					   always be inserted after
					   sizeof */
PUBLIC int      bl_at_proctop;		/* This is vaguely similar to
					   blanklines_after_declarations
					    except that it only applies
					   to the first set of
					   declarations in a procedure
					   (just after the first '{')
					   and it causes a blank line
					   to be generated even if
					   there are no declarations */
PUBLIC int      bk_max_col;
PUBLIC int      ex_expr_indent;		/* True if continuation lines
					   from the expression part of
					   "if(e)", "while(e)",
					   "for(e;e;e)" should be
					   indented an extra tab stop
					   so that they don't conflict
					   with the code that follows */

/* -troff font state information */

struct fstate
{
   char            font[4];
   char            size;
   int             allcaps;
};

PUBLIC struct fstate
                keywordf,		/* keyword font */
                stringf,		/* string font */
                boxcomf,		/* Box comment font */
                blkcomf,		/* Block comment font */
                scomf,			/* Same line comment font */
                bodyf;			/* major body font */


#define STACKSIZE 150

PUBLIC struct parser_state
{
   int             last_token;
   struct fstate   cfont;		/* Current font */
   int             p_stack[STACKSIZE];	/* this is the parsers stack */
   int             il[STACKSIZE];	/* this stack stores
					   indentation levels */
   long            cstk[STACKSIZE];	/* used to store case stmt
					   indentation levels */
   int             box_com;		/* set to true when we are in a
					   "boxed" comment. In that
					   case, the first non-blank
					   char should be lined up with
					   the / in / *     */
   int             comment_delta, n_comment_delta;
   int             cast_mask;		/* indicates which close parens
					   close off casts */
   int             sizeof_mask;		/* indicates which close parens
					   close off sizeof''s */
   int             block_init;		/* true iff inside a block
					   initialization */
   int             block_init_level;	/* The level of brace nesting
					   in an initialization */
   int             last_nl;		/* this is true if the last
					   thing scanned was a newline */
   int             in_or_st;		/* Will be true iff there has
					   been a declarator (e.g. int
					   or char) and no left paren
					   since the last semicolon.
					   When true, a '{' is starting
					   a structure definition or an
					   initialization list */
   int             bl_line;		/* set to 1 by dump_line if the
					   line is blank */
   int             col_1;		/* set to true if the last
					   token started in column 1 */
   int             com_col;		/* this is the column in which
					   the current coment should
					   start */
   int             com_ind;		/* the column in which comments
					   to the right of code should
					   start */
   int             com_lines;		/* the number of lines with
					   comments, set by dump_line */
   int             dec_nest;		/* current nesting level for
					   structure or init */
   int             decl_com_ind;	/* the column in which comments
					   after declarations should be
					   put */
   int             decl_on_line;	/* set to true if this line of
					   code has part of a
					   declaration on it */
   int             i_l_follow;		/* the level to which ind_level
					   should be set after the
					   current line is printed */
   int             in_decl;		/* set to true when we are in a
					   declaration stmt.  The
					   processing of braces is then
					   slightly different */
   int             in_stmt;		/* set to 1 while in a stmt */
   int             ind_level;		/* the current indentation
					   level */
   int             ind_size;		/* the size of one indentation
					   level */
   int             ind_stmt;		/* set to 1 if next line should
					   have an extra indentation
					   level because we are in the
					   middle of a stmt */
   int             last_u_d;		/* set to true after scanning a
					   token which forces a
					   following operator to be
					   unary */
   int             leave_comma;		/* if true, never break
					   declarations after commas */
   int             ljust_decl;		/* true if declarations should
					   be left justified */
   int             out_coms;		/* the number of comments
					   processed, set by pr_comment */
   int             out_lines;		/* the number of lines written,
					   set by dump_line */
   int             p_l_follow;		/* used to remember how to
					   indent following statement */
   int             paren_level;		/* parenthesization level. used
					   to indent within stmts */
   short           paren_indents[20];	/* column positions of each
					   paren */
   int             pcase;		/* set to 1 if the current line
					   label is a case.  It is
					   printed differently from a
					   regular label */
   int             search_brace;	/* set to true by parse when it
					   is necessary to buffer up
					   all info up to the start of
					   a stmt after an if, while,
					   etc */
   int             unindent_displace;	/* comments not to the right of
					   code will be placed this
					   many indentation levels to
					   the left of code */
   int             use_ff;		/* set to one if the current
					   line should be terminated
					   with a form feed */
   int             want_blank;		/* set to true when the
					   following token should be
					   prefixed by a blank. (Said
					   prefixing is ignored in some
					   cases.) */
   int             else_if;		/* True iff else if pairs
					   should be handled specially */
   int             decl_indent;		/* column to indent declared
					   identifiers to */
   int             its_a_keyword;
   int             sizeof_keyword;
   int             dumped_decl_indent;
   int            case_indent;		/* The distance to indent case
					   labels from the switch
					   statement */
   int             in_par_decl;
   int             indent_parameters;
   int             tos;			/* pointer to top of stack */
   char            procname[100];	/* The name of the current
					   procedure */
   int             just_saw_decl;
}               ps;

PUBLIC int      ifdef_level;
PUBLIC struct parser_state state_stack[5];
PUBLIC struct parser_state match_state[5];
