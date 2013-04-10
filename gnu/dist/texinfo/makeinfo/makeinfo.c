/*	$NetBSD: makeinfo.c,v 1.16 2009/02/28 19:51:13 joerg Exp $	*/

/* makeinfo -- convert Texinfo source into other formats.
   Id: makeinfo.c,v 1.74 2004/12/19 17:15:42 karl Exp

   Copyright (C) 1987, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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

   Original author of makeinfo: Brian Fox (bfox@ai.mit.edu).  */

#include "system.h"
#include "getopt.h"

#define COMPILING_MAKEINFO
#include "makeinfo.h"
#include "cmds.h"
#include "files.h"
#include "float.h"
#include "footnote.h"
#include "html.h"
#include "index.h"
#include "insertion.h"
#include "lang.h"
#include "macro.h"
#include "node.h"
#include "sectioning.h"
#include "toc.h"
#include "xml.h"

/* You can change some of the behavior of Makeinfo by changing the
   following defines: */

/* Define INDENT_PARAGRAPHS_IN_TABLE if you want the paragraphs which
   appear within an @table, @ftable, or @itemize environment to have
   standard paragraph indentation.  Without this, such paragraphs have
   no starting indentation. */
/* #define INDENT_PARAGRAPHS_IN_TABLE */

/* Define PARAGRAPH_START_INDENT to be the amount of indentation that
   the first lines of paragraphs receive by default, where no other
   value has been specified.  Users can change this value on the command
   line, with the --paragraph-indent option, or within the texinfo file,
   with the @paragraphindent command. */
#define PARAGRAPH_START_INDENT 3

/* Define DEFAULT_PARAGRAPH_SPACING as the number of blank lines that you
   wish to appear between paragraphs.  A value of 1 creates a single blank
   line between paragraphs.  Paragraphs are defined by 2 or more consecutive
   newlines in the input file (i.e., one or more blank lines). */
#define DEFAULT_PARAGRAPH_SPACING 1

/* Global variables.  */

/* The output file name. */
char *output_filename = NULL;

/* Name of the output file that the user elected to pass on the command line.
   Such a name overrides any name found with the @setfilename command. */
char *command_output_filename = NULL;
static char *save_command_output_filename = NULL;

#define INITIAL_PARAGRAPH_SPACE 5000
int paragraph_buffer_len = INITIAL_PARAGRAPH_SPACE;

/* The amount of indentation to add at the starts of paragraphs.
   0 means don't change existing indentation at paragraph starts.
   > 0 is amount to indent new paragraphs by.
   < 0 means indent to column zero by removing indentation if necessary.

   This is normally zero, but some people prefer paragraph starts to be
   somewhat more indented than paragraph bodies.  A pretty value for
   this is 3. */
int paragraph_start_indent = PARAGRAPH_START_INDENT;

/* Indentation that is pending insertion.  We have this for hacking lines
   which look blank, but contain whitespace.  We want to treat those as
   blank lines. */
int pending_indent = 0;

/* The index in our internal command table of the currently
   executing command. */
int command_index;

/* A search string which is used to find the first @setfilename. */
char setfilename_search[] =
  { COMMAND_PREFIX,
      's', 'e', 't', 'f', 'i', 'l', 'e', 'n', 'a', 'm', 'e', 0 };

/* Values for calling handle_variable_internal (). */
#define SET     1
#define CLEAR   2
#define IFSET   3
#define IFCLEAR 4

/* Flags controlling the operation of the program. */

/* Default is to remove output if there were errors.  */
int force = 0;

/* Default is to notify users of bad choices. */
int print_warnings = 1;

/* Number of errors that we tolerate on a given fileset. */
int max_error_level = 100;

/* The actual last inserted character.  Note that this may be something
   other than NEWLINE even if last_char_was_newline is 1. */
int last_inserted_character = 0;

/* Nonzero means that a newline character has already been
   inserted, so close_paragraph () should insert one less. */
int line_already_broken = 0;

/* When nonzero we have finished an insertion (see end_insertion ()) and we
   want to ignore false continued paragraph closings. */
int insertion_paragraph_closed = 0;

/* Nonzero means attempt to make all of the lines have fill_column width. */
int do_justification = 0;

/* Nonzero means don't replace whitespace with &nbsp; in HTML mode.  */
int in_html_elt = 0;

/* Nonzero means we are inserting a block level HTML element that must not be
   enclosed in a <p>, such as <ul>, <ol> and <h?>.  */
int in_html_block_level_elt = 0;

/* True when expanding a macro definition.  */
static int executing_macro = 0;

/* True when we are inside a <li> block of a menu.  */
static int in_menu_item = 0;

typedef struct brace_element
{
  struct brace_element *next;
  COMMAND_FUNCTION *proc;
  char *command;
  int pos, line;
  int in_fixed_width_font;
} BRACE_ELEMENT;

BRACE_ELEMENT *brace_stack = NULL;

static void convert_from_file (char *name);
static void convert_from_loaded_file (char *name);
static void convert_from_stream (FILE *stream, char *name);
static void do_flush_right_indentation (void);
static void handle_variable (int action);
static void handle_variable_internal (int action, char *name);
static void init_brace_stack (void);
static void init_internals (void);
static void pop_and_call_brace (void);
static void remember_brace (COMMAND_FUNCTION (*proc));
static int end_of_sentence_p (void);

void maybe_update_execution_strings (char **text, unsigned int new_len);

/* Error handling.  */

/* Number of errors encountered. */
int errors_printed = 0;

/* Remember that an error has been printed.  If more than
   max_error_level have been printed, then exit the program. */
static void
remember_error (void)
{
  errors_printed++;
  if (max_error_level && (errors_printed > max_error_level))
    {
      fprintf (stderr, _("Too many errors!  Gave up.\n"));
      flush_file_stack ();
      if (errors_printed - max_error_level < 2)
	cm_bye ();
      xexit (1);
    }
}

/* Print the last error gotten from the file system. */
int
fs_error (char *filename)
{
  remember_error ();
  perror (filename);
  return 0;
}

/* Print an error message, and return false. */
void
#if defined (VA_FPRINTF) && __STDC__
error (const char *format, ...)
#else
error (format, va_alist)
     const char *format;
     va_dcl
#endif
{
#ifdef VA_FPRINTF
  va_list ap;
#endif

  remember_error ();

  VA_START (ap, format);
#ifdef VA_FPRINTF
  VA_FPRINTF (stderr, format, ap);
#else
  fprintf (stderr, format, a1, a2, a3, a4, a5, a6, a7, a8);
#endif /* not VA_FPRINTF */
  va_end (ap);

  putc ('\n', stderr);
}

/* Just like error (), but print the input file and line number as well. */
void
#if defined (VA_FPRINTF) && __STDC__
file_line_error (char *infile, int lno, const char *format, ...)
#else
file_line_error (infile, lno, format, va_alist)
   char *infile;
   int lno;
   const char *format;
   va_dcl
#endif
{
#ifdef VA_FPRINTF
  va_list ap;
#endif

  remember_error ();
  fprintf (stderr, "%s:%d: ", infile, lno);

  VA_START (ap, format);
#ifdef VA_FPRINTF
  VA_FPRINTF (stderr, format, ap);
#else
  fprintf (stderr, format, a1, a2, a3, a4, a5, a6, a7, a8);
#endif /* not VA_FPRINTF */
  va_end (ap);

  fprintf (stderr, ".\n");
}

/* Just like file_line_error (), but take the input file and the line
   number from global variables. */
void
#if defined (VA_FPRINTF) && __STDC__
line_error (const char *format, ...)
#else
line_error (format, va_alist)
   const char *format;
   va_dcl
#endif
{
#ifdef VA_FPRINTF
  va_list ap;
#endif

  remember_error ();
  fprintf (stderr, "%s:%d: ", input_filename, line_number);

  VA_START (ap, format);
#ifdef VA_FPRINTF
  VA_FPRINTF (stderr, format, ap);
#else
  fprintf (stderr, format, a1, a2, a3, a4, a5, a6, a7, a8);
#endif /* not VA_FPRINTF */
  va_end (ap);

  fprintf (stderr, ".\n");
}

void
#if defined (VA_FPRINTF) && __STDC__
warning (const char *format, ...)
#else
warning (format, va_alist)
     const char *format;
     va_dcl
#endif
{
#ifdef VA_FPRINTF
  va_list ap;
#endif

  if (print_warnings)
    {
      fprintf (stderr, _("%s:%d: warning: "), input_filename, line_number);

      VA_START (ap, format);
#ifdef VA_FPRINTF
      VA_FPRINTF (stderr, format, ap);
#else
      fprintf (stderr, format, a1, a2, a3, a4, a5, a6, a7, a8);
#endif /* not VA_FPRINTF */
      va_end (ap);

      fprintf (stderr, ".\n");
    }
}


/* The other side of a malformed expression. */
static void
misplaced_brace (void)
{
  line_error (_("Misplaced %c"), '}');
}

/* Main.  */

/* Display the version info of this invocation of Makeinfo. */
static void
print_version_info (void)
{
  printf ("makeinfo (GNU %s) %s\n", PACKAGE, VERSION);
}

/* If EXIT_VALUE is zero, print the full usage message to stdout.
   Otherwise, just say to use --help for more info.
   Then exit with EXIT_VALUE. */
static void
usage (int exit_value)
{
  if (exit_value != 0)
    fprintf (stderr, _("Try `%s --help' for more information.\n"), progname);
  else
  {
    printf (_("Usage: %s [OPTION]... TEXINFO-FILE...\n"), progname);
    puts ("");

    puts (_("\
Translate Texinfo source documentation to various other formats, by default\n\
Info files suitable for reading online with Emacs or standalone GNU Info.\n"));

    printf (_("\
General options:\n\
      --error-limit=NUM       quit after NUM errors (default %d).\n\
      --force                 preserve output even if errors.\n\
      --help                  display this help and exit.\n\
      --no-validate           suppress node cross-reference validation.\n\
      --no-warn               suppress warnings (but not errors).\n\
      --reference-limit=NUM   warn about at most NUM references (default %d).\n\
  -v, --verbose               explain what is being done.\n\
      --version               display version information and exit.\n"),
            max_error_level, reference_warning_limit);
    puts ("");

     /* xgettext: no-wrap */
    puts (_("\
Output format selection (default is to produce Info):\n\
      --docbook             output Docbook XML rather than Info.\n\
      --html                output HTML rather than Info.\n\
      --xml                 output Texinfo XML rather than Info.\n\
      --plaintext           output plain text rather than Info.\n\
"));

    puts (_("\
General output options:\n\
  -E, --macro-expand FILE   output macro-expanded source to FILE.\n\
                            ignoring any @setfilename.\n\
      --no-headers          suppress node separators, Node: lines, and menus\n\
                              from Info output (thus producing plain text)\n\
                              or from HTML (thus producing shorter output);\n\
                              also, write to standard output by default.\n\
      --no-split            suppress splitting of Info or HTML output,\n\
                            generate only one output file.\n\
      --no-version-headers  suppress header with makeinfo version and\n\
                            source path.\n\
      --number-sections     output chapter and sectioning numbers.\n\
  -o, --output=FILE         output to FILE (directory if split HTML),\n\
"));

    printf (_("\
Options for Info and plain text:\n\
      --enable-encoding       output accented and special characters in\n\
                                Info output based on @documentencoding.\n\
      --fill-column=NUM       break Info lines at NUM characters (default %d).\n\
      --footnote-style=STYLE  output footnotes in Info according to STYLE:\n\
                                `separate' to put them in their own node;\n\
                                `end' to put them at the end of the node\n\
                                  in which they are defined (default).\n\
      --paragraph-indent=VAL  indent Info paragraphs by VAL spaces (default %d).\n\
                                If VAL is `none', do not indent; if VAL is\n\
                                `asis', preserve existing indentation.\n\
      --split-size=NUM        split Info files at size NUM (default %d).\n"),
             fill_column, paragraph_start_indent,
             DEFAULT_SPLIT_SIZE);
    puts ("");

    puts (_("\
Options for HTML:\n\
      --css-include=FILE        include FILE in HTML <style> output;\n\
                                  read stdin if FILE is -.\n\
"));

    printf (_("\
Options for XML and Docbook:\n\
      --output-indent=VAL       indent XML elements by VAL spaces (default %d).\n\
                                  If VAL is 0, ignorable whitespace is dropped.\n\
"), xml_indentation_increment);
    puts ("");

    puts (_("\
Input file options:\n\
      --commands-in-node-names  allow @ commands in node names.\n\
  -D VAR                        define the variable VAR, as with @set.\n\
  -I DIR                        append DIR to the @include search path.\n\
  -P DIR                        prepend DIR to the @include search path.\n\
  -U VAR                        undefine the variable VAR, as with @clear.\n\
"));

    puts (_("\
Conditional processing in input:\n\
  --ifdocbook       process @ifdocbook and @docbook even if\n\
                      not generating Docbook.\n\
  --ifhtml          process @ifhtml and @html even if not generating HTML.\n\
  --ifinfo          process @ifinfo even if not generating Info.\n\
  --ifplaintext     process @ifplaintext even if not generating plain text.\n\
  --iftex           process @iftex and @tex; implies --no-split.\n\
  --ifxml           process @ifxml and @xml.\n\
  --no-ifdocbook    do not process @ifdocbook and @docbook text.\n\
  --no-ifhtml       do not process @ifhtml and @html text.\n\
  --no-ifinfo       do not process @ifinfo text.\n\
  --no-ifplaintext  do not process @ifplaintext text.\n\
  --no-iftex        do not process @iftex and @tex text.\n\
  --no-ifxml        do not process @ifxml and @xml text.\n\
\n\
  Also, for the --no-ifFORMAT options, do process @ifnotFORMAT text.\n\
"));

    puts (_("\
  The defaults for the @if... conditionals depend on the output format:\n\
  if generating HTML, --ifhtml is on and the others are off;\n\
  if generating Info, --ifinfo is on and the others are off;\n\
  if generating plain text, --ifplaintext is on and the others are off;\n\
  if generating XML, --ifxml is on and the others are off.\n\
"));

    fputs (_("\
Examples:\n\
  makeinfo foo.texi                     write Info to foo's @setfilename\n\
  makeinfo --html foo.texi              write HTML to @setfilename\n\
  makeinfo --xml foo.texi               write Texinfo XML to @setfilename\n\
  makeinfo --docbook foo.texi           write DocBook XML to @setfilename\n\
  makeinfo --no-headers foo.texi        write plain text to standard output\n\
\n\
  makeinfo --html --no-headers foo.texi write html without node lines, menus\n\
  makeinfo --number-sections foo.texi   write Info with numbered sections\n\
  makeinfo --no-split foo.texi          write one Info file however big\n\
"), stdout);

    puts (_("\n\
Email bug reports to bug-texinfo@gnu.org,\n\
general questions and discussion to help-texinfo@gnu.org.\n\
Texinfo home page: http://www.gnu.org/software/texinfo/"));

  } /* end of full help */

  xexit (exit_value);
}

