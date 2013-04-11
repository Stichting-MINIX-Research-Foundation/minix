/*	$NetBSD: multi.c,v 1.1.1.5 2008/09/02 07:50:44 christos Exp $	*/

/* multi.c -- multiple-column tables (@multitable) for makeinfo.
   Id: multi.c,v 1.8 2004/04/11 17:56:47 karl Exp

   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2004 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   
   Originally written by phr@gnu.org (Paul Rubin).  */

#include "system.h"
#include "cmds.h"
#include "insertion.h"
#include "makeinfo.h"
#include "multi.h"
#include "xml.h"

#define MAXCOLS 100             /* remove this limit later @@ */


/*
 * Output environments.  This is a hack grafted onto existing
 * structure.  The "output environment" used to consist of the
 * global variables `output_paragraph', `fill_column', etc.
 * Routines like add_char would manipulate these variables.
 *
 * Now, when formatting a multitable, we maintain separate environments
 * for each column.  That way we can build up the columns separately
 * and write them all out at once.  The "current" output environment"
 * is still kept in those global variables, so that the old output
 * routines don't have to change.  But we provide routines to save
 * and restore these variables in an "environment table".  The
 * `select_output_environment' function switches from one output
 * environment to another.
 *
 * Environment #0 (i.e., element #0 of the table) is the regular
 * environment that is used when we're not formatting a multitable.
 *
 * Environment #N (where N = 1,2,3,...) is the env. for column #N of
 * the table, when a multitable is active.
 */

/* contents of an output environment */
/* some more vars may end up being needed here later @@ */
struct env
{
  unsigned char *output_paragraph;
  int output_paragraph_offset;
  int meta_char_pos;
  int output_column;
  int paragraph_is_open;
  int current_indent;
  int fill_column;
} envs[MAXCOLS];                /* the environment table */

/* index in environment table of currently selected environment */
static int current_env_no;

/* current column number */
static int current_column_no;

/* We need to make a difference between template based widths and
   @columnfractions for HTML tables' sake.  Sigh.  */
static int seen_column_fractions;

/* column number of last column in current multitable */
static int last_column;

/* flags indicating whether horizontal and vertical separators need
   to be drawn, separating rows and columns in the current multitable. */
static int hsep, vsep;

/* whether this is the first row. */
static int first_row;

/* Called to handle a {...} template on the @multitable line.
   We're at the { and our first job is to find the matching }; as a side
   effect, we change *PARAMS to point to after it.  Our other job is to
   expand the template text and return the width of that string.  */
static unsigned
find_template_width (char **params)
{
  char *template, *xtemplate;
  unsigned len;
  char *start = *params;
  int brace_level = 0;

  /* The first character should be a {.  */
  if (!params || !*params || **params != '{')
    {
      line_error ("find_template width internal error: passed %s",
                  params ? *params : "null");
      return 0;
    }

  do
    {
      if (**params == '{' && (*params == start || (*params)[-1] != '@'))
        brace_level++;
      else if (**params == '}' && (*params)[-1] != '@')
        brace_level--;
      else if (**params == 0)
        {
          line_error (_("Missing } in @multitable template"));
          return 0;
        }
      (*params)++;
    }
  while (brace_level > 0);
  
  template = substring (start + 1, *params - 1); /* omit braces */
  xtemplate = expansion (template, 0);
  len = strlen (xtemplate);
  
  free (template);
  free (xtemplate);
  
  return len;
}

/* Direct current output to environment number N.  Used when
   switching work from one column of a multitable to the next.
   Returns previous environment number. */
static int 
select_output_environment (int n)
{
  struct env *e = &envs[current_env_no];
  int old_env_no = current_env_no;

  /* stash current env info from global vars into the old environment */
  e->output_paragraph = output_paragraph;
  e->output_paragraph_offset = output_paragraph_offset;
  e->meta_char_pos = meta_char_pos;
  e->output_column = output_column;
  e->paragraph_is_open = paragraph_is_open;
  e->current_indent = current_indent;
  e->fill_column = fill_column;

  /* now copy new environment into global vars */
  current_env_no = n;
  e = &envs[current_env_no];
  output_paragraph = e->output_paragraph;
  output_paragraph_offset = e->output_paragraph_offset;
  meta_char_pos = e->meta_char_pos;
  output_column = e->output_column;
  paragraph_is_open = e->paragraph_is_open;
  current_indent = e->current_indent;
  fill_column = e->fill_column;
  return old_env_no;
}