struct option long_options[] =
{
  { "commands-in-node-names", 0, &expensive_validation, 1 },
  { "css-include", 1, 0, 'C' },
  { "docbook", 0, 0, 'd' },
  { "enable-encoding", 0, &enable_encoding, 1 },
  { "error-limit", 1, 0, 'e' },
  { "fill-column", 1, 0, 'f' },
  { "footnote-style", 1, 0, 's' },
  { "force", 0, &force, 1 },
  { "help", 0, 0, 'h' },
  { "html", 0, 0, 'w' },
  { "ifdocbook", 0, &process_docbook, 1 },
  { "ifhtml", 0, &process_html, 1 },
  { "ifinfo", 0, &process_info, 1 },
  { "ifplaintext", 0, &process_plaintext, 1 },
  { "iftex", 0, &process_tex, 1 },
  { "ifxml", 0, &process_xml, 1 },
  { "macro-expand", 1, 0, 'E' },
  { "no-headers", 0, &no_headers, 1 },
  { "no-ifdocbook", 0, &process_docbook, 0 },
  { "no-ifhtml", 0, &process_html, 0 },
  { "no-ifinfo", 0, &process_info, 0 },
  { "no-ifplaintext", 0, &process_plaintext, 0 },
  { "no-iftex", 0, &process_tex, 0 },
  { "no-ifxml", 0, &process_xml, 0 },
  { "no-number-footnotes", 0, &number_footnotes, 0 },
  { "no-number-sections", 0, &number_sections, 0 },
  { "no-pointer-validate", 0, &validating, 0 },
  { "no-split", 0, &splitting, 0 },
  { "no-validate", 0, &validating, 0 },
  { "no-version-header", 0, &no_version_header, 1 },
  { "no-warn", 0, &print_warnings, 0 },
  { "number-footnotes", 0, &number_footnotes, 1 },
  { "number-sections", 0, &number_sections, 1 },
  { "output", 1, 0, 'o' },
  { "output-indent", 1, 0, 'i' },
  { "paragraph-indent", 1, 0, 'p' },
  { "plaintext", 0, 0, 't' },
  { "reference-limit", 1, 0, 'r' },
  { "split-size", 1, 0, 'S'},
  { "verbose", 0, &verbose_mode, 1 },
  { "version", 0, 0, 'V' },
  { "xml", 0, 0, 'x' },
  {NULL, 0, NULL, 0}
};

/* We use handle_variable_internal for -D and -U, and it depends on
   execute_string, which depends on input_filename, which is not defined
   while we are handling options. :-\  So we save these defines in this
   struct, and handle them later.  */
typedef struct command_line_define
{
  struct command_line_define *next;
  int action;
  char *define;
} COMMAND_LINE_DEFINE;

static COMMAND_LINE_DEFINE *command_line_defines = NULL;

/* For each file mentioned in the command line, process it, turning
   Texinfo commands into wonderfully formatted output text. */
int
main (int argc, char **argv)
{
  int c, ind;
  int reading_from_stdin = 0;

#ifdef HAVE_SETLOCALE
  /* Do not use LC_ALL, because LC_NUMERIC screws up the scanf parsing
     of the argument to @multicolumn.  */
  setlocale (LC_TIME, "");
#ifdef LC_MESSAGES /* ultrix */
  setlocale (LC_MESSAGES, "");
#endif
  setlocale (LC_CTYPE, "");
  setlocale (LC_COLLATE, "");
#endif

#ifdef ENABLE_NLS
  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif

  /* If TEXINFO_OUTPUT_FORMAT envvar is set, use it to set default output.
     Can be overridden with one of the output options.  */
  if (getenv ("TEXINFO_OUTPUT_FORMAT") != NULL)
    {
      if (STREQ (getenv ("TEXINFO_OUTPUT_FORMAT"), "docbook"))
        {
          splitting = 0;
          html = 0;
          docbook = 1;
          xml = 1;
          process_docbook = 1;
        }
      else if (STREQ (getenv ("TEXINFO_OUTPUT_FORMAT"), "html"))
        {
          html = 1;
          docbook = 0;
          xml = 0;
          process_html = 1;
        }
      else if (STREQ (getenv ("TEXINFO_OUTPUT_FORMAT"), "info"))
        {
          html = 0;
          docbook = 0;
          xml = 0;
        }
      else if (STREQ (getenv ("TEXINFO_OUTPUT_FORMAT"), "plaintext"))
        {
          splitting = 0;
          no_headers = 1;
          html = 0;
          docbook = 0;
          xml = 0;
          process_plaintext = 1;
        }
      else if (STREQ (getenv ("TEXINFO_OUTPUT_FORMAT"), "xml"))
        {
          splitting = 0;
          html = 0;
          docbook = 0;
          xml = 1;
          process_xml = 1;
        }
      else
        fprintf (stderr,
            _("%s: Ignoring unrecognized TEXINFO_OUTPUT_FORMAT value `%s'.\n"),
                 progname, getenv ("TEXINFO_OUTPUT_FORMAT"));
    }

  /* Parse argument flags from the input line. */
  while ((c = getopt_long (argc, argv, "D:de:E:f:hI:i:o:p:P:r:s:t:U:vV:wx",
                           long_options, &ind)) != EOF)
    {
      if (c == 0 && long_options[ind].flag == 0)
        c = long_options[ind].val;

      switch (c)
        {
        case 'C':  /* --css-include */
          css_include = xstrdup (optarg);
          break;

        case 'D':
        case 'U':
          /* User specified variable to set or clear. */
          if (xml && !docbook)
            {
              COMMAND_LINE_DEFINE *new = xmalloc (sizeof (COMMAND_LINE_DEFINE));
              new->action = (c == 'D') ? SET : CLEAR;
              new->define = xstrdup (optarg);
              new->next = command_line_defines;
              command_line_defines = new;
            }
          else
            handle_variable_internal ((c == 'D' ? SET : CLEAR), optarg);
          break;

        case 'd': /* --docbook */
          splitting = 0;
          xml = 1;
          docbook = 1;
          html = 0;
	  process_docbook = 1;
          break;

        case 'e': /* --error-limit */
          if (sscanf (optarg, "%d", &max_error_level) != 1)
            {
              fprintf (stderr,
                      _("%s: %s arg must be numeric, not `%s'.\n"),
                      progname, "--error-limit", optarg);
              usage (1);
            }
          break;

        case 'E': /* --macro-expand */
          if (!macro_expansion_output_stream)
            {
              macro_expansion_filename = optarg;
              macro_expansion_output_stream
                = strcmp (optarg, "-") == 0 ? stdout : fopen (optarg, "w");
              if (!macro_expansion_output_stream)
                error (_("%s: could not open macro expansion output `%s'"),
                       progname, optarg);
            }
          else
            fprintf (stderr,
                     _("%s: ignoring second macro expansion output `%s'.\n"),
                     progname, optarg);
          break;

        case 'f': /* --fill-column */
          if (sscanf (optarg, "%d", &fill_column) != 1)
            {
              fprintf (stderr,
                       _("%s: %s arg must be numeric, not `%s'.\n"),
                       progname, "--fill-column", optarg);
              usage (1);
            }
          break;

        case 'h': /* --help */
          usage (0);
          break;

        case 'I':
          /* Append user-specified dir to include file path. */
          append_to_include_path (optarg);
          break;

        case 'i':
          if (sscanf (optarg, "%d", &xml_indentation_increment) != 1)
            {
              fprintf (stderr,
                     _("%s: %s arg must be numeric, not `%s'.\n"),
                     progname, "--output-indent", optarg);
              usage (1);
            }
          break;

        case 'o': /* --output */
          command_output_filename = xstrdup (optarg);
          save_command_output_filename = command_output_filename;
          break;

        case 'p': /* --paragraph-indent */
          if (set_paragraph_indent (optarg) < 0)
            {
              fprintf (stderr,
   _("%s: --paragraph-indent arg must be numeric/`none'/`asis', not `%s'.\n"),
                       progname, optarg);
              usage (1);
            }
          break;

        case 'P':
          /* Prepend user-specified include dir to include path. */
          prepend_to_include_path (optarg);
          break;

        case 'r': /* --reference-limit */
          if (sscanf (optarg, "%d", &reference_warning_limit) != 1)
            {
              fprintf (stderr,
                     _("%s: %s arg must be numeric, not `%s'.\n"),
                     progname, "--reference-limit", optarg);
              usage (1);
            }
          break;

        case 's': /* --footnote-style */
          if (set_footnote_style (optarg) < 0)
            {
              fprintf (stderr,
        _("%s: --footnote-style arg must be `separate' or `end', not `%s'.\n"),
                       progname, optarg);
              usage (1);
            }
          footnote_style_preset = 1;
          break;

        case 'S': /* --split-size */
          if (sscanf (optarg, "%d", &split_size) != 1)
            {
              fprintf (stderr,
                     _("%s: %s arg must be numeric, not `%s'.\n"),
                     progname, "--split-size", optarg);
              usage (1);
            }
          break;

        case 't': /* --plaintext */
          splitting = 0;
          no_headers = 1;
          html = 0;
          docbook = 0;
          xml = 0;
          process_plaintext = 1;
          break;

        case 'v':
          verbose_mode++;
          break;

        case 'V': /* --version */
          print_version_info ();
          puts ("");
          puts ("Copyright (C) 2004 Free Software Foundation, Inc.");
          printf (_("There is NO warranty.  You may redistribute this software\n\
under the terms of the GNU General Public License.\n\
For more information about these matters, see the files named COPYING.\n"));
          xexit (0);
          break;

        case 'w': /* --html */
          xml = 0;
          docbook = 0;
          html = 1;
          process_html = 1;
          break;

        case 'x': /* --xml */
          splitting = 0;
          html = 0;
          docbook = 0;
          xml = 1;
          process_xml = 1;
          break;

        case '?':
          usage (1);
          break;
        }
    }

  if (macro_expansion_output_stream)
    validating = 0;

  if (!validating)
    expensive_validation = 0;

  if (optind == argc)
    {
      /* Check to see if input is a file.  If so, process that. */
      if (!isatty (fileno (stdin)))
        reading_from_stdin = 1;
      else
        {
          fprintf (stderr, _("%s: missing file argument.\n"), progname);
          usage (1);
        }
    }

  if (no_headers)
    {
      /* If the user did not specify an output file, use stdout. */
      if (!command_output_filename)
        command_output_filename = xstrdup ("-");

      if (html && splitting && !STREQ (command_output_filename, "-"))
        { /* --no-headers --no-split --html indicates confusion. */
          fprintf (stderr,
                  "%s: can't split --html output to `%s' with --no-headers.\n",
                   progname, command_output_filename);
          usage (1);
        }

      /* --no-headers implies --no-split.  */
      splitting = 0;
    }

  if (process_info == -1)
    { /* no explicit --[no-]ifinfo option, so we'll do @ifinfo
         if we're generating info or (for compatibility) plain text.  */
      process_info = !html && !xml;
    }

  if (process_plaintext == -1)
    { /* no explicit --[no-]ifplaintext option, so we'll do @ifplaintext
         if we're generating plain text.  */
      process_plaintext = no_headers && !html && !xml;
    }

  if (verbose_mode)
    print_version_info ();

  /* Remaining arguments are file names of texinfo files.
     Convert them, one by one. */
  if (!reading_from_stdin)
    {
      while (optind != argc)
        convert_from_file (argv[optind++]);
    }
  else
    convert_from_stream (stdin, "stdin");

  xexit (errors_printed ? 2 : 0);
  return 0; /* Avoid bogus warnings.  */
}

/* Hacking tokens and strings.  */

/* Return the next token as a string pointer.  We cons the string.  This
   `token' means simply a command name.  */

/* = is so @alias works.  ^ and _ are so macros can be used in math mode
   without a space following.  Possibly we should simply allow alpha, to
   be compatible with TeX.  */
#define COMMAND_CHAR(c) (!cr_or_whitespace(c) \
                         && (c) != '{' \
                         && (c) != '}' \
                         && (c) != '=' \
                         && (c) != '_' \
                         && (c) != '^' \
                         )

static char *
read_token (void)
{
  int i, character;
  char *result;

  /* If the first character to be read is self-delimiting, then that
     is the command itself. */
  character = curchar ();
  if (self_delimiting (character))
    {
      input_text_offset++;

      if (character == '\n')
        line_number++;

      result = xstrdup (" ");
      *result = character;
      return result;
    }

  for (i = 0; ((input_text_offset != input_text_length)
               && (character = curchar ())
               && COMMAND_CHAR (character));
       i++, input_text_offset++);
  result = xmalloc (i + 1);
  memcpy (result, &input_text[input_text_offset - i], i);
  result[i] = 0;
  return result;
}

/* Return nonzero if CHARACTER is self-delimiting. */
int
self_delimiting (int character)
{
  /* @; and @\ are not Texinfo commands, but they are listed here
     anyway.  I don't know why.  --karl, 10aug96.  */
  return strchr ("~{|}`^\\@?=;:./-,*\'\" !\n\t", character) != NULL;
}

/* Clear whitespace from the front and end of string. */
void
canon_white (char *string)
{
  char *p = string;
  unsigned len;

  if (!*p)
    return;

  do
    {
      if (!cr_or_whitespace (*p))
	break;
      ++p;
    }
  while (*p);

  len = strlen (p);
  while (len && cr_or_whitespace (p[len-1]))
    --len;

  if (p != string)
    memmove (string, p, len);

  string[len] = 0;
}

/* Bash STRING, replacing all whitespace with just one space. */
void
fix_whitespace (char *string)
{
  char *temp = xmalloc (strlen (string) + 1);
  int string_index = 0;
  int temp_index = 0;
  int c;

  canon_white (string);

  while (string[string_index])
    {
      c = temp[temp_index++] = string[string_index++];

      if (c == ' ' || c == '\n' || c == '\t')
        {
          temp[temp_index - 1] = ' ';
          while ((c = string[string_index]) && (c == ' ' ||
                                                c == '\t' ||
                                                c == '\n'))
            string_index++;
        }
    }
  temp[temp_index] = 0;
  strcpy (string, temp);
  free (temp);
}

/* Discard text until the desired string is found.  The string is
   included in the discarded text. */
void
discard_until (char *string)
{
  int temp = search_forward (string, input_text_offset);

  int tt = (temp < 0) ? input_text_length : temp + strlen (string);
  int from = input_text_offset;

  /* Find out what line we are on. */
  while (from != tt)
    if (input_text[from++] == '\n')
      line_number++;

  if (temp < 0)
    {
      /* not found, move current position to end of string */
      input_text_offset = input_text_length;
      if (strcmp (string, "\n") != 0)
        { /* Give a more descriptive feedback, if we are looking for ``@end ''
             during macro execution.  That means someone used a multiline
             command as an argument to, say, @section ... style commands.  */
          char *end_block = xmalloc (8);
          sprintf (end_block, "\n%cend ", COMMAND_PREFIX);
          if (executing_string && strstr (string, end_block))
            line_error (_("Multiline command %c%s used improperly"), 
                COMMAND_PREFIX, command);
          else
            line_error (_("Expected `%s'"), string);
          free (end_block);
          return;
        }
    }
  else
    /* found, move current position to after the found string */
    input_text_offset = temp + strlen (string);
}

/* Read characters from the file until we are at MATCH.
   Place the characters read into STRING.
   On exit input_text_offset is after the match string.
   Return the offset where the string starts. */
int
get_until (char *match, char **string)
{
  int len, current_point, x, new_point, tem;

  current_point = x = input_text_offset;
  new_point = search_forward (match, input_text_offset);

  if (new_point < 0)
    new_point = input_text_length;
  len = new_point - current_point;

  /* Keep track of which line number we are at. */
  tem = new_point + (strlen (match) - 1);
  while (x != tem)
    if (input_text[x++] == '\n')
      line_number++;

  *string = xmalloc (len + 1);

  memcpy (*string, &input_text[current_point], len);
  (*string)[len] = 0;

  /* Now leave input_text_offset in a consistent state. */
  input_text_offset = tem;

  if (input_text_offset > input_text_length)
    input_text_offset = input_text_length;

  return new_point;
}

/* Replace input_text[FROM .. TO] with its expansion.  */
void
replace_with_expansion (int from, int *to)
{
  char *xp;
  unsigned xp_len, new_len;
  char *old_input = input_text;
  unsigned raw_len = *to - from;
  char *str;

  /* The rest of the code here moves large buffers, so let's
     not waste time if the input cannot possibly expand
     into anything.  Unfortunately, we cannot avoid expansion
     when we see things like @code etc., even if they only
     asked for expansion of macros, since any Texinfo command
     can be potentially redefined with a macro.  */
  if (only_macro_expansion &&
      memchr (input_text + from, COMMAND_PREFIX, raw_len) == 0)
    return;

  /* Get original string from input.  */
  str = xmalloc (raw_len + 1);
  memcpy (str, input_text + from, raw_len);
  str[raw_len] = 0;

  /* We are going to relocate input_text, so we had better output
     pending portion of input_text now, before the pointer changes.  */
  if (macro_expansion_output_stream && !executing_string
      && !me_inhibit_expansion)
    append_to_expansion_output (from);

  /* Expand it.  */
  xp = expansion (str, 0);
  xp_len = strlen (xp);
  free (str);

  /* Plunk the expansion into the middle of `input_text' --
     which is terminated by a newline, not a null.  Avoid
     expensive move of the rest of the input if the expansion
     has the same length as the original string.  */
  if (xp_len != raw_len)
    {
      new_len = from + xp_len + input_text_length - *to + 1;
      if (executing_string)
        { /* If we are in execute_string, we might need to update
             the relevant element in the execution_strings[] array,
             since it could have to be relocated from under our
             feet.  (input_text is reallocated here as well, if needed.)  */
          maybe_update_execution_strings (&input_text, new_len);
        }
      else if (new_len > input_text_length + 1)
        /* Don't bother to realloc if we have enough space.  */
        input_text = xrealloc (input_text, new_len);

      memmove (input_text + from + xp_len,
               input_text + *to, input_text_length - *to + 1);

      *to += xp_len - raw_len;
      /* Since we change input_text_length here, the comparison above
         isn't really valid, but it seems the worst that might happen is
         an extra xrealloc or two, so let's not worry.  */
      input_text_length += xp_len - raw_len;
    }
  memcpy (input_text + from, xp, xp_len);
  free (xp);

  /* Synchronize the macro-expansion pointers with our new input_text.  */
  if (input_text != old_input)
    forget_itext (old_input);
  if (macro_expansion_output_stream && !executing_string)
    remember_itext (input_text, from);
}

/* Read characters from the file until we are at MATCH or end of line.
   Place the characters read into STRING.  If EXPAND is nonzero,
   expand the text before looking for MATCH for those cases where
   MATCH might be produced by some macro.  */
void
get_until_in_line (int expand, char *match, char **string)
{
  int real_bottom = input_text_length;
  int limit = search_forward ("\n", input_text_offset);
  if (limit < 0)
    limit = input_text_length;

  /* Replace input_text[input_text_offset .. limit-1] with its expansion.
     This allows the node names and menu entries themselves to be
     constructed via a macro, as in:
        @macro foo{p, q}
        Together: \p\ & \q\.
        @end macro

        @node @foo{A,B}, next, prev, top

     Otherwise, the `,' separating the macro args A and B is taken as
     the node argument separator, so the node name is `@foo{A'.  This
     expansion is only necessary on the first call, since we expand the
     whole line then.  */
  if (expand)
    {
      replace_with_expansion (input_text_offset, &limit);
    }

  real_bottom = input_text_length;
  input_text_length = limit;
  get_until (match, string);
  input_text_length = real_bottom;
}

void
get_rest_of_line (int expand, char **string)
{
  xml_no_para ++;
  if (expand)
    {
      char *tem;

      /* Don't expand non-macros in input, since we want them
         intact in the macro-expanded output.  */
      only_macro_expansion++;
      get_until_in_line (1, "\n", &tem);
      only_macro_expansion--;
      *string = expansion (tem, 0);
      free (tem);
    }
  else
    get_until_in_line (0, "\n", string);

  canon_white (*string);

  if (curchar () == '\n')       /* as opposed to the end of the file... */
    {
      line_number++;
      input_text_offset++;
    }
  xml_no_para --;
}

/* Backup the input pointer to the previous character, keeping track
   of the current line number. */
void
backup_input_pointer (void)
{
  if (input_text_offset)
    {
      input_text_offset--;
      if (curchar () == '\n')
        line_number--;
    }
}

/* Read characters from the file until we are at MATCH or closing brace.
   Place the characters read into STRING.  */
void
get_until_in_braces (char *match, char **string)
{
  char *temp;
  int i, brace = 0;
  int match_len = strlen (match);

  for (i = input_text_offset; i < input_text_length; i++)
    {
      if (i < input_text_length - 1 && input_text[i] == '@')
        {
          i++;                  /* skip commands like @, and @{ */
          continue;
        }
      else if (input_text[i] == '{')
        brace++;
      else if (input_text[i] == '}')
        {
          brace--;
          /* If looking for a brace, don't stop at the interior brace,
             like after "baz" in "@foo{something @bar{baz} more}".  */
          if (brace == 0)
            continue;
        }
      else if (input_text[i] == '\n')
        line_number++;

      if (brace < 0 ||
          (brace == 0 && strncmp (input_text + i, match, match_len) == 0))
        break;
    }

  match_len = i - input_text_offset;
  temp = xmalloc (2 + match_len);
  memcpy (temp, input_text + input_text_offset, match_len);
  temp[match_len] = 0;
  input_text_offset = i;
  *string = temp;
}



/* Converting a file.  */

/* Convert the file named by NAME.  The output is saved on the file
   named as the argument to the @setfilename command. */
static char *suffixes[] = {
  /* ".txi" is checked first so that on 8+3 DOS filesystems, if they
     have "texinfo.txi" and "texinfo.tex" in the same directory, the
     former is used rather than the latter, due to file name truncation.  */
  ".txi",
  ".texinfo",
  ".texi",
  ".txinfo",
  "",
  NULL
};

static void
initialize_conversion (void)
{
  init_tag_table ();
  init_indices ();
  init_internals ();
  init_paragraph ();

  /* This is used for splitting the output file and for doing section
     headings.  It was previously initialized in `init_paragraph', but its
     use there loses with the `init_paragraph' calls done by the
     multitable code; the tag indices get reset to zero.  */
  output_position = 0;
}

/* Reverse the chain of structures in LIST.  Output the new head
   of the chain.  You should always assign the output value of this
   function to something, or you will lose the chain. */
GENERIC_LIST *
reverse_list (GENERIC_LIST *list)
{
  GENERIC_LIST *next;
  GENERIC_LIST *prev = NULL;

  while (list)
    {
      next = list->next;
      list->next = prev;
      prev = list;
      list = next;
    }
  return prev;
}

/* We read in multiples of 4k, simply because it is a typical pipe size
   on unix systems. */
#define READ_BUFFER_GROWTH (4 * 4096)

/* Convert the Texinfo file coming from the open stream STREAM.  Assume the
   source of the stream is named NAME. */
static void
convert_from_stream (FILE *stream, char *name)
{
  char *buffer = NULL;
  int buffer_offset = 0, buffer_size = 0;

  initialize_conversion ();

  /* Read until the end of the stream.  This isn't strictly correct, since
     the texinfo input may end before the stream ends, but it is a quick
     working hueristic. */
  while (!feof (stream))
    {
      int count;

      if (buffer_offset + (READ_BUFFER_GROWTH + 1) >= buffer_size)
        buffer = (char *)
          xrealloc (buffer, (buffer_size += READ_BUFFER_GROWTH));

      count = fread (buffer + buffer_offset, 1, READ_BUFFER_GROWTH, stream);

      if (count < 0)
        {
          perror (name);
          xexit (1);
        }

      buffer_offset += count;
      if (count == 0)
        break;
    }

  /* Set the globals to the new file. */
  input_text = buffer;
  input_text_length = buffer_offset;
  input_filename = xstrdup (name);
  node_filename = xstrdup (name);
  input_text_offset = 0;
  line_number = 1;

  /* Not strictly necessary.  This magic prevents read_token () from doing
     extra unnecessary work each time it is called (that is a lot of times).
     The INPUT_TEXT_LENGTH is one past the actual end of the text. */
  input_text[input_text_length] = '\n';

  convert_from_loaded_file (name);
}

static void
convert_from_file (char *name)
{
  int i;
  char *filename = xmalloc (strlen (name) + 50);

  /* Prepend file directory to the search path, so relative links work.  */
  prepend_to_include_path (pathname_part (name));

  initialize_conversion ();

  /* Try to load the file specified by NAME, concatenated with our
     various suffixes.  Prefer files like `makeinfo.texi' to
     `makeinfo'.  */
  for (i = 0; suffixes[i]; i++)
    {
      strcpy (filename, name);
      strcat (filename, suffixes[i]);

      if (find_and_load (filename, 1))
        break;

      if (!suffixes[i][0] && strrchr (filename, '.'))
        {
          fs_error (filename);
          free (filename);
          return;
        }
    }

  if (!suffixes[i])
    {
      fs_error (name);
      free (filename);
      return;
    }

  input_filename = filename;

  convert_from_loaded_file (name);

  /* Pop the prepended path, so multiple filenames in the
     command line do not screw each others include paths.  */
  pop_path_from_include_path ();
}

static int
create_html_directory (char *dir, int can_remove_file)
{
  struct stat st;

  /* Already exists.  */
  if (stat (dir, &st) == 0)
    {
      /* And it's a directory, so silently reuse it.  */
      if (S_ISDIR (st.st_mode))
        return 1;
      /* Not a directory, so move it out of the way if we are allowed.  */
      else if (can_remove_file)
        {
          if (unlink (dir) != 0)
            return 0;
        }
      else
        return 0;
    }

  if (mkdir (dir, 0777) == 0)
    /* Success!  */
    return 1;
  else
    return 0;
}

/* Given OUTPUT_FILENAME == ``/foo/bar/baz.html'', return
   "/foo/bar/baz/baz.html".  This routine is called only if html && splitting.

  Split html output goes into the subdirectory of the toplevel
  filename, without extension.  For example:
      @setfilename foo.info
  produces output in files foo/index.html, foo/second-node.html, ...

  But if the user said -o foo.whatever on the cmd line, then use
  foo.whatever unchanged.  */