/* Initialize environment number ENV_NO, of width WIDTH.
   The idea is that we're going to use one environment for each column of
   a multitable, so we can build them up separately and print them
   all out at the end. */
static int
setup_output_environment (int env_no, int width)
{
  int old_env = select_output_environment (env_no);

  /* clobber old environment and set width of new one */
  init_paragraph ();

  /* make our change */
  fill_column = width;

  /* Save new environment and restore previous one. */
  select_output_environment (old_env);

  return env_no;
}

/* Read the parameters for a multitable from the current command
   line, save the parameters away, and return the
   number of columns. */
static int
setup_multitable_parameters (void)
{
  char *params = insertion_stack->item_function;
  int nchars;
  float columnfrac;
  char command[200]; /* xx no fixed limits */
  int i = 1;

  /* We implement @hsep and @vsep even though TeX doesn't.
     We don't get mixing of @columnfractions and templates right,
     but TeX doesn't either.  */
  hsep = vsep = 0;

  /* Assume no @columnfractions per default.  */
  seen_column_fractions = 0;

  while (*params) {
    while (whitespace (*params))
      params++;

    if (*params == '@') {
      sscanf (params, "%200s", command);
      nchars = strlen (command);
      params += nchars;
      if (strcmp (command, "@hsep") == 0)
        hsep++;
      else if (strcmp (command, "@vsep") == 0)
        vsep++;
      else if (strcmp (command, "@columnfractions") == 0) {
        seen_column_fractions = 1;
        /* Clobber old environments and create new ones, starting at #1.
           Environment #0 is the normal output, so don't mess with it. */
        for ( ; i <= MAXCOLS; i++) {
          if (sscanf (params, "%f", &columnfrac) < 1)
            goto done;
          /* Unfortunately, can't use %n since m68k-hp-bsd libc (at least)
             doesn't support it.  So skip whitespace (preceding the
             number) and then non-whitespace (the number).  */
          while (*params && (*params == ' ' || *params == '\t'))
            params++;
          /* Hmm, but what about @columnfractions 3foo.  Oh well, 
             it's invalid input anyway.  */
          while (*params && *params != ' ' && *params != '\t'
                 && *params != '\n' && *params != '@')
            params++;

          {
            /* For html/xml/docbook, translate fractions into integer
               percentages, adding .005 to avoid rounding problems.  For
               info, we want the character width.  */
            int width = xml || html ? (columnfrac + .005) * 100
                        : (columnfrac * (fill_column - current_indent) + .5);
            setup_output_environment (i, width);
          }
        }
      }

    } else if (*params == '{') {
      unsigned template_width = find_template_width (&params);

      /* This gives us two spaces between columns.  Seems reasonable.
         How to take into account current_indent here?  */
      setup_output_environment (i++, template_width + 2);
      
    } else {
      warning (_("ignoring stray text `%s' after @multitable"), params);
      break;
    }
  }

done:
  flush_output ();
  inhibit_output_flushing ();

  last_column = i - 1;
  return last_column;
}

/* Output a row.  Calls insert, but also flushes the buffered output
   when we see a newline, since in multitable every line is a separate
   paragraph.  */
static void
out_char (int ch)
{
  if (html || xml)
    add_char (ch);
  else
    {
      int env = select_output_environment (0);
      insert (ch);
      if (ch == '\n')
	{
	  uninhibit_output_flushing ();
	  flush_output ();
	  inhibit_output_flushing ();
	}
      select_output_environment (env);
    }
}


static void
draw_horizontal_separator (void)
{
  int i, j, s;

  if (html)
    {
      add_word ("<hr>");
      return;
    }
  if (xml)
    return;

  for (s = 0; s < envs[0].current_indent; s++)
    out_char (' ');
  if (vsep)
    out_char ('+');
  for (i = 1; i <= last_column; i++) {
    for (j = 0; j <= envs[i].fill_column; j++)
      out_char ('-');
    if (vsep)
      out_char ('+');
  }
  out_char (' ');
  out_char ('\n');
}