static char *
insert_toplevel_subdirectory (char *output_filename)
{
  static const char index_name[] = "index.html";
  char *dir, *subdir, *base, *basename, *p;
  char buf[PATH_MAX];
  const int index_len = sizeof (index_name) - 1;

  strcpy (buf, output_filename);
  dir = pathname_part (buf);   /* directory of output_filename */
  base = filename_part (buf);  /* strips suffix, too */
  basename = xstrdup (base);   /* remember real @setfilename name */
  p = dir + strlen (dir) - 1;
  if (p > dir && IS_SLASH (*p))
    *p = 0;
  p = strrchr (base, '.');
  if (p)
    *p = 0;

  /* Split html output goes into subdirectory of toplevel name. */
  if (save_command_output_filename
      && STREQ (output_filename, save_command_output_filename))
    subdir = basename;  /* from user, use unchanged */
  else
    subdir = base;      /* implicit, omit suffix */

  free (output_filename);
  output_filename = xmalloc (strlen (dir) + 1
                             + strlen (basename) + 1
                             + index_len
                             + 1);
  strcpy (output_filename, dir);
  if (strlen (dir))
    strcat (output_filename, "/");
  strcat (output_filename, subdir);

  /* First try, do not remove existing file.  */
  if (!create_html_directory (output_filename, 0))
    {
      /* That failed, try subdir name with .html.
         Remove it if it exists.  */
      strcpy (output_filename, dir);
      if (strlen (dir))
        strcat (output_filename, "/");
      strcat (output_filename, basename);

      if (!create_html_directory (output_filename, 1))
        {
          /* Last try failed too :-\  */
          line_error (_("Can't create directory `%s': %s"),
              output_filename, strerror (errno));
          xexit (1);
        }
    }

  strcat (output_filename, "/");
  strcat (output_filename, index_name);
  return output_filename;
}

/* FIXME: this is way too hairy */
static void
convert_from_loaded_file (char *name)
{
  char *real_output_filename = NULL;

  remember_itext (input_text, 0);

  input_text_offset = 0;

  /* Avoid the `\input texinfo' line in HTML output (assuming it starts
     the file).  */
  if (looking_at ("\\input"))
    discard_until ("\n");

  /* Search this file looking for the special string which starts conversion.
     Once found, we may truly begin. */
  while (input_text_offset >= 0)
    {
      input_text_offset =
        search_forward (setfilename_search, input_text_offset);

      if (input_text_offset == 0
          || (input_text_offset > 0
              && input_text[input_text_offset -1] == '\n'))
        break;
      else if (input_text_offset > 0)
        input_text_offset++;
    }

  if (input_text_offset < 0)
    {
      if (!command_output_filename)
        {
#if defined (REQUIRE_SETFILENAME)
          error (_("No `%s' found in `%s'"), setfilename_search, name);
          goto finished;
#else
          command_output_filename = output_name_from_input_name (name);
#endif /* !REQUIRE_SETFILENAME */
        }

      {
        int i, end_of_first_line;

        /* Find the end of the first line in the file. */
        for (i = 0; i < input_text_length - 1; i++)
          if (input_text[i] == '\n')
            break;

        end_of_first_line = i + 1;

        for (i = 0; i < end_of_first_line; i++)
          {
            if ((input_text[i] == '\\') &&
                (strncmp (input_text + i + 1, "input", 5) == 0))
              {
                input_text_offset = i;
                break;
              }
          }
      }
    }
  else
    input_text_offset += strlen (setfilename_search);

  if (!command_output_filename)
    {
      get_until ("\n", &output_filename); /* read rest of line */
      if (html || xml)
        { /* Change any extension to .html or .xml.  */
          char *html_name, *directory_part, *basename_part, *temp;

          canon_white (output_filename);
          directory_part = pathname_part (output_filename);

          basename_part = filename_part (output_filename);

          /* Zap any existing extension.  */
          temp = strrchr (basename_part, '.');
          if (temp)
            *temp = 0;

          /* Construct new filename.  */
          html_name = xmalloc (strlen (directory_part)
                               + strlen (basename_part) + 6);
          strcpy (html_name, directory_part);
          strcat (html_name, basename_part);
          strcat (html_name, html ? ".html" : ".xml");

          /* Replace name from @setfilename with the html name.  */
          free (output_filename);
          output_filename = html_name;
        }
    }
  else
    {
      if (input_text_offset != -1)
        discard_until ("\n");
      else
        input_text_offset = 0;

      real_output_filename = output_filename = command_output_filename;
      command_output_filename = NULL;  /* for included files or whatever */
    }

  canon_white (output_filename);
  toplevel_output_filename = xstrdup (output_filename);

  if (real_output_filename && strcmp (real_output_filename, "-") == 0)
    {
      if (macro_expansion_filename
          && strcmp (macro_expansion_filename, "-") == 0)
        {
          fprintf (stderr,
  _("%s: Skipping macro expansion to stdout as Info output is going there.\n"),
                   progname);
          macro_expansion_output_stream = NULL;
        }
      real_output_filename = xstrdup (real_output_filename);
      output_stream = stdout;
      splitting = 0;            /* Cannot split when writing to stdout. */
    }
  else
    {
      if (html && splitting)
        {
          if (FILENAME_CMP (output_filename, NULL_DEVICE) == 0
              || FILENAME_CMP (output_filename, ALSO_NULL_DEVICE) == 0)
            splitting = 0;
          else
            output_filename = insert_toplevel_subdirectory (output_filename);
          real_output_filename = xstrdup (output_filename);
        }
      else if (!real_output_filename)
        real_output_filename = expand_filename (output_filename, name);
      else
        real_output_filename = xstrdup (real_output_filename);

      output_stream = fopen (real_output_filename, "w");
    }

  set_current_output_filename (real_output_filename);

  if (xml && !docbook)
    xml_begin_document (filename_part (output_filename));

  if (verbose_mode)
    printf (_("Making %s file `%s' from `%s'.\n"),
            no_headers ? "text"
            : html ? "HTML"
            : xml ? "XML"
            : "info",
            output_filename, input_filename);

  if (output_stream == NULL)
    {
      fs_error (real_output_filename);
      goto finished;
    }

  /* Make the displayable filename from output_filename.  Only the base
     portion of the filename need be displayed. */
  flush_output ();              /* in case there was no @bye */
  if (output_stream != stdout)
    pretty_output_filename = filename_part (output_filename);
  else
    pretty_output_filename = xstrdup ("stdout");

  /* For this file only, count the number of newlines from the top of
     the file to here.  This way, we keep track of line numbers for
     error reporting.  Line_number starts at 1, since the user isn't
     zero-based. */
  {
    int temp = 0;
    line_number = 1;
    while (temp != input_text_offset)
      if (input_text[temp++] == '\n')
        line_number++;
  }

  /* html fixxme: should output this as trailer on first page.  */
  if (!no_headers && !html && !xml && !no_version_header)
    add_word_args (_("This is %s, produced by makeinfo version %s from %s.\n"),
                   output_filename, VERSION, input_filename);

  close_paragraph ();

  if (xml && !docbook)
    {
      /* Just before the real main loop, let's handle the defines.  */
      COMMAND_LINE_DEFINE *temp;

      for (temp = command_line_defines; temp; temp = temp->next)
        {
          handle_variable_internal (temp->action, temp->define);
          free(temp->define);
        }
    }

  reader_loop ();
  if (xml)
    xml_end_document ();


finished:
  discard_insertions (0);
  close_paragraph ();
  flush_file_stack ();

  if (macro_expansion_output_stream)
    {
      fclose (macro_expansion_output_stream);
      if (errors_printed && !force
          && strcmp (macro_expansion_filename, "-") != 0
          && FILENAME_CMP (macro_expansion_filename, NULL_DEVICE) != 0
          && FILENAME_CMP (macro_expansion_filename, ALSO_NULL_DEVICE) != 0)
        {
          fprintf (stderr,
_("%s: Removing macro output file `%s' due to errors; use --force to preserve.\n"),
                   progname, macro_expansion_filename);
          if (unlink (macro_expansion_filename) < 0)
            perror (macro_expansion_filename);
        }
    }

  if (output_stream)
    {
      output_pending_notes ();

      if (html)
        {
          no_indent = 1;
          start_paragraph ();
          add_word ("</body></html>\n");
          close_paragraph ();
        }

      /* maybe we want local variables in info output.  */
      {
        char *trailer = info_trailer ();
	if (!xml && !docbook && trailer)
          {
            if (html)
              insert_string ("<!--");
            insert_string (trailer);
            free (trailer);
            if (html)
              insert_string ("\n-->\n");
          }
      }

      /* Write stuff makeinfo generates after @bye, ie. info_trailer.  */
      flush_output ();

      if (output_stream != stdout)
        fclose (output_stream);

      /* If validating, then validate the entire file right now. */
      if (validating)
        validate_file (tag_table);

      handle_delayed_writes ();

      if (tag_table)
        {
          tag_table = (TAG_ENTRY *) reverse_list ((GENERIC_LIST *) tag_table);
          if (!no_headers && !html && !STREQ (current_output_filename, "-"))
            write_tag_table (real_output_filename);
        }

      if (splitting && !html && (!errors_printed || force))
        {
          clean_old_split_files (real_output_filename);
          split_file (real_output_filename, split_size);
        }
      else if (errors_printed
               && !force
               && strcmp (real_output_filename, "-") != 0
               && FILENAME_CMP (real_output_filename, NULL_DEVICE) != 0
               && FILENAME_CMP (real_output_filename, ALSO_NULL_DEVICE) != 0)
        { /* If there were errors, and no --force, remove the output.  */
          fprintf (stderr,
  _("%s: Removing output file `%s' due to errors; use --force to preserve.\n"),
                   progname, real_output_filename);
          if (unlink (real_output_filename) < 0)
            perror (real_output_filename);
        }
    }
  free (real_output_filename);
}

/* If enable_encoding is set and @documentencoding is used, return a
   Local Variables section (as a malloc-ed string) so that Emacs'
   locale features can work.  Else return NULL.  */
char *
info_trailer (void)
{
  char *encoding;

  if (!enable_encoding)
    return NULL;

  encoding = current_document_encoding ();

  if (encoding && *encoding)
    {
#define LV_FMT "\n\037\nLocal Variables:\ncoding: %s\nEnd:\n"
      char *lv = xmalloc (sizeof (LV_FMT) + strlen (encoding));
      sprintf (lv, LV_FMT, encoding);
      free (encoding);
      return lv;
    }

  free (encoding);
  return NULL;
}

void
free_and_clear (char **pointer)
{
  if (*pointer)
    {
      free (*pointer);
      *pointer = NULL;
    }
}

 /* Initialize some state. */
static void
init_internals (void)
{
  free_and_clear (&output_filename);
  free_and_clear (&command);
  free_and_clear (&input_filename);
  free_node_references ();
  free_node_node_references ();
  toc_free ();
  init_insertion_stack ();
  init_brace_stack ();
  current_node = NULL; /* sometimes already freed */
  command_index = 0;
  in_menu = 0;
  in_detailmenu = 0;
  top_node_seen = 0;
  non_top_node_seen = 0;
  node_number = -1;
}

void
init_paragraph (void)
{
  free (output_paragraph);
  output_paragraph = xmalloc (paragraph_buffer_len);
  output_paragraph[0] = 0;
  output_paragraph_offset = 0;
  output_column = 0;
  paragraph_is_open = 0;
  current_indent = 0;
  meta_char_pos = 0;
}

/* This is called from `reader_loop' when we are at the * beginning a
   menu line.  */

static void
handle_menu_entry (void)
{
  char *tem;

  /* Ugh, glean_node_from_menu wants to read the * itself.  */
  input_text_offset--;

  /* Find node name in menu entry and save it in references list for
     later validation.  Use followed_reference type for detailmenu
     references since we don't want to use them for default node pointers.  */
  tem = glean_node_from_menu (1, in_detailmenu
                                 ? followed_reference : menu_reference);

  if (html && tem)
    { /* Start a menu item with the cleaned-up line.  Put an anchor
         around the start text (before `:' or the node name). */
      char *string;

      discard_until ("* ");

      /* The line number was already incremented in reader_loop when we
         saw the newline, and discard_until has now incremented again.  */
      line_number--;

      if (had_menu_commentary)
        {
          add_html_block_elt ("<ul class=\"menu\">\n");
          had_menu_commentary = 0;
          in_paragraph = 0;
        }

      if (in_paragraph)
        {
          add_html_block_elt ("</p>\n");
          add_html_block_elt ("<ul class=\"menu\">\n");
          in_paragraph = 0;
        }

      in_menu_item = 1;

      add_html_block_elt ("<li><a");
      if (next_menu_item_number <= 9)
        {
          add_word(" accesskey=");
          add_word_args("\"%d\"", next_menu_item_number);
          next_menu_item_number++;
        }
      add_word (" href=\"");
      string = expansion (tem, 0);
      add_anchor_name (string, 1);
      add_word ("\">");
      free (string);

      /* The menu item may use macros, so expand them now.  */
      only_macro_expansion++;
      get_until_in_line (1, ":", &string);
      only_macro_expansion--;
      execute_string ("%s", string); /* get escaping done */
      free (string);

      add_word ("</a>");

      if (looking_at ("::"))
        discard_until (":");
      else
        { /* discard the node name */
          get_until_in_line (0, ".", &string);
          free (string);
        }
      input_text_offset++;      /* discard the second colon or the period */

      /* Insert a colon only if there is a description of this menu item.  */
      {
        int save_input_text_offset = input_text_offset;
        int save_line_number = line_number;
        char *test_string;
        get_rest_of_line (0, &test_string);
        if (strlen (test_string) > 0)
          add_word (": ");
        input_text_offset = save_input_text_offset;
        line_number = save_line_number;
      }
    }
  else if (xml && tem)
    {
      xml_start_menu_entry (tem);
    }
  else if (tem)
    { /* For Info output, we can just use the input and the main case in
         reader_loop where we output what comes in.  Just move off the *
         so the next time through reader_loop we don't end up back here.  */
      add_char ('*');
      input_text_offset += 2; /* undo the pointer back-up above.  */
    }

  if (tem)
    free (tem);
}

/* Find the command corresponding to STRING.  If the command is found,
   return a pointer to the data structure.  Otherwise return -1.  */
static COMMAND *
get_command_entry (char *string)
{
  int i;

  for (i = 0; command_table[i].name; i++)
    if (strcmp (command_table[i].name, string) == 0)
      return &command_table[i];

  /* This command is not in our predefined command table.  Perhaps
     it is a user defined command. */
  for (i = 0; i < user_command_array_len; i++)
    if (user_command_array[i] &&
        (strcmp (user_command_array[i]->name, string) == 0))
      return user_command_array[i];

  /* We never heard of this command. */
  return (COMMAND *) -1;
}

/* input_text_offset is right at the command prefix character.
   Read the next token to determine what to do.  Return zero
   if there's no known command or macro after the prefix character.  */
static int
read_command (void)
{
  COMMAND *entry;
  int old_text_offset = input_text_offset++;

  free_and_clear (&command);
  command = read_token ();

  /* Check to see if this command is a macro.  If so, execute it here. */
  {
    MACRO_DEF *def;

    def = find_macro (command);

    if (def)
      {
        /* We disallow recursive use of a macro call.  Inhibit the expansion
           of this macro during the life of its execution. */
        if (!(def->flags & ME_RECURSE))
          def->inhibited = 1;

        executing_macro++;
        execute_macro (def);
        executing_macro--;

        if (!(def->flags & ME_RECURSE))
          def->inhibited = 0;

        return 1;
      }
  }

  if (only_macro_expansion)
    {
      /* Back up to the place where we were called, so the
         caller will have a chance to process this non-macro.  */
      input_text_offset = old_text_offset;
      return 0;
    }

  /* Perform alias expansion */
  command = alias_expand (command);

  if (enclosure_command (command))
    {
      remember_brace (enclosure_expand);
      enclosure_expand (START, output_paragraph_offset, 0);
      return 0;
    }

  entry = get_command_entry (command);
  if (entry == (COMMAND *)-1)
    {
      line_error (_("Unknown command `%s'"), command);
      return 0;
    }

  if (entry->argument_in_braces == BRACE_ARGS)
    remember_brace (entry->proc);
  else if (entry->argument_in_braces == MAYBE_BRACE_ARGS)
    {
      if (curchar () == '{')
        remember_brace (entry->proc);
      else
        { /* No braces, so arg is next char.  */
          int ch;
          int saved_offset = output_paragraph_offset;
          (*(entry->proc)) (START, output_paragraph_offset, 0);

          /* Possibilities left for the next character: @ (error), }
             (error), whitespace (skip) anything else (normal char).  */
          skip_whitespace ();
          ch = curchar ();
          if (ch == '@')
            {
           line_error (_("Use braces to give a command as an argument to @%s"),
               entry->name);
              return 0;
            }
          else if (ch == '}')
            {
              /* Our caller will give the error message, because this }
                 won't match anything.  */
              return 0;
            }

          add_char (ch);
          input_text_offset++;
          (*(entry->proc)) (END, saved_offset, output_paragraph_offset);
          return 1;
        }
    }

  /* Get here if we have BRACE_ARGS, NO_BRACE_ARGS, or MAYBE_BRACE_ARGS
     with braces.  */
  (*(entry->proc)) (START, output_paragraph_offset, 0);
  return 1;
}

/* Okay, we are ready to start the conversion.  Call the reader on
   some text, and fill the text as it is output.  Handle commands by
   remembering things like open braces and the current file position on a
   stack, and when the corresponding close brace is found, you can call
   the function with the proper arguments.  Although the filling isn't
   necessary for HTML, it should do no harm.  */
void
reader_loop (void)
{
  int character;
  int done = 0;

  while (!done)
    {
      if (input_text_offset >= input_text_length)
        break;

      character = curchar ();

      /* If only_macro_expansion, only handle macros and leave
         everything else intact.  */
      if (!only_macro_expansion && !in_fixed_width_font
          && ((!html && !xml) || escape_html)
          && (character == '\'' || character == '`')
          && input_text[input_text_offset + 1] == character)
        {
          if (html)
            {
              input_text_offset += 2;
              add_word (character == '`' ? "&ldquo;" : "&rdquo;");
              continue;
            }
          else if (xml)
            {
              input_text_offset += 2;
              xml_insert_entity (character == '`' ? "ldquo" : "rdquo");
              continue;
            }
          else
            {
              input_text_offset++;
              character = '"';
            }
        }

      /* Convert --- to --.  */
      if (!only_macro_expansion && character == '-' && !in_fixed_width_font
          && ((!html && !xml) || escape_html))
        {
          int dash_count = 0;

          /* Get the number of consequtive dashes.  */
          while (input_text[input_text_offset] == '-')
            {
              dash_count++;
              input_text_offset++;
            }

          /* Eat one dash.  */
          dash_count--;

          if (html || xml)
            {
              if (dash_count == 0)
                add_char ('-');
              else
                while (dash_count > 0)
                  {
                    if (dash_count >= 2)
                      {
                        if (html)
                          add_word ("&mdash;");
                        else
                          xml_insert_entity ("mdash");
                        dash_count -= 2;
                      }
                    else if (dash_count >= 1)
                      {
                        if (html)
                          add_word ("&ndash;");
                        else
                          xml_insert_entity ("ndash");
                        dash_count--;
                      }
                  }
            }
          else
            {
              add_char ('-');
              while (--dash_count > 0)
                add_char ('-');
            }

          continue;
        }

      /* If this is a whitespace character, then check to see if the line
         is blank.  If so, advance to the carriage return. */
      if (!only_macro_expansion && whitespace (character))
        {
          int i = input_text_offset + 1;

          while (i < input_text_length && whitespace (input_text[i]))
            i++;

          if (i == input_text_length || input_text[i] == '\n')
            {
              if (i == input_text_length)
                i--;

              input_text_offset = i;
              character = curchar ();
            }
        }

      if (character == '\n')
        line_number++;

      switch (character)
        {
        case '*': /* perhaps we are at a menu */
          /* We used to check for this in the \n case but an @c in a
             menu swallows its newline, so check here instead.  */
          if (!only_macro_expansion && in_menu
              && input_text_offset + 1 < input_text_length
              && input_text[input_text_offset-1] == '\n')
            handle_menu_entry ();
          else
            { /* Duplicate code from below, but not worth twisting the
                 fallthroughs to get down there.  */
              add_char (character);
              input_text_offset++;
            }
          break;

        /* Escapes for HTML unless we're outputting raw HTML.  Do
           this always, even if SGML rules don't require it since
           that's easier and safer for non-conforming browsers. */
        case '&':
          if (html && escape_html)
            add_word ("&amp;");
          else
            add_char (character);
          input_text_offset++;
          break;

        case '<':
          if (html && escape_html)
            add_word ("&lt;");
          else if (xml && escape_html)
            xml_insert_entity ("lt");
          else
            add_char (character);
          input_text_offset++;
          break;

        case '>':
          if (html && escape_html)
            add_word ("&gt;");
          else if (xml && escape_html)
            xml_insert_entity ("gt");
          else
            add_char (character);
          input_text_offset++;
          break;

        case COMMAND_PREFIX: /* @ */
          if (read_command () || !only_macro_expansion)
            break;

        /* FALLTHROUGH (usually) */
        case '{':
          /* Special case.  We're not supposed to see this character by itself.
             If we do, it means there is a syntax error in the input text.
             Report the error here, but remember this brace on the stack so
             we can ignore its partner. */
          if (!only_macro_expansion)
            {
              if (command && !STREQ (command, "math"))
                {
                  line_error (_("Misplaced %c"), '{');
                  remember_brace (misplaced_brace);
                }
              else
                /* We don't mind `extra' braces inside @math.  */
                remember_brace (cm_no_op);
              /* remember_brace advances input_text_offset.  */
              break;
            }

        /* FALLTHROUGH (usually) */
        case '}':
          if (!only_macro_expansion)
            {
              pop_and_call_brace ();
              input_text_offset++;
              break;
            }

        /* FALLTHROUGH (usually) */
        default:
          add_char (character);
          input_text_offset++;
        }
    }
  if (macro_expansion_output_stream && !only_macro_expansion)
    maybe_write_itext (input_text, input_text_offset);
}

static void
init_brace_stack (void)
{
  brace_stack = NULL;
}

/* Remember the current output position here.  Save PROC
   along with it so you can call it later. */
static void
remember_brace_1 (COMMAND_FUNCTION (*proc), int position)
{
  BRACE_ELEMENT *new = xmalloc (sizeof (BRACE_ELEMENT));
  new->next = brace_stack;
  new->proc = proc;
  new->command = command ? xstrdup (command) : "";
  new->pos = position;
  new->line = line_number;
  new->in_fixed_width_font = in_fixed_width_font;
  brace_stack = new;
}

static void
remember_brace (COMMAND_FUNCTION (*proc))
{
  if (curchar () != '{')
    line_error (_("%c%s expected braces"), COMMAND_PREFIX, command);
  else
    input_text_offset++;
  remember_brace_1 (proc, output_paragraph_offset);
}

/* Pop the top of the brace stack, and call the associated function
   with the args END and POS. */
static void
pop_and_call_brace (void)
{
  if (brace_stack == NULL)
    {
      line_error (_("Unmatched }"));
      return;
    }

  {
    BRACE_ELEMENT *temp;

    int pos = brace_stack->pos;
    COMMAND_FUNCTION *proc = brace_stack->proc;
    in_fixed_width_font = brace_stack->in_fixed_width_font;

    /* Reset current command, so the proc can know who it is.  This is
       used in cm_accent.  */
    command = brace_stack->command;

    temp = brace_stack->next;
    free (brace_stack);
    brace_stack = temp;

    (*proc) (END, pos, output_paragraph_offset);
  }
}

/* Shift all of the markers in `brace_stack' by AMOUNT. */
static void
adjust_braces_following (int here, int amount)
{
  BRACE_ELEMENT *stack = brace_stack;

  while (stack)
    {
      if (stack->pos >= here)
        stack->pos += amount;
      stack = stack->next;
    }
}

/* Return the string which invokes PROC; a pointer to a function.
   Always returns the first function in the command table if more than
   one matches PROC.  */
static const char *
find_proc_name (COMMAND_FUNCTION (*proc))
{
  int i;

  for (i = 0; command_table[i].name; i++)
    if (proc == command_table[i].proc)
      return command_table[i].name;
  return _("NO_NAME!");
}

/* You call discard_braces () when you shouldn't have any braces on the stack.
   I used to think that this happens for commands that don't take arguments
   in braces, but that was wrong because of things like @code{foo @@}.  So now
   I only detect it at the beginning of nodes. */
void
discard_braces (void)
{
  if (!brace_stack)
    return;

  while (brace_stack)
    {
      if (brace_stack->proc != misplaced_brace)
        {
          const char *proc_name;

          proc_name = find_proc_name (brace_stack->proc);
          file_line_error (input_filename, brace_stack->line,
                           _("%c%s missing close brace"), COMMAND_PREFIX,
                           proc_name);
          pop_and_call_brace ();
        }
      else
        {
          BRACE_ELEMENT *temp;
          temp = brace_stack->next;
          free (brace_stack);
          brace_stack = temp;
        }
    }
}

static int
get_char_len (int character)
{
  /* Return the printed length of the character. */
  int len;

  switch (character)
    {
    case '\t':
      len = (output_column + 8) & 0xf7;
      if (len > fill_column)
        len = fill_column - output_column;
      else
        len = len - output_column;
      break;

    case '\n':
      len = fill_column - output_column;
      break;

    default:
      /* ASCII control characters appear as two characters in the output
         (e.g., ^A).  But characters with the high bit set are just one
         on suitable terminals, so don't count them as two for line
         breaking purposes.  */
      if (0 <= character && character < ' ')
        len = 2;
      else
        len = 1;
    }
  return len;
}

void
#if defined (VA_FPRINTF) && __STDC__
add_word_args (const char *format, ...)
#else
add_word_args (format, va_alist)
    const char *format;
    va_dcl
#endif
{
  char buffer[2000]; /* xx no fixed limits */
#ifdef VA_FPRINTF
  va_list ap;
#endif

  VA_START (ap, format);
#ifdef VA_SPRINTF
  VA_SPRINTF (buffer, format, ap);
#else
  sprintf (buffer, format, a1, a2, a3, a4, a5, a6, a7, a8);
#endif /* not VA_SPRINTF */
  va_end (ap);
  add_word (buffer);
}

/* Add STRING to output_paragraph. */
void
add_word (char *string)
{
  while (*string)
    add_char (*string++);
}

/* Like add_word, but inhibits conversion of whitespace into &nbsp;.
   Use this to output HTML directives with embedded blanks, to make
   them @w-safe.  */
void
add_html_elt (char *string)
{
  in_html_elt++;
  add_word (string);
  in_html_elt--;
}

/* These two functions below, add_html_block_elt and add_html_block_elt_args,
   are mixtures of add_html_elt and add_word_args.  They inform makeinfo that
   the current HTML element being inserted should not be enclosed in a <p>
   element.  */
void
add_html_block_elt (char *string)
{
  in_html_block_level_elt++;
  add_word (string);
  in_html_block_level_elt--;
}

void
#if defined (VA_FPRINTF) && __STDC__
add_html_block_elt_args (const char *format, ...)
#else
add_html_block_elt_args (format, va_alist)
    const char *format;
    va_dcl
#endif
{
  char buffer[2000]; /* xx no fixed limits */
#ifdef VA_FPRINTF
  va_list ap;
#endif

  VA_START (ap, format);
#ifdef VA_SPRINTF
  VA_SPRINTF (buffer, format, ap);
#else
  sprintf (buffer, format, a1, a2, a3, a4, a5, a6, a7, a8);
#endif /* not VA_SPRINTF */
  va_end (ap);
  add_html_block_elt (buffer);
}

/* Here is another awful kludge, used in add_char.  Ordinarily, macro
   expansions take place in the body of the document, and therefore we
   should html_output_head when we see one.  But there's an exception: a
   macro call might take place within @copying, and that does not start
   the real output, even though we fully expand the copying text.

   So we need to be able to check if we are defining the @copying text.
   We do this by looking back through the insertion stack.  */
static int
defining_copying (void)
{
  INSERTION_ELT *i;
  for (i = insertion_stack; i; i = i->next)
    {
      if (i->insertion == copying)
        return 1;
    }
  return 0;
}


/* Add the character to the current paragraph.  If filling_enabled is
   nonzero, then do filling as well. */