/* multitable strategy:
    for each item {
       for each column in an item {
        initialize a new paragraph
        do ordinary formatting into the new paragraph
        save the paragraph away
        repeat if there are more paragraphs in the column
      }
      dump out the saved paragraphs and free the storage
    }

   For HTML we construct a simple HTML 3.2 table with <br>s inserted
   to help non-tables browsers.  `@item' inserts a <tr> and `@tab'
   inserts <td>; we also try to close <tr>.  The only real
   alternative is to rely on the info formatting engine and present
   preformatted text.  */

void
do_multitable (void)
{
  int ncolumns;

  if (multitable_active)
    {
      line_error ("Multitables cannot be nested");
      return;
    }

  close_single_paragraph ();

  if (xml)
    {
      xml_no_para = 1;
      if (output_paragraph[output_paragraph_offset-1] == '\n')
        output_paragraph_offset--;
    }

  /* scan the current item function to get the field widths
     and number of columns, and set up the output environment list
     accordingly. */
  ncolumns = setup_multitable_parameters ();
  first_row = 1;

  /* <p> for non-tables browsers.  @multitable implicitly ends the
     current paragraph, so this is ok.  */
  if (html)
    add_html_block_elt ("<p><table summary=\"\">");
  /*  else if (docbook)*/ /* 05-08 */
  else if (xml)
    {
      int *widths = xmalloc (ncolumns * sizeof (int));
      int i;
      for (i=0; i<ncolumns; i++)
	widths[i] = envs[i+1].fill_column;
      xml_begin_multitable (ncolumns, widths);
      free (widths);
    }

  if (hsep)
    draw_horizontal_separator ();

  /* The next @item command will direct stdout into the first column
     and start processing.  @tab will then switch to the next column,
     and @item will flush out the saved output and return to the first
     column.  Environment #1 is the first column.  (Environment #0 is
     the normal output) */

  ++multitable_active;
}

/* advance to the next environment number */
static void
nselect_next_environment (void)
{
  if (current_env_no >= last_column) {
    line_error (_("Too many columns in multitable item (max %d)"), last_column);
    return;
  }
  select_output_environment (current_env_no + 1);
}


/* do anything needed at the beginning of processing a
   multitable column. */
static void
init_column (void)
{
  /* don't indent 1st paragraph in the item */
  cm_noindent ();

  /* throw away possible whitespace after @item or @tab command */
  skip_whitespace ();
}

static void
output_multitable_row (void)
{
  /* offset in the output paragraph of the next char needing
     to be output for that column. */
  int offset[MAXCOLS];
  int i, j, s, remaining;
  int had_newline = 0;

  for (i = 0; i <= last_column; i++)
    offset[i] = 0;

  /* select the current environment, to make sure the env variables
     get updated */
  select_output_environment (current_env_no);

#define CHAR_ADDR(n) (offset[i] + (n))
#define CHAR_AT(n) (envs[i].output_paragraph[CHAR_ADDR(n)])

  /* remove trailing whitespace from each column */
  for (i = 1; i <= last_column; i++) {
    if (envs[i].output_paragraph_offset)
      while (cr_or_whitespace (CHAR_AT (envs[i].output_paragraph_offset - 1)))
        envs[i].output_paragraph_offset--;

    if (i == current_env_no)
      output_paragraph_offset = envs[i].output_paragraph_offset;
  }

  /* read the current line from each column, outputting them all
     pasted together.  Do this til all lines are output from all
     columns.  */
  for (;;) {
    remaining = 0;
    /* first, see if there is any work to do */
    for (i = 1; i <= last_column; i++) {
      if (CHAR_ADDR (0) < envs[i].output_paragraph_offset) {
        remaining = 1;
        break;
      }
    }
    if (!remaining)
      break;
    
    for (s = 0; s < envs[0].current_indent; s++)
      out_char (' ');
    
    if (vsep)
      out_char ('|');

    for (i = 1; i <= last_column; i++) {
      for (s = 0; s < envs[i].current_indent; s++)
        out_char (' ');
      for (j = 0; CHAR_ADDR (j) < envs[i].output_paragraph_offset; j++) {
        if (CHAR_AT (j) == '\n')
          break;
        out_char (CHAR_AT (j));
      }
      offset[i] += j + 1;       /* skip last text plus skip the newline */
      
      /* Do not output trailing blanks if we're in the last column and
         there will be no trailing |.  */
      if (i < last_column && !vsep)
        for (; j <= envs[i].fill_column; j++)
          out_char (' ');
      if (vsep)
        out_char ('|'); /* draw column separator */
    }
    out_char ('\n');    /* end of line */
    had_newline = 1;
  }
  
  /* If completely blank item, get blank line despite no other output.  */
  if (!had_newline)
    out_char ('\n');    /* end of line */

  if (hsep)
    draw_horizontal_separator ();

  /* Now dispose of the buffered output. */
  for (i = 1; i <= last_column; i++) {
    select_output_environment (i);
    init_paragraph ();
  }
}