void
add_char (int character)
{
  if (xml)
    {
      xml_add_char (character);
      return;
    }

  /* If we are avoiding outputting headers, and we are currently
     in a menu, then simply return.  But if we're only expanding macros,
     then we're being called from glean_node_from_menu to try to
     remember a menu reference, and we need that so we can do defaulting.  */
  if (no_headers && !only_macro_expansion && (in_menu || in_detailmenu))
    return;

  /* If we are adding a character now, then we don't have to
     ignore close_paragraph () calls any more. */
  if (must_start_paragraph && character != '\n')
    {
      must_start_paragraph = 0;
      line_already_broken = 0;  /* The line is no longer broken. */
      if (current_indent > output_column)
        {
          indent (current_indent - output_column);
          output_column = current_indent;
        }
    }

  if (non_splitting_words
      && !(html && in_html_elt)
      && strchr (" \t\n", character))
    {
      if (html || docbook)
        { /* Seems cleaner to use &nbsp; than an 8-bit char.  */
          int saved_escape_html = escape_html;
          escape_html = 0;
          add_word ("&nbsp");
          escape_html = saved_escape_html;
          character = ';';
        }
      else
        character = META (' '); /* unmeta-d in flush_output */
    }

  insertion_paragraph_closed = 0;

  switch (character)
    {
    case '\n':
      if (!filling_enabled && !(html && (in_menu || in_detailmenu)))
        {
          insert ('\n');

          if (force_flush_right)
            {
              close_paragraph ();
              /* Hack to force single blank lines out in this mode. */
              flush_output ();
            }

          output_column = 0;

          if (!no_indent && paragraph_is_open)
            indent (output_column = current_indent);
          break;
        }
      else if (end_of_sentence_p ())
        /* CHARACTER is newline, and filling is enabled. */
        {
          insert (' ');
          output_column++;
          last_inserted_character = character;
        }

      if (last_char_was_newline)
        {
          if (html)
            last_char_was_newline++;
          close_paragraph ();
          pending_indent = 0;
        }
      else
        {
          last_char_was_newline = 1;
          if (html)
            insert ('\n');
          else
            insert (' ');
          output_column++;
        }
      break;

    default: /* not at newline */
      {
        int len = get_char_len (character);
        int suppress_insert = 0;

        if ((character == ' ') && (last_char_was_newline))
          {
            if (!paragraph_is_open)
              {
                pending_indent++;
                return;
              }
          }

        /* This is sad, but it seems desirable to not force any
           particular order on the front matter commands.  This way,
           the document can do @settitle, @documentlanguage, etc, in
           any order and with any omissions, and we'll still output
           the html <head> `just in time'.  */
        if ((executing_macro || !executing_string)
            && !only_macro_expansion
            && html && !html_output_head_p && !defining_copying ())
          html_output_head ();

        if (!paragraph_is_open)
          {
            start_paragraph ();
            /* If the paragraph is supposed to be indented a certain
               way, then discard all of the pending whitespace.
               Otherwise, we let the whitespace stay. */
            if (!paragraph_start_indent)
              indent (pending_indent);
            pending_indent = 0;

            /* This check for in_html_block_level_elt prevents <p> from being
               inserted when we already have html markup starting a paragraph,
               as with <ul> and <h1> and the like.  */
            if (html && !in_html_block_level_elt)
              {
                if ((in_menu || in_detailmenu) && in_menu_item)
                  {
                    insert_string ("</li></ul>\n");
                    in_menu_item = 0;
                  }
                insert_string ("<p>");
                in_paragraph = 1;
                adjust_braces_following (0, 3); /* adjust for <p> */
              }
          }

        output_column += len;
        if (output_column > fill_column)
          {
            if (filling_enabled && !html)
              {
                int temp = output_paragraph_offset;
                while (--temp > 0 && output_paragraph[temp] != '\n')
                  {
                    /* If we have found a space, we have the place to break
                       the line. */
                    if (output_paragraph[temp] == ' ')
                      {
                        /* Remove trailing whitespace from output. */
                        while (temp && whitespace (output_paragraph[temp - 1]))
                          temp--;

                        /* If we went back all the way to the newline of the
                           preceding line, it probably means that the word we
                           are adding is itself wider than the space that the
                           indentation and the fill_column let us use.  In
                           that case, do NOT insert another newline, since it
                           won't help.  Just indent to current_indent and
                           leave it alone, since that's the most we can do.  */
                        if (temp && output_paragraph[temp - 1] != '\n')
                          output_paragraph[temp++] = '\n';

                        /* We have correctly broken the line where we want
                           to.  What we don't want is spaces following where
                           we have decided to break the line.  We get rid of
                           them. */
                        {
                          int t1 = temp;

                          for (;; t1++)
                            {
                              if (t1 == output_paragraph_offset)
                                {
                                  if (whitespace (character))
                                    suppress_insert = 1;
                                  break;
                                }
                              if (!whitespace (output_paragraph[t1]))
                                break;
                            }

                          if (t1 != temp)
                            {
                              adjust_braces_following (temp, (- (t1 - temp)));
                              memmove (&output_paragraph[temp],
                                       &output_paragraph[t1],
                                       output_paragraph_offset - t1);
                              output_paragraph_offset -= (t1 - temp);
                            }
                        }

                        /* Filled, but now indent if that is right. */
                        if (indented_fill && current_indent > 0)
                          {
                            int buffer_len = ((output_paragraph_offset - temp)
                                              + current_indent);
                            char *temp_buffer = xmalloc (buffer_len);
                            int indentation = 0;

                            /* We have to shift any markers that are in
                               front of the wrap point. */
                            adjust_braces_following (temp, current_indent);

                            while (current_indent > 0 &&
                                   indentation != current_indent)
                              temp_buffer[indentation++] = ' ';

                            memcpy ((char *) &temp_buffer[current_indent],
                                     (char *) &output_paragraph[temp],
                                     buffer_len - current_indent);

                            if (output_paragraph_offset + buffer_len
                                >= paragraph_buffer_len)
                              {
                                unsigned char *tt = xrealloc
                                  (output_paragraph,
                                   (paragraph_buffer_len += buffer_len));
                                output_paragraph = tt;
                              }
                            memcpy ((char *) &output_paragraph[temp],
                                     temp_buffer, buffer_len);
                            output_paragraph_offset += current_indent;
                            free (temp_buffer);
                          }
                        output_column = 0;
                        while (temp < output_paragraph_offset)
                          output_column +=
                            get_char_len (output_paragraph[temp++]);
                        output_column += len;
                        break;
                      }
                  }
              }
          }

        if (!suppress_insert)
          {
            insert (character);
            last_inserted_character = character;
          }
        last_char_was_newline = 0;
        line_already_broken = 0;
      }
    }
}

/* Add a character and store its position in meta_char_pos.  */
void
add_meta_char (int character)
{
  meta_char_pos = output_paragraph_offset;
  add_char (character);
}

/* Insert CHARACTER into `output_paragraph'. */
void
insert (int character)
{
  /* We don't want to strip trailing whitespace in multitables.  Otherwise
     horizontal separators confuse the font locking in Info mode in Emacs,
     because it looks like a @subsection.  Adding a trailing space to those
     lines fixes it.  */
  if (character == '\n' && !html && !xml && !multitable_active)
    {
      while (output_paragraph_offset
	     && whitespace (output_paragraph[output_paragraph_offset-1]))
	output_paragraph_offset--;
    }

  output_paragraph[output_paragraph_offset++] = character;
  if (output_paragraph_offset == paragraph_buffer_len)
    {
      output_paragraph =
        xrealloc (output_paragraph, (paragraph_buffer_len += 100));
    }
}

/* Insert the null-terminated string STRING into `output_paragraph'.  */
void
insert_string (const char *string)
{
  while (*string)
    insert (*string++);
}


/* Sentences might have these characters after the period (or whatever).  */
#define POST_SENTENCE(c) ((c) == ')' || (c) == '\'' || (c) == '"' \
                          || (c) == ']')

/* Return true if at an end-of-sentence character, possibly followed by
   post-sentence punctuation to ignore.  */
static int
end_of_sentence_p (void)
{
  int loc = output_paragraph_offset - 1;

  /* If nothing has been output, don't check output_paragraph[-1].  */
  if (loc < 0)
    return 0;

  /* A post-sentence character that is at meta_char_pos is not really
     a post-sentence character; it was produced by a markup such as
     @samp.  We don't want the period inside @samp to be treated as a
     sentence ender. */
  while (loc > 0
         && loc != meta_char_pos && POST_SENTENCE (output_paragraph[loc]))
    loc--;
  return loc != meta_char_pos && sentence_ender (output_paragraph[loc]);
}


/* Remove upto COUNT characters of whitespace from the
   the current output line.  If COUNT is less than zero,
   then remove until none left. */
void
kill_self_indent (int count)
{
  /* Handle infinite case first. */
  if (count < 0)
    {
      output_column = 0;
      while (output_paragraph_offset)
        {
          if (whitespace (output_paragraph[output_paragraph_offset - 1]))
            output_paragraph_offset--;
          else
            break;
        }
    }
  else
    {
      while (output_paragraph_offset && count--)
        if (whitespace (output_paragraph[output_paragraph_offset - 1]))
          output_paragraph_offset--;
        else
          break;
    }
}

/* Nonzero means do not honor calls to flush_output (). */
static int flushing_ignored = 0;

/* Prevent calls to flush_output () from having any effect. */
void
inhibit_output_flushing (void)
{
  flushing_ignored++;
}

/* Allow calls to flush_output () to write the paragraph data. */
void
uninhibit_output_flushing (void)
{
  flushing_ignored--;
}

void
flush_output (void)
{
  int i;

  if (!output_paragraph_offset || flushing_ignored)
    return;

  for (i = 0; i < output_paragraph_offset; i++)
    {
      if (output_paragraph[i] == '\n')
        {
          output_line_number++;
          node_line_number++;
        }

      /* If we turned on the 8th bit for a space inside @w, turn it
         back off for output.  This might be problematic, since the
         0x80 character may be used in 8-bit character sets.  Sigh.
         In any case, don't do this for HTML, since the nbsp character
         is valid input and must be passed along to the browser.  */
      if (!html && (output_paragraph[i] & meta_character_bit))
        {
          int temp = UNMETA (output_paragraph[i]);
          if (temp == ' ')
            output_paragraph[i] &= 0x7f;
        }
    }

  fwrite (output_paragraph, 1, output_paragraph_offset, output_stream);

  output_position += output_paragraph_offset;
  output_paragraph_offset = 0;
  meta_char_pos = 0;
}

/* How to close a paragraph controlling the number of lines between
   this one and the last one. */

/* Paragraph spacing is controlled by this variable.  It is the number of
   blank lines that you wish to appear between paragraphs.  A value of
   1 creates a single blank line between paragraphs. */
int paragraph_spacing = DEFAULT_PARAGRAPH_SPACING;

static void
close_paragraph_with_lines (int lines)
{
  int old_spacing = paragraph_spacing;
  paragraph_spacing = lines;
  close_paragraph ();
  paragraph_spacing = old_spacing;
}

/* Close the current paragraph, leaving no blank lines between them. */
void
close_single_paragraph (void)
{
  close_paragraph_with_lines (0);
}

/* Close a paragraph after an insertion has ended. */
void
close_insertion_paragraph (void)
{
  if (!insertion_paragraph_closed)
    {
      /* Close the current paragraph, breaking the line. */
      close_single_paragraph ();

      /* Start a new paragraph, with the correct indentation for the now
         current insertion level (one above the one that we are ending). */
      start_paragraph ();

      /* Tell `close_paragraph' that the previous line has already been
         broken, so it should insert one less newline. */
      line_already_broken = 1;

      /* Tell functions such as `add_char' we've already found a newline. */
      ignore_blank_line ();
    }
  else
    {
      /* If the insertion paragraph is closed already, then we are seeing
         two `@end' commands in a row.  Note that the first one we saw was
         handled in the first part of this if-then-else clause, and at that
         time `start_paragraph' was called, partially to handle the proper
         indentation of the current line.  However, the indentation level
         may have just changed again, so we may have to outdent the current
         line to the new indentation level. */
      if (current_indent < output_column)
        kill_self_indent (output_column - current_indent);
    }

  insertion_paragraph_closed = 1;
}

/* Close the currently open paragraph. */
void
close_paragraph (void)
{
  int i;

  /* We don't need these newlines in XML and Docbook outputs for
     paragraph seperation.  We have <para> element for that.  */
  if (xml)
    return;

  /* The insertion paragraph is no longer closed. */
  insertion_paragraph_closed = 0;

  if (paragraph_is_open && !must_start_paragraph)
    {
      int tindex = output_paragraph_offset;

      /* Back up to last non-newline/space character, forcing all such
         subsequent characters to be newlines.  This isn't strictly
         necessary, but a couple of functions use the presence of a newline
         to make decisions. */
      for (tindex = output_paragraph_offset - 1; tindex >= 0; --tindex)
        {
          int c = output_paragraph[tindex];

          if (c == ' '|| c == '\n')
            output_paragraph[tindex] = '\n';
          else
            break;
        }

      /* All trailing whitespace is ignored. */
      output_paragraph_offset = ++tindex;

      /* Break the line if that is appropriate. */
      if (paragraph_spacing >= 0)
        insert ('\n');

      /* Add as many blank lines as is specified in `paragraph_spacing'. */
      if (!force_flush_right)
        {
          for (i = 0; i < (paragraph_spacing - line_already_broken); i++)
            {
              insert ('\n');
              /* Don't need anything extra for HTML in usual case of no
                 extra paragraph spacing.  */
              if (html && i > 0)
                insert_string ("<br>");
            }
        }

      /* If we are doing flush right indentation, then do it now
         on the paragraph (really a single line). */
      if (force_flush_right)
        do_flush_right_indentation ();

      flush_output ();
      paragraph_is_open = 0;
      no_indent = 0;
      output_column = 0;
    }

  ignore_blank_line ();
}

/* Make the last line just read look as if it were only a newline. */
void
ignore_blank_line (void)
{
  last_inserted_character = '\n';
  last_char_was_newline = 1;
}

/* Align the end of the text in output_paragraph with fill_column. */
static void
do_flush_right_indentation (void)
{
  char *temp;
  int temp_len;

  kill_self_indent (-1);

  if (output_paragraph[0] != '\n')
    {
      output_paragraph[output_paragraph_offset] = 0;

      if (output_paragraph_offset < fill_column)
        {
          int i;

          if (fill_column >= paragraph_buffer_len)
            output_paragraph =
              xrealloc (output_paragraph,
                        (paragraph_buffer_len += fill_column));

          temp_len = strlen ((char *)output_paragraph);
          temp = xmalloc (temp_len + 1);
          memcpy (temp, (char *)output_paragraph, temp_len);

          for (i = 0; i < fill_column - output_paragraph_offset; i++)
            output_paragraph[i] = ' ';

          memcpy ((char *)output_paragraph + i, temp, temp_len);
          free (temp);
          output_paragraph_offset = fill_column;
          adjust_braces_following (0, i);
        }
    }
}

/* Begin a new paragraph. */
void
start_paragraph (void)
{
  /* First close existing one. */
  if (paragraph_is_open)
    close_paragraph ();

  /* In either case, the insertion paragraph is no longer closed. */
  insertion_paragraph_closed = 0;

  /* However, the paragraph is open! */
  paragraph_is_open = 1;

  /* If we MUST_START_PARAGRAPH, that simply means that start_paragraph ()
     had to be called before we would allow any other paragraph operations
     to have an effect. */
  if (!must_start_paragraph)
    {
      int amount_to_indent = 0;

      /* If doing indentation, then insert the appropriate amount. */
      if (!no_indent)
        {
          if (inhibit_paragraph_indentation)
            {
              amount_to_indent = current_indent;
              if (inhibit_paragraph_indentation < 0)
                inhibit_paragraph_indentation++;
            }
          else if (paragraph_start_indent < 0)
            amount_to_indent = current_indent;
          else
            amount_to_indent = current_indent + paragraph_start_indent;

          if (amount_to_indent >= output_column)
            {
              amount_to_indent -= output_column;
              indent (amount_to_indent);
              output_column += amount_to_indent;
            }
        }
    }
  else
    must_start_paragraph = 0;
}

/* Insert the indentation specified by AMOUNT. */
void
indent (int amount)
{
  /* For every START_POS saved within the brace stack which will be affected
     by this indentation, bump that start pos forward. */
  adjust_braces_following (output_paragraph_offset, amount);

  while (--amount >= 0)
    insert (' ');
}

/* Search forward for STRING in input_text.
   FROM says where where to start. */
int
search_forward (char *string, int from)
{
  int len = strlen (string);

  while (from < input_text_length)
    {
      if (strncmp (input_text + from, string, len) == 0)
        return from;
      from++;
    }
  return -1;
}

/* search_forward until n characters.  */
int
search_forward_until_pos (char *string, int from, int end_pos)
{
  int save_input_text_length = input_text_length;
  input_text_length = end_pos;

  from = search_forward (string, from);

  input_text_length = save_input_text_length;

  return from;
}

/* Return next non-whitespace and non-cr character.  */
int
next_nonwhitespace_character (void)
{
  /* First check the current input_text.  Start from the next char because
     we already have input_text[input_text_offset] in ``current''.  */
  int pos = input_text_offset + 1;

  while (pos < input_text_length)
    {
      if (!cr_or_whitespace(input_text[pos]))
        return input_text[pos];
      pos++;
    }

  { /* Can't find a valid character, so go through filestack
       in case we are doing @include or expanding a macro.  */
    FSTACK *tos = filestack;

    while (tos)
      {
        int tmp_input_text_length = filestack->size;
        int tmp_input_text_offset = filestack->offset;
        char *tmp_input_text = filestack->text;

        while (tmp_input_text_offset < tmp_input_text_length)
          {
            if (!cr_or_whitespace(tmp_input_text[tmp_input_text_offset]))
              return tmp_input_text[tmp_input_text_offset];
            tmp_input_text_offset++;
          }

        tos = tos->next;
      }
  }

  return -1;
}

/* An external image is a reference, kind of.  The parsing is (not
   coincidentally) similar, anyway.  */
void
cm_image (int arg)
{
  char *name_arg, *w_arg, *h_arg, *alt_arg, *ext_arg;

  if (arg == END)
    return;

  name_arg = get_xref_token (1); /* expands all macros in image */
  w_arg = get_xref_token (0);
  h_arg = get_xref_token (0);
  alt_arg = get_xref_token (1); /* expands all macros in alt text */
  ext_arg = get_xref_token (0);

  if (*name_arg)
    {
      struct stat file_info;
      char *pathname = NULL;
      char *fullname = xmalloc (strlen (name_arg)
                       + (ext_arg && *ext_arg ? strlen (ext_arg) + 1: 4) + 1);

      if (ext_arg && *ext_arg)
        {
          sprintf (fullname, "%s%s", name_arg, ext_arg);
          if (access (fullname, R_OK) != 0)
            pathname = get_file_info_in_path (fullname, include_files_path,
                                              &file_info);

	  if (pathname == NULL)
	    {
	      /* Backwards compatibility (4.6 <= version < 4.7):
		 try prefixing @image's EXTENSION parameter with a period. */
	      sprintf (fullname, "%s.%s", name_arg, ext_arg);
	      if (access (fullname, R_OK) != 0)
		pathname = get_file_info_in_path (fullname, include_files_path,
						  &file_info);
	    }
        }
      else
        {
          sprintf (fullname, "%s.png", name_arg);
          if (access (fullname, R_OK) != 0) {
            pathname = get_file_info_in_path (fullname,
                                              include_files_path, &file_info);
            if (pathname == NULL) {
              sprintf (fullname, "%s.jpg", name_arg);
              if (access (fullname, R_OK) != 0) {
                sprintf (fullname, "%s.gif", name_arg);
                if (access (fullname, R_OK) != 0) {
                  pathname = get_file_info_in_path (fullname,
                                               include_files_path, &file_info);
                }
              }
            }
          }
        }

      if (html)
        {
          int image_in_div = 0;

          if (pathname == NULL && access (fullname, R_OK) != 0)
            {
              line_error(_("@image file `%s' (for HTML) not readable: %s"),
                             fullname, strerror (errno));
              return;
            }
          if (pathname != NULL && access (pathname, R_OK) != 0)
            {
              line_error (_("No such file `%s'"),
                          fullname);
              return;
            }

          if (!paragraph_is_open)
            {
              add_html_block_elt ("<div class=\"block-image\">");
              image_in_div = 1;
            }

          add_html_elt ("<img src=");
          add_word_args ("\"%s\"", fullname);
          add_html_elt (" alt=");
          add_word_args ("\"%s\">", 
              escape_string (*alt_arg ? text_expansion (alt_arg) : fullname));

          if (image_in_div)
            add_html_block_elt ("</div>");
        }
      else if (xml && docbook)
        xml_insert_docbook_image (name_arg);
      else if (xml)
        {
          extern int xml_in_para;
          extern int xml_no_para;
          int elt = xml_in_para ? INLINEIMAGE : IMAGE;

          if (!xml_in_para)
            xml_no_para++;

          xml_insert_element_with_attribute (elt,
              START, "width=\"%s\" height=\"%s\" name=\"%s\" extension=\"%s\"",
              w_arg, h_arg, name_arg, ext_arg);
          xml_insert_element (IMAGEALTTEXT, START);
          execute_string ("%s", alt_arg);
          xml_insert_element (IMAGEALTTEXT, END);
          xml_insert_element (elt, END);

          if (!xml_in_para)
            xml_no_para--;
        }
      else
        { /* Try to open foo.EXT or foo.txt.  */
          FILE *image_file;
          char *txtpath = NULL;
          char *txtname = xmalloc (strlen (name_arg)
                                   + (ext_arg && *ext_arg
                                      ? strlen (ext_arg) : 4) + 1);
          strcpy (txtname, name_arg);
          strcat (txtname, ".txt");
          image_file = fopen (txtname, "r");
          if (image_file == NULL)
            {
              txtpath = get_file_info_in_path (txtname,
                                               include_files_path, &file_info);
              if (txtpath != NULL)
                image_file = fopen (txtpath, "r");
            }

          if (image_file != NULL
              || access (fullname, R_OK) == 0
              || (pathname != NULL && access (pathname, R_OK) == 0))
            {
              int ch;
              int save_inhibit_indentation = inhibit_paragraph_indentation;
              int save_filling_enabled = filling_enabled;
              int image_in_brackets = paragraph_is_open;

              /* Write magic ^@^H[image ...^@^H] cookie in the info file, if
                 there's an accompanying bitmap.  Otherwise just include the
                 text image.  In the plaintext output, always include the text
                 image without the magic cookie.  */
              int use_magic_cookie = !no_headers
                && access (fullname, R_OK) == 0 && !STREQ (fullname, txtname);

              inhibit_paragraph_indentation = 1;
              filling_enabled = 0;
              last_char_was_newline = 0;

              if (use_magic_cookie)
                {
                  add_char ('\0');
                  add_word ("\010[image");

                  if (access (fullname, R_OK) == 0
                      || (pathname != NULL && access (pathname, R_OK) == 0))
                    add_word_args (" src=\"%s\"", fullname);

                  if (*alt_arg)
                    add_word_args (" alt=\"%s\"", alt_arg);
                }

              if (image_file != NULL)
                {
                  if (use_magic_cookie)
                    add_word (" text=\"");

                  if (image_in_brackets)
                    add_char ('[');

                  /* Maybe we need to remove the final newline if the image
                     file is only one line to allow in-line images.  On the
                     other hand, they could just make the file without a
                     final newline.  */
                  while ((ch = getc (image_file)) != EOF)
                    {
                      if (use_magic_cookie && (ch == '"' || ch == '\\'))
                        add_char ('\\');
                      add_char (ch);
                    }

                  if (image_in_brackets)
                    add_char (']');
                  
                  if (use_magic_cookie)
                    add_char ('"');

                  if (fclose (image_file) != 0)
                    perror (txtname);
                }

              if (use_magic_cookie)
                {
                  add_char ('\0');
                  add_word ("\010]");
                }

              inhibit_paragraph_indentation = save_inhibit_indentation;
              filling_enabled = save_filling_enabled;
            }
          else
            warning (_("@image file `%s' (for text) unreadable: %s"),
                        txtname, strerror (errno));
        }

      free (fullname);
      if (pathname)
        free (pathname);
    }
  else
    line_error (_("@image missing filename argument"));

  if (name_arg)
    free (name_arg);
  if (w_arg)
    free (w_arg);
  if (h_arg)
    free (h_arg);
  if (alt_arg)
    free (alt_arg);
  if (ext_arg)
    free (ext_arg);
}

/* Conditionals.  */

/* A structure which contains `defined' variables. */
typedef struct defines {
  struct defines *next;
  char *name;
  char *value;
} DEFINE;

/* The linked list of `set' defines. */
DEFINE *defines = NULL;

/* Add NAME to the list of `set' defines. */
static void
set (char *name, char *value)
{
  DEFINE *temp;

  for (temp = defines; temp; temp = temp->next)
    if (strcmp (name, temp->name) == 0)
      {
        free (temp->value);
        temp->value = xstrdup (value);
        return;
      }

  temp = xmalloc (sizeof (DEFINE));
  temp->next = defines;
  temp->name = xstrdup (name);
  temp->value = xstrdup (value);
  defines = temp;

  if (xml && !docbook)
    {
      xml_insert_element_with_attribute (SETVALUE, START, "name=\"%s\"", name);
      execute_string ("%s", value);
      xml_insert_element (SETVALUE, END);
    }
}

/* Remove NAME from the list of `set' defines. */
static void
clear (char *name)
{
  DEFINE *temp, *last;

  last = NULL;
  temp = defines;

  while (temp)
    {
      if (strcmp (temp->name, name) == 0)
        {
          if (last)
            last->next = temp->next;
          else
            defines = temp->next;

          free (temp->name);
          free (temp->value);
          free (temp);
          break;
        }
      last = temp;
      temp = temp->next;
    }

  if (xml && !docbook)
    {
      xml_insert_element_with_attribute (CLEARVALUE, START, "name=\"%s\"", name);
      xml_insert_element (CLEARVALUE, END);
    }
}

/* Return the value of NAME.  The return value is NULL if NAME is unset. */
static char *
set_p (char *name)
{
  DEFINE *temp;

  for (temp = defines; temp; temp = temp->next)
    if (strcmp (temp->name, name) == 0)
      return temp->value;

  return NULL;
}

/* Create a variable whose name appears as the first word on this line. */
void
cm_set (void)
{
  handle_variable (SET);
}

/* Remove a variable whose name appears as the first word on this line. */
void
cm_clear (void)
{
  handle_variable (CLEAR);
}

void
cm_ifset (void)
{
  handle_variable (IFSET);
}

void
cm_ifclear (void)
{
  handle_variable (IFCLEAR);
}

/* This command takes braces, but we parse the contents specially, so we
   don't use the standard brace popping code.

   The syntax @ifeq{arg1, arg2, texinfo-commands} performs texinfo-commands
   if ARG1 and ARG2 caselessly string compare to the same string, otherwise,
   it produces no output. */
void
cm_ifeq (void)
{
  char **arglist;

  arglist = get_brace_args (0);

  if (arglist)
    {
      if (array_len (arglist) > 1)
        {
          if ((strcasecmp (arglist[0], arglist[1]) == 0) &&
              (arglist[2]))
            execute_string ("%s\n", arglist[2]);
        }

      free_array (arglist);
    }
}