int after_headitem = 0;
int headitem_row = 0;

/* start a new item (row) of a multitable */
int
multitable_item (void)
{
  if (!multitable_active) {
    line_error ("multitable_item internal error: no active multitable");
    xexit (1);
  }

  current_column_no = 1;

  if (html)
    {
      if (!first_row)
        /* <br> for non-tables browsers. */
	add_word_args ("<br></%s></tr>", after_headitem ? "th" : "td");

      if (seen_column_fractions)
        add_word_args ("<tr align=\"left\"><%s valign=\"top\" width=\"%d%%\">",
            headitem_flag ? "th" : "td",
            envs[current_column_no].fill_column);
      else
        add_word_args ("<tr align=\"left\"><%s valign=\"top\">",
            headitem_flag ? "th" : "td");

      if (headitem_flag)
        after_headitem = 1;
      else
        after_headitem = 0;
      first_row = 0;
      headitem_row = headitem_flag;
      headitem_flag = 0;
      return 0;
    }
  /*  else if (docbook)*/ /* 05-08 */
  else if (xml)
    {
      xml_end_multitable_row (first_row);
      if (headitem_flag)
        after_headitem = 1;
      else
        after_headitem = 0;
      first_row = 0;
      headitem_flag = 0;
      return 0;
    }
  first_row = 0;

  if (current_env_no > 0) {
    output_multitable_row ();
  }
  /* start at column 1 */
  select_output_environment (1);
  if (!output_paragraph) {
    line_error (_("[unexpected] cannot select column #%d in multitable"),
                current_env_no);
    xexit (1);
  }

  init_column ();

  if (headitem_flag)
    hsep = 1;
  else
    hsep = 0;

  if (headitem_flag)
    after_headitem = 1;
  else
    after_headitem = 0;
  headitem_flag = 0;

  return 0;
}

#undef CHAR_AT
#undef CHAR_ADDR

/* select a new column in current row of multitable */
void
cm_tab (void)
{
  if (!multitable_active)
    error (_("ignoring @tab outside of multitable"));

  current_column_no++;
  
  if (html)
    {
      if (seen_column_fractions)
        add_word_args ("</%s><%s valign=\"top\" width=\"%d%%\">",
            headitem_row ? "th" : "td",
            headitem_row ? "th" : "td",
            envs[current_column_no].fill_column);
      else
        add_word_args ("</%s><%s valign=\"top\">",
            headitem_row ? "th" : "td",
            headitem_row ? "th" : "td");
    }
  /*  else if (docbook)*/ /* 05-08 */
  else if (xml)
    xml_end_multitable_column ();
  else
    nselect_next_environment ();

  init_column ();
}

/* close a multitable, flushing its output and resetting
   whatever needs resetting */
void
end_multitable (void)
{
  if (!html && !docbook) 
    output_multitable_row ();

  /* Multitables cannot be nested.  Otherwise, we'd have to save the
     previous output environment number on a stack somewhere, and then
     restore to that environment.  */
  select_output_environment (0);
  multitable_active = 0;
  uninhibit_output_flushing ();
  close_insertion_paragraph ();

  if (html)
    add_word_args ("<br></%s></tr></table>\n", headitem_row ? "th" : "td");
  /*  else if (docbook)*/ /* 05-08 */
  else if (xml)
    xml_end_multitable ();

#if 0
  printf (_("** Multicolumn output from last row:\n"));
  for (i = 1; i <= last_column; i++) {
    select_output_environment (i);
    printf (_("* column #%d: output = %s\n"), i, output_paragraph);
  }
#endif
}