void
cm_value (int arg, int start_pos, int end_pos)
{
  static int value_level = 0, saved_meta_pos = -1;

  /* xml_add_char() skips any content inside menus when output format is
     Docbook, so @value{} is no use there.  Also start_pos and end_pos does not
     get updated, causing name to be empty string.  So just return.  */
   if (docbook && in_menu)
     return;

  /* All the text after @value{ upto the matching } will eventually
     disappear from output_paragraph, when this function is called
     with ARG == END.  If the text produced until then sets
     meta_char_pos, we will need to restore it to the value it had
     before @value was seen.  So we need to save the previous value
     of meta_char_pos here.  */
  if (arg == START)
    {
      /* If we are already inside some outer @value, don't overwrite
         the value saved in saved_meta_pos.  */
      if (!value_level)
        saved_meta_pos = meta_char_pos;
      value_level++;
      /* While the argument of @value is processed, we need to inhibit
         textual transformations like "--" into "-", since @set didn't
         do that when it grabbed the name of the variable.  */
      in_fixed_width_font++;
    }
  else
    {
      char *name = (char *) &output_paragraph[start_pos];
      char *value;
      output_paragraph[end_pos] = 0;
      name = xstrdup (name);
      value = set_p (name);
      output_column -= end_pos - start_pos;
      output_paragraph_offset = start_pos;

      /* Restore the previous value of meta_char_pos if the stuff
         inside this @value{} moved it.  */
      if (saved_meta_pos == -1) /* can't happen inside @value{} */
        abort ();
      if (value_level == 1
          && meta_char_pos >= start_pos && meta_char_pos < end_pos)
        {
          meta_char_pos = saved_meta_pos;
          saved_meta_pos = -1;
        }
      value_level--;
      /* No need to decrement in_fixed_width_font, since before
         we are called with arg == END, the reader loop already
         popped the brace stack, which restored in_fixed_width_font,
         among other things.  */

      if (value)
	{
	  /* We need to get past the closing brace since the value may
	     expand to a context-sensitive macro (e.g. @xref) and produce
	     spurious warnings */
	  input_text_offset++; 
	  execute_string ("%s", value);
	  input_text_offset--;
	}
      else
	{
          warning (_("undefined flag: %s"), name);
          add_word_args (_("{No value for `%s'}"), name);
	}

      free (name);
    }
}

/* Set, clear, or conditionalize based on ACTION. */
static void
handle_variable (int action)
{
  char *name;

  get_rest_of_line (0, &name);
  /* If we hit the end of text in get_rest_of_line, backing up
     input pointer will cause the last character of the last line
     be pushed back onto the input, which is wrong.  */
  if (input_text_offset < input_text_length)
    backup_input_pointer ();
  handle_variable_internal (action, name);
  free (name);
}

static void
handle_variable_internal (int action, char *name)
{
  char *temp;
  int delimiter, additional_text_present = 0;

  /* Only the first word of NAME is a valid tag. */
  temp = name;
  delimiter = 0;
  while (*temp && (delimiter || !whitespace (*temp)))
    {
/* #if defined (SET_WITH_EQUAL) */
      if (*temp == '"' || *temp == '\'')
        {
          if (*temp == delimiter)
            delimiter = 0;
          else
            delimiter = *temp;
        }
/* #endif SET_WITH_EQUAL */
      temp++;
    }

  if (*temp)
    additional_text_present++;

  *temp = 0;

  if (!*name)
    line_error (_("%c%s requires a name"), COMMAND_PREFIX, command);
  else
    {
      switch (action)
        {
        case SET:
          {
            char *value;

#if defined (SET_WITH_EQUAL)
            /* Allow a value to be saved along with a variable.  The value is
               the text following an `=' sign in NAME, if any is present. */

            for (value = name; *value && *value != '='; value++);

            if (*value)
              *value++ = 0;

            if (*value == '"' || *value == '\'')
              {
                value++;
                value[strlen (value) - 1] = 0;
              }

#else /* !SET_WITH_EQUAL */
            /* The VALUE of NAME is the remainder of the line sans
               whitespace. */
            if (additional_text_present)
              {
                value = temp + 1;
                canon_white (value);
              }
            else
              value = "";
#endif /* !SET_WITH_VALUE */

            set (name, value);
          }
          break;

        case CLEAR:
          clear (name);
          break;

        case IFSET:
        case IFCLEAR:
          /* If IFSET and NAME is not set, or if IFCLEAR and NAME is set,
             read lines from the the file until we reach a matching
             "@end CONDITION".  This means that we only take note of
             "@ifset/clear" and "@end" commands. */
          {
            char condition[8];
            int condition_len;
            int orig_line_number = line_number;

            if (action == IFSET)
              strcpy (condition, "ifset");
            else
              strcpy (condition, "ifclear");

            condition_len = strlen (condition);

          if ((action == IFSET && !set_p (name))
              || (action == IFCLEAR && set_p (name)))
            {
              int level = 0, done = 0;

              while (!done && input_text_offset < input_text_length)
                {
                  char *freeable_line, *line;

                  get_rest_of_line (0, &freeable_line);

                  for (line = freeable_line; whitespace (*line); line++);

                  if (*line == COMMAND_PREFIX &&
                      (strncmp (line + 1, condition, condition_len) == 0))
                    level++;
                  else if (strncmp (line, "@end", 4) == 0)
                    {
                      char *cname = line + 4;
                      char *temp;

                      while (*cname && whitespace (*cname))
                        cname++;
                      temp = cname;

                      while (*temp && !whitespace (*temp))
                        temp++;
                      *temp = 0;

                      if (strcmp (cname, condition) == 0)
                        {
                          if (!level)
                            {
                              done = 1;
                            }
                          else
                            level--;
                        }
                    }
                  free (freeable_line);
                }

              if (!done)
                file_line_error (input_filename, orig_line_number,
                                 _("Reached eof before matching @end %s"),
                                 condition);

              /* We found the end of a false @ifset/ifclear.  If we are
                 in a menu, back up over the newline that ends the ifset,
                 since that newline may also begin the next menu entry. */
              break;
            }
          else
            {
              if (action == IFSET)
                begin_insertion (ifset);
              else
                begin_insertion (ifclear);
            }
          }
          break;
        }
    }
}

/* Execution of random text not in file. */
typedef struct {
  char *string;                 /* The string buffer. */
  int size;                     /* The size of the buffer. */
  int in_use;                   /* Nonzero means string currently in use. */
} EXECUTION_STRING;

static EXECUTION_STRING **execution_strings = NULL;
static int execution_strings_index = 0;
static int execution_strings_slots = 0;

static EXECUTION_STRING *
get_execution_string (int initial_size)
{
  int i = 0;
  EXECUTION_STRING *es = NULL;

  if (execution_strings)
    {
      for (i = 0; i < execution_strings_index; i++)
        if (execution_strings[i] && (execution_strings[i]->in_use == 0))
          {
            es = execution_strings[i];
            break;
          }
    }

  if (!es)
    {
      if (execution_strings_index + 1 >= execution_strings_slots)
        {
          execution_strings = xrealloc
            (execution_strings,
             (execution_strings_slots += 3) * sizeof (EXECUTION_STRING *));
          for (; i < execution_strings_slots; i++)
            execution_strings[i] = NULL;
        }

      execution_strings[execution_strings_index] =
        xmalloc (sizeof (EXECUTION_STRING));
      es = execution_strings[execution_strings_index];
      execution_strings_index++;

      es->size = 0;
      es->string = NULL;
      es->in_use = 0;
    }

  if (initial_size > es->size)
    {
      es->string = xrealloc (es->string, initial_size);
      es->size = initial_size;
    }
  return es;
}

/* Given a pointer to TEXT and its desired length NEW_LEN, find TEXT's
   entry in the execution_strings[] array and change the .STRING and
   .SIZE members of that entry as appropriate.  */
void
maybe_update_execution_strings (char **text, unsigned int new_len)
{
  int i = 0;

  if (execution_strings)
    {
      for (i = 0; i < execution_strings_index; i++)
        if (execution_strings[i] && (execution_strings[i]->in_use == 1) &&
            execution_strings[i]->string == *text)
          {
            /* Don't ever shrink the string storage in execution_strings[]!
               execute_string assumes that it is always big enough to store
               every possible execution_string, and will break if that's
               not true.  So we only enlarge the string storage if the
               current size isn't big enough.  */
            if (execution_strings[i]->size < new_len)
              {
                execution_strings[i]->string =
                  *text = xrealloc (*text, new_len + 1);
                execution_strings[i]->size = new_len + 1;
              }
            return;
          }
    }
  /* We should *never* end up here, since if we are inside
     execute_string, TEXT is always in execution_strings[].  */
  abort ();
}

/* FIXME: this is an arbitrary limit.  */
#define EXECUTE_STRING_MAX 16*1024

/* Execute the string produced by formatting the ARGs with FORMAT.  This
   is like submitting a new file with @include. */
void
#if defined (VA_FPRINTF) && __STDC__
execute_string (char *format, ...)
#else
execute_string (format, va_alist)
    char *format;
    va_dcl
#endif
{
  EXECUTION_STRING *es;
  char *temp_string, *temp_input_filename;
#ifdef VA_FPRINTF
  va_list ap;
#endif
  int insertion_level_at_start = insertion_level;

  es = get_execution_string (EXECUTE_STRING_MAX);
  temp_string = es->string;
  es->in_use = 1;

  VA_START (ap, format);
#ifdef VA_SPRINTF
  VA_SPRINTF (temp_string, format, ap);
#else
  sprintf (temp_string, format, a1, a2, a3, a4, a5, a6, a7, a8);
#endif /* not VA_SPRINTF */
  va_end (ap);

  pushfile ();
  input_text_offset = 0;
  input_text = temp_string;
  input_text_length = strlen (temp_string);
  input_filename = xstrdup (input_filename);
  temp_input_filename = input_filename;

  executing_string++;
  reader_loop ();

  /* If insertion stack level changes during execution, that means a multiline
     command is used inside braces or @section ... kind of commands.  */
  if (insertion_level_at_start != insertion_level && !executing_macro)
    {
      line_error (_("Multiline command %c%s used improperly"),
          COMMAND_PREFIX,
          command);
      /* We also need to keep insertion_level intact to make sure warnings are
         issued for @end ... command.  */
      while (insertion_level > insertion_level_at_start)
        pop_insertion ();
    }

  popfile ();
  executing_string--;
  es->in_use = 0;
  free (temp_input_filename);
}


/* Return what would be output for STR (in newly-malloced memory), i.e.,
   expand Texinfo commands according to the current output format.  If
   IMPLICIT_CODE is set, expand @code{STR}.  This is generally used for
   short texts; filling, indentation, and html escapes are disabled.  */

char *
expansion (char *str, int implicit_code)
{
  return maybe_escaped_expansion (str, implicit_code, 0);
}


/* Do HTML escapes according to DO_HTML_ESCAPE.  Needed in
   cm_printindex, q.v.  */

char *
maybe_escaped_expansion (char *str, int implicit_code, int do_html_escape)
{
  char *result;

  /* Inhibit indentation and filling, so that extra newlines
     are not added to the expansion.  (This is undesirable if
     we write the expanded text to macro_expansion_output_stream.)  */
  int saved_filling_enabled = filling_enabled;
  int saved_indented_fill = indented_fill;
  int saved_no_indent = no_indent;
  int saved_escape_html = escape_html;

  filling_enabled = 0;
  indented_fill = 0;
  no_indent = 1;
  escape_html = do_html_escape;

  result = full_expansion (str, implicit_code);

  filling_enabled = saved_filling_enabled;
  indented_fill = saved_indented_fill;
  no_indent = saved_no_indent;
  escape_html = saved_escape_html;

  return result;
}


/* Expand STR (or @code{STR} if IMPLICIT_CODE is nonzero).  No change to
   any formatting parameters -- filling, indentation, html escapes,
   etc., are not reset.  Always returned in new memory.  */

char *
full_expansion (char *str, int implicit_code)
{
  int length;
  char *result;

  /* Inhibit any real output.  */
  int start = output_paragraph_offset;
  int saved_paragraph_is_open = paragraph_is_open;
  int saved_output_column = output_column;

  /* More output state to save.  */
  int saved_meta_pos = meta_char_pos;
  int saved_last_char = last_inserted_character;
  int saved_last_nl = last_char_was_newline;

  /* If we are called in the middle of processing a command, we need
     to dup and save the global variable `command' (which holds the
     name of this command), since the recursive reader loop will free
     it from under our feet if it finds any macros in STR.  */
  char *saved_command = command ? xstrdup (command) : NULL;

  inhibit_output_flushing ();
  paragraph_is_open = 1;
  if (strlen (str) > (implicit_code
                      ? EXECUTE_STRING_MAX - 1 - sizeof("@code{}")
                      : EXECUTE_STRING_MAX - 1))
    line_error (_("`%.40s...' is too long for expansion; not expanded"), str);
  else
    execute_string (implicit_code ? "@code{%s}" : "%s", str);
  uninhibit_output_flushing ();

  /* Copy the expansion from the buffer.  */
  length = output_paragraph_offset - start;
  result = xmalloc (1 + length);
  memcpy (result, (char *) (output_paragraph + start), length);
  result[length] = 0;

  /* Pretend it never happened.  */
  free_and_clear (&command);
  command = saved_command;

  output_paragraph_offset = start;
  paragraph_is_open = saved_paragraph_is_open;
  output_column = saved_output_column;

  meta_char_pos = saved_meta_pos;
  last_inserted_character = saved_last_char;
  last_char_was_newline = saved_last_nl;

  return result;
}


/* Return text (info) expansion of STR no matter what the current output
   format is.  */

char *
text_expansion (char *str)
{
  char *ret;
  int save_html = html;
  int save_xml = xml;
  int save_docbook = docbook;

  html = 0;
  xml = 0;
  docbook = 0;
  ret = expansion (str, 0);
  html = save_html;
  xml = save_xml;
  docbook = save_docbook;

  return ret;
}


/* Set the paragraph indentation variable to the value specified in STRING.
   Values can be:
     `asis': Don't change existing indentation.
     `none': Remove existing indentation.
        NUM: Indent NUM spaces at the starts of paragraphs.
             If NUM is zero, we assume `none'.
   Returns 0 if successful, or nonzero if STRING isn't one of the above. */
int
set_paragraph_indent (char *string)
{
  if (strcmp (string, "asis") == 0 || strcmp (string, _("asis")) == 0)
    paragraph_start_indent = 0;
  else if (strcmp (string, "none") == 0 || strcmp (string, _("none")) == 0)
    paragraph_start_indent = -1;
  else
    {
      if (sscanf (string, "%d", &paragraph_start_indent) != 1)
        return -1;
      else
        {
          if (paragraph_start_indent == 0)
            paragraph_start_indent = -1;
        }
    }
  return 0;
}
