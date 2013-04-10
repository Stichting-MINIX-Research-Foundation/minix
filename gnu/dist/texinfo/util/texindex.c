/*	$NetBSD: texindex.c,v 1.11 2008/09/02 08:00:24 christos Exp $	*/

/* texindex -- sort TeX index dribble output into an actual index.
   Id: texindex.c,v 1.11 2004/04/11 17:56:47 karl Exp

   Copyright (C) 1987, 1991, 1992, 1996, 1997, 1998, 1999, 2000, 2001,
   2002, 2003, 2004 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307. */

#include "system.h"
#include <getopt.h>

static char *program_name = "texindex";

#if defined (emacs)
#  include "../src/config.h"
/* Some s/os.h files redefine these. */
#  undef read
#  undef close
#  undef write
#  undef open
#endif

#if !defined (HAVE_MEMSET)
#undef memset
#define memset(ptr, ignore, count) bzero (ptr, count)
#endif

#if !defined (SEEK_SET)
#  define SEEK_SET 0
#  define SEEK_CUR 1
#  define SEEK_END 2
#endif /* !SEEK_SET */

/* When sorting in core, this structure describes one line
   and the position and length of its first keyfield.  */
struct lineinfo
{
  char *text;           /* The actual text of the line. */
  union {
    char *text;         /* The start of the key (for textual comparison). */
    long number;        /* The numeric value (for numeric comparison). */
  } key;
  long keylen;          /* Length of KEY field. */
};

/* This structure describes a field to use as a sort key. */
struct keyfield
{
  int startwords;       /* Number of words to skip. */
  int startchars;       /* Number of additional chars to skip. */
  int endwords;         /* Number of words to ignore at end. */
  int endchars;         /* Ditto for characters of last word. */
  char ignore_blanks;   /* Non-zero means ignore spaces and tabs. */
  char fold_case;       /* Non-zero means case doesn't matter. */
  char reverse;         /* Non-zero means compare in reverse order. */
  char numeric;         /* Non-zeros means field is ASCII numeric. */
  char positional;      /* Sort according to file position. */
  char braced;          /* Count balanced-braced groupings as fields. */
};

/* Vector of keyfields to use. */
struct keyfield keyfields[3];

/* Number of keyfields stored in that vector.  */
int num_keyfields = 3;

/* Vector of input file names, terminated with a null pointer. */
char **infiles;

/* Vector of corresponding output file names, or NULL, meaning default it
   (add an `s' to the end). */
char **outfiles;

/* Length of `infiles'. */
int num_infiles;

/* Pointer to the array of pointers to lines being sorted. */
char **linearray;

/* The allocated length of `linearray'. */
long nlines;

/* During in-core sort, this points to the base of the data block
   which contains all the lines of data.  */
char *text_base;

/* Initially 0; changed to 1 if we want initials in this index.  */
int need_initials;

/* Remembers the first initial letter seen in this index, so we can
   determine whether we need initials in the sorted form.  */
char first_initial;

/* Forward declarations of functions in this file. */
void decode_command (int argc, char **argv);
void sort_in_core (char *infile, int total, char *outfile);
char **parsefile (char *filename, char **nextline, char *data, long int size);
char *find_field (struct keyfield *keyfield, char *str, long int *lengthptr);
char *find_pos (char *str, int words, int chars, int ignore_blanks);
long find_value (char *start, long int length);
char *find_braced_pos (char *str, int words, int chars, int ignore_blanks);
char *find_braced_end (char *str);
void writelines (char **linearray, int nlines, FILE *ostream);
int compare_field (struct keyfield *keyfield, char *start1,
                   long int length1, long int pos1, char *start2,
                   long int length2, long int pos2);
int compare_full (const void *, const void *);
void pfatal_with_name (const char *name);
void fatal (const char *format, const char *arg);
void error (const char *format, const char *arg);
void *xmalloc (), *xrealloc ();
static char *concat3 (const char *, const char *, const char *);

int
main (int argc, char **argv)
{
  int i;

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* In case we write to a redirected stdout that fails.  */
  /* not ready atexit (close_stdout); */

  /* Describe the kind of sorting to do. */
  /* The first keyfield uses the first braced field and folds case. */
  keyfields[0].braced = 1;
  keyfields[0].fold_case = 1;
  keyfields[0].endwords = -1;
  keyfields[0].endchars = -1;

  /* The second keyfield uses the second braced field, numerically. */
  keyfields[1].braced = 1;
  keyfields[1].numeric = 1;
  keyfields[1].startwords = 1;
  keyfields[1].endwords = -1;
  keyfields[1].endchars = -1;

  /* The third keyfield (which is ignored while discarding duplicates)
     compares the whole line. */
  keyfields[2].endwords = -1;
  keyfields[2].endchars = -1;

  decode_command (argc, argv);

  /* Process input files completely, one by one.  */

  for (i = 0; i < num_infiles; i++)
    {
      int desc;
      off_t ptr;
      char *outfile;
      struct stat instat;

      desc = open (infiles[i], O_RDONLY, 0);
      if (desc < 0)
        pfatal_with_name (infiles[i]);

      if (stat (infiles[i], &instat))
        pfatal_with_name (infiles[i]);
      if (S_ISDIR (instat.st_mode))
        {
#ifdef EISDIR
          errno = EISDIR;
#endif
          pfatal_with_name (infiles[i]);
        }

      lseek (desc, (off_t) 0, SEEK_END);
      ptr = (off_t) lseek (desc, (off_t) 0, SEEK_CUR);

      close (desc);

      outfile = outfiles[i];
      if (!outfile)
        outfile = concat3 (infiles[i], "s", "");

      need_initials = 0;
      first_initial = '\0';

      if (ptr != (int)ptr)
	{
	  fprintf (stderr, "%s: %s: file too large\n", program_name,
		   infiles[i]);
	  xexit (1);
	}
      sort_in_core (infiles[i], (int)ptr, outfile);
    }

  xexit (0);
  return 0; /* Avoid bogus warnings.  */
}

typedef struct
{
  char *long_name;
  char *short_name;
  int *variable_ref;
  int variable_value;
  char *arg_name;
  char *doc_string;
} TEXINDEX_OPTION;

TEXINDEX_OPTION texindex_options[] = {
  { "--help", "-h", (int *)NULL, 0, (char *)NULL,
      N_("display this help and exit") },
  { "--output", "-o", (int *)NULL, 0, "FILE",
      N_("send output to FILE") },
  { "--version", (char *)NULL, (int *)NULL, 0, (char *)NULL,
      N_("display version information and exit") },
  { (char *)NULL, (char *)NULL, (int *)NULL, 0, (char *)NULL }
};

void
usage (int result_value)
{
  register int i;
  FILE *f = result_value ? stderr : stdout;

  fprintf (f, _("Usage: %s [OPTION]... FILE...\n"), program_name);
  fprintf (f, _("Generate a sorted index for each TeX output FILE.\n"));
  /* Avoid trigraph nonsense.  */
  fprintf (f,
_("Usually FILE... is specified as `foo.%c%c\' for a document `foo.texi'.\n"),
           '?', '?'); /* avoid trigraph in cat-id-tbl.c */
  fprintf (f, _("\nOptions:\n"));

  for (i = 0; texindex_options[i].long_name; i++)
    {
      putc (' ', f);

      if (texindex_options[i].short_name)
        fprintf (f, "%s, ", texindex_options[i].short_name);

      fprintf (f, "%s %s",
               texindex_options[i].long_name,
               texindex_options[i].arg_name
               ? texindex_options[i].arg_name : "");

      fprintf (f, "\t%s\n", _(texindex_options[i].doc_string));
    }
  fputs (_("\n\
Email bug reports to bug-texinfo@gnu.org,\n\
general questions and discussion to help-texinfo@gnu.org.\n\
Texinfo home page: http://www.gnu.org/software/texinfo/"), f);
  fputs ("\n", f);

  xexit (result_value);
}

/* Decode the command line arguments to set the parameter variables
   and set up the vector of keyfields and the vector of input files. */

void
decode_command (int argc, char **argv)
{
  int arg_index = 1;
  char **ip;
  char **op;

  /* Allocate ARGC input files, which must be enough.  */

  infiles = (char **) xmalloc (argc * sizeof (char *));
  outfiles = (char **) xmalloc (argc * sizeof (char *));
  ip = infiles;
  op = outfiles;

  while (arg_index < argc)
    {
      char *arg = argv[arg_index++];

      if (*arg == '-')
        {
          if (strcmp (arg, "--version") == 0)
            {
              printf ("texindex (GNU %s) %s\n", PACKAGE, VERSION);
              puts ("");
              puts ("Copyright (C) 2004 Free Software Foundation, Inc.");
              printf (_("There is NO warranty.  You may redistribute this software\n\
under the terms of the GNU General Public License.\n\
For more information about these matters, see the files named COPYING.\n"));
              xexit (0);
            }
          else if ((strcmp (arg, "--keep") == 0) ||
                   (strcmp (arg, "-k") == 0))
            {
	      /* Ignore, for backward compatibility */
            }
          else if ((strcmp (arg, "--help") == 0) ||
                   (strcmp (arg, "-h") == 0))
            {
              usage (0);
            }
          else if ((strcmp (arg, "--output") == 0) ||
                   (strcmp (arg, "-o") == 0))
            {
              if (argv[arg_index] != (char *)NULL)
                {
                  arg_index++;
                  if (op > outfiles)
                    *(op - 1) = argv[arg_index];
                }
              else
                usage (1);
            }
          else
            usage (1);
        }
      else
        {
          *ip++ = arg;
          *op++ = (char *)NULL;
        }
    }

  /* Record number of keyfields and terminate list of filenames. */
  num_infiles = ip - infiles;
  *ip = (char *)NULL;
  if (num_infiles == 0)
    usage (1);
}

/* Compare LINE1 and LINE2 according to the specified set of keyfields. */

int
compare_full (const void *p1, const void *p2)
{
  char **line1 = (char **) p1;
  char **line2 = (char **) p2;
  int i;

  /* Compare using the first keyfield;
     if that does not distinguish the lines, try the second keyfield;
     and so on. */

  for (i = 0; i < num_keyfields; i++)
    {
      long length1, length2;
      char *start1 = find_field (&keyfields[i], *line1, &length1);
      char *start2 = find_field (&keyfields[i], *line2, &length2);
      int tem = compare_field (&keyfields[i], start1, length1,
                               *line1 - text_base,
                               start2, length2, *line2 - text_base);
      if (tem)
        {
          if (keyfields[i].reverse)
            return -tem;
          return tem;
        }
    }

  return 0;                     /* Lines match exactly. */
}

/* Compare LINE1 and LINE2, described by structures
   in which the first keyfield is identified in advance.
   For positional sorting, assumes that the order of the lines in core
   reflects their nominal order.  */
int
compare_prepared (const void *p1, const void *p2)
{
  struct lineinfo *line1 = (struct lineinfo *) p1;
  struct lineinfo *line2 = (struct lineinfo *) p2;
  int i;
  int tem;
  char *text1, *text2;

  /* Compare using the first keyfield, which has been found for us already. */
  if (keyfields->positional)
    {
      if (line1->text - text_base > line2->text - text_base)
        tem = 1;
      else
        tem = -1;
    }
  else if (keyfields->numeric)
    tem = line1->key.number - line2->key.number;
  else
    tem = compare_field (keyfields, line1->key.text, line1->keylen, 0,
                         line2->key.text, line2->keylen, 0);
  if (tem)
    {
      if (keyfields->reverse)
        return -tem;
      return tem;
    }

  text1 = line1->text;
  text2 = line2->text;

  /* Compare using the second keyfield;
     if that does not distinguish the lines, try the third keyfield;
     and so on. */

  for (i = 1; i < num_keyfields; i++)
    {
      long length1, length2;
      char *start1 = find_field (&keyfields[i], text1, &length1);
      char *start2 = find_field (&keyfields[i], text2, &length2);
      int tem = compare_field (&keyfields[i], start1, length1,
                               text1 - text_base,
                               start2, length2, text2 - text_base);
      if (tem)
        {
          if (keyfields[i].reverse)
            return -tem;
          return tem;
        }
    }

  return 0;                     /* Lines match exactly. */
}

/* Like compare_full but more general.
   You can pass any strings, and you can say how many keyfields to use.
   POS1 and POS2 should indicate the nominal positional ordering of
   the two lines in the input.  */

int
compare_general (char *str1, char *str2, long int pos1, long int pos2, int use_keyfields)
{
  int i;

  /* Compare using the first keyfield;
     if that does not distinguish the lines, try the second keyfield;
     and so on. */

  for (i = 0; i < use_keyfields; i++)
    {
      long length1, length2;
      char *start1 = find_field (&keyfields[i], str1, &length1);
      char *start2 = find_field (&keyfields[i], str2, &length2);
      int tem = compare_field (&keyfields[i], start1, length1, pos1,
                               start2, length2, pos2);
      if (tem)
        {
          if (keyfields[i].reverse)
            return -tem;
          return tem;
        }
    }

  return 0;                     /* Lines match exactly. */
}

/* Find the start and length of a field in STR according to KEYFIELD.
   A pointer to the starting character is returned, and the length
   is stored into the int that LENGTHPTR points to.  */

char *
find_field (struct keyfield *keyfield, char *str, long int *lengthptr)
{
  char *start;
  char *end;
  char *(*fun) ();

  if (keyfield->braced)
    fun = find_braced_pos;
  else
    fun = find_pos;

  start = (*fun) (str, keyfield->startwords, keyfield->startchars,
                  keyfield->ignore_blanks);
  if (keyfield->endwords < 0)
    {
      if (keyfield->braced)
        end = find_braced_end (start);
      else
        {
          end = start;
          while (*end && *end != '\n')
            end++;
        }
    }
  else
    {
      end = (*fun) (str, keyfield->endwords, keyfield->endchars, 0);
      if (end - str < start - str)
        end = start;
    }
  *lengthptr = end - start;
  return start;
}

/* Return a pointer to a specified place within STR,
   skipping (from the beginning) WORDS words and then CHARS chars.
   If IGNORE_BLANKS is nonzero, we skip all blanks
   after finding the specified word.  */

char *
find_pos (char *str, int words, int chars, int ignore_blanks)
{
  int i;
  char *p = str;

  for (i = 0; i < words; i++)
    {
      char c;
      /* Find next bunch of nonblanks and skip them. */
      while ((c = *p) == ' ' || c == '\t')
        p++;
      while ((c = *p) && c != '\n' && !(c == ' ' || c == '\t'))
        p++;
      if (!*p || *p == '\n')
        return p;
    }

  while (*p == ' ' || *p == '\t')
    p++;

  for (i = 0; i < chars; i++)
    {
      if (!*p || *p == '\n')
        break;
      p++;
    }
  return p;
}

/* Like find_pos but assumes that each field is surrounded by braces
   and that braces within fields are balanced. */

char *
find_braced_pos (char *str, int words, int chars, int ignore_blanks)
{
  int i;
  int bracelevel;
  char *p = str;
  char c;

  for (i = 0; i < words; i++)
    {
      bracelevel = 1;
      while ((c = *p++) != '{' && c != '\n' && c)
        /* Do nothing. */ ;
      if (c != '{')
        return p - 1;
      while (bracelevel)
        {
          c = *p++;
          if (c == '{')
            bracelevel++;
          if (c == '}')
            bracelevel--;
          if (c == 0 || c == '\n')
            return p - 1;
        }
    }

  while ((c = *p++) != '{' && c != '\n' && c)
    /* Do nothing. */ ;

  if (c != '{')
    return p - 1;

  if (ignore_blanks)
    while ((c = *p) == ' ' || c == '\t')
      p++;

  for (i = 0; i < chars; i++)
    {
      if (!*p || *p == '\n')
        break;
      p++;
    }
  return p;
}

/* Find the end of the balanced-brace field which starts at STR.
   The position returned is just before the closing brace. */

char *
find_braced_end (char *str)
{
  int bracelevel;
  char *p = str;
  char c;

  bracelevel = 1;
  while (bracelevel)
    {
      c = *p++;
      if (c == '{')
        bracelevel++;
      if (c == '}')
        bracelevel--;
      if (c == 0 || c == '\n')
        return p - 1;
    }
  return p - 1;
}

long
find_value (char *start, long int length)
{
  while (length != 0L)
    {
      if (isdigit (*start))
        return atol (start);
      length--;
      start++;
    }
  return 0l;
}

/* Vector used to translate characters for comparison.
   This is how we make all alphanumerics follow all else,
   and ignore case in the first sorting.  */
int char_order[256];

void
init_char_order (void)
{
  int i;
  for (i = 1; i < 256; i++)
    char_order[i] = i;

  for (i = '0'; i <= '9'; i++)
    char_order[i] += 512;

  for (i = 'a'; i <= 'z'; i++)
    {
      char_order[i] = 512 + i;
      char_order[i + 'A' - 'a'] = 512 + i;
    }
}

/* Compare two fields (each specified as a start pointer and a character count)
   according to KEYFIELD.
   The sign of the value reports the relation between the fields. */

int
compare_field (struct keyfield *keyfield, char *start1, long int length1,
               long int pos1, char *start2, long int length2, long int pos2)
{
  if (keyfields->positional)
    {
      if (pos1 > pos2)
        return 1;
      else
        return -1;
    }
  if (keyfield->numeric)
    {
      long value = find_value (start1, length1) - find_value (start2, length2);
      if (value > 0)
        return 1;
      if (value < 0)
        return -1;
      return 0;
    }
  else
    {
      char *p1 = start1;
      char *p2 = start2;
      char *e1 = start1 + length1;
      char *e2 = start2 + length2;

      while (1)
        {
          int c1, c2;

          if (p1 == e1)
            c1 = 0;
          else
            c1 = *p1++;
          if (p2 == e2)
            c2 = 0;
          else
            c2 = *p2++;

          if (char_order[c1] != char_order[c2])
            return char_order[c1] - char_order[c2];
          if (!c1)
            break;
        }

      /* Strings are equal except possibly for case.  */
      p1 = start1;
      p2 = start2;
      while (1)
        {
          int c1, c2;

          if (p1 == e1)
            c1 = 0;
          else
            c1 = *p1++;
          if (p2 == e2)
            c2 = 0;
          else
            c2 = *p2++;

          if (c1 != c2)
            /* Reverse sign here so upper case comes out last.  */
            return c2 - c1;
          if (!c1)
            break;
        }

      return 0;
    }
}

/* Sort INFILE, whose size is TOTAL,
   assuming that is small enough to be done in-core,
   then indexify it and send the output to OUTFILE (or to stdout).  */

void
sort_in_core (char *infile, int total, char *outfile)
{
  char **nextline;
  char *data = (char *) xmalloc (total + 1);
  char *file_data;
  long file_size;
  int i;
  FILE *ostream = stdout;
  struct lineinfo *lineinfo;

  /* Read the contents of the file into the moby array `data'. */

  int desc = open (infile, O_RDONLY, 0);

  if (desc < 0)
    fatal (_("failure reopening %s"), infile);
  for (file_size = 0;;)
    {
      i = read (desc, data + file_size, total - file_size);
      if (i <= 0)
        break;
      file_size += i;
    }
  file_data = data;
  data[file_size] = 0;

  close (desc);

  if (file_size > 0 && data[0] != '\\' && data[0] != '@')
    {
      error (_("%s: not a texinfo index file"), infile);
      return;
    }

  init_char_order ();

  /* Sort routines want to know this address. */

  text_base = data;

  /* Create the array of pointers to lines, with a default size
     frequently enough.  */

  nlines = total / 50;
  if (!nlines)
    nlines = 2;
  linearray = (char **) xmalloc (nlines * sizeof (char *));

  /* `nextline' points to the next free slot in this array.
     `nlines' is the allocated size.  */

  nextline = linearray;

  /* Parse the input file's data, and make entries for the lines.  */

  nextline = parsefile (infile, nextline, file_data, file_size);
  if (nextline == 0)
    {
      error (_("%s: not a texinfo index file"), infile);
      return;
    }

  /* Sort the lines. */

  /* If we have enough space, find the first keyfield of each line in advance.
     Make a `struct lineinfo' for each line, which records the keyfield
     as well as the line, and sort them.  */

  lineinfo = malloc ((nextline - linearray) * sizeof (struct lineinfo));

  if (lineinfo)
    {
      struct lineinfo *lp;
      char **p;

      for (lp = lineinfo, p = linearray; p != nextline; lp++, p++)
        {
          lp->text = *p;
          lp->key.text = find_field (keyfields, *p, &lp->keylen);
          if (keyfields->numeric)
            lp->key.number = find_value (lp->key.text, lp->keylen);
        }

      qsort (lineinfo, nextline - linearray, sizeof (struct lineinfo),
             compare_prepared);

      for (lp = lineinfo, p = linearray; p != nextline; lp++, p++)
        *p = lp->text;

      free (lineinfo);
    }
  else
    qsort (linearray, nextline - linearray, sizeof (char *), compare_full);

  /* Open the output file. */

  if (outfile)
    {
      ostream = fopen (outfile, "w");
      if (!ostream)
        pfatal_with_name (outfile);
    }

  writelines (linearray, nextline - linearray, ostream);
  if (outfile)
    fclose (ostream);

  free (linearray);
  free (data);
}

/* Parse an input string in core into lines.
   DATA is the input string, and SIZE is its length.
   Data goes in LINEARRAY starting at NEXTLINE.
   The value returned is the first entry in LINEARRAY still unused.
   Value 0 means input file contents are invalid.  */

char **
parsefile (char *filename, char **nextline, char *data, long int size)
{
  char *p, *end;
  char **line = nextline;

  p = data;
  end = p + size;
  *end = 0;

  while (p != end)
    {
      if (p[0] != '\\' && p[0] != '@')
        return 0;

      *line = p;

      /* Find the first letter of the first field of this line.  If it
         is different from the first letter of the first field of the
         first line, we need initial headers in the output index.  */
      while (*p && *p != '{')
        p++;
      if (p == end)
        return 0;
      p++;
      if (first_initial)
        {
          if (first_initial != toupper (*p))
            need_initials = 1;
        }
      else
        first_initial = toupper (*p);

      while (*p && *p != '\n')
        p++;
      if (p != end)
        p++;

      line++;
      if (line == linearray + nlines)
        {
          char **old = linearray;
          linearray = xrealloc (linearray, sizeof (char *) * (nlines *= 4));
          line += linearray - old;
        }
    }

  return line;
}

/* Indexification is a filter applied to the sorted lines
   as they are being written to the output file.
   Multiple entries for the same name, with different page numbers,
   get combined into a single entry with multiple page numbers.
   The first braced field, which is used for sorting, is discarded.
   However, its first character is examined, folded to lower case,
   and if it is different from that in the previous line fed to us
   a \initial line is written with one argument, the new initial.

   If an entry has four braced fields, then the second and third
   constitute primary and secondary names.
   In this case, each change of primary name
   generates a \primary line which contains only the primary name,
   and in between these are \secondary lines which contain
   just a secondary name and page numbers. */

/* The last primary name we wrote a \primary entry for.
   If only one level of indexing is being done, this is the last name seen. */
char *lastprimary;
/* Length of storage allocated for lastprimary. */
int lastprimarylength;

/* Similar, for the secondary name. */
char *lastsecondary;
int lastsecondarylength;

/* Zero if we are not in the middle of writing an entry.
   One if we have written the beginning of an entry but have not
   yet written any page numbers into it.
   Greater than one if we have written the beginning of an entry
   plus at least one page number. */
int pending;

/* The initial (for sorting purposes) of the last primary entry written.
   When this changes, a \initial {c} line is written */

char *lastinitial;

int lastinitiallength;

/* When we need a string of length 1 for the value of lastinitial,
   store it here.  */

char lastinitial1[2];

/* Initialize static storage for writing an index. */

void
init_index (void)
{
  pending = 0;
  lastinitial = lastinitial1;
  lastinitial1[0] = 0;
  lastinitial1[1] = 0;
  lastinitiallength = 0;
  lastprimarylength = 100;
  lastprimary = (char *) xmalloc (lastprimarylength + 1);
  memset (lastprimary, '\0', lastprimarylength + 1);
  lastsecondarylength = 100;
  lastsecondary = (char *) xmalloc (lastsecondarylength + 1);
  memset (lastsecondary, '\0', lastsecondarylength + 1);
}

/* Indexify.  Merge entries for the same name,
   insert headers for each initial character, etc.  */

void
indexify (char *line, FILE *ostream)
{
  char *primary, *secondary, *pagenumber;
  int primarylength, secondarylength = 0, pagelength;
  int nosecondary;
  int initiallength;
  char *initial;
  char initial1[2];
  register char *p;

  /* First, analyze the parts of the entry fed to us this time. */

  p = find_braced_pos (line, 0, 0, 0);
  if (*p == '{')
    {
      initial = p;
      /* Get length of inner pair of braces starting at `p',
         including that inner pair of braces.  */
      initiallength = find_braced_end (p + 1) + 1 - p;
    }
  else
    {
      initial = initial1;
      initial1[0] = toupper (*p);
      initial1[1] = 0;
      initiallength = 1;
    }

  pagenumber = find_braced_pos (line, 1, 0, 0);
  pagelength = find_braced_end (pagenumber) - pagenumber;
  if (pagelength == 0)
    fatal (_("No page number in %s"), line);

  primary = find_braced_pos (line, 2, 0, 0);
  primarylength = find_braced_end (primary) - primary;

  secondary = find_braced_pos (line, 3, 0, 0);
  nosecondary = !*secondary;
  if (!nosecondary)
    secondarylength = find_braced_end (secondary) - secondary;

  /* If the primary is different from before, make a new primary entry. */
  if (strncmp (primary, lastprimary, primarylength))
    {
      /* Close off current secondary entry first, if one is open. */
      if (pending)
        {
          fputs ("}\n", ostream);
          pending = 0;
        }

      /* If this primary has a different initial, include an entry for
         the initial. */
      if (need_initials &&
          (initiallength != lastinitiallength ||
           strncmp (initial, lastinitial, initiallength)))
        {
          fprintf (ostream, "\\initial {");
          fwrite (initial, 1, initiallength, ostream);
          fputs ("}\n", ostream);
          if (initial == initial1)
            {
              lastinitial = lastinitial1;
              *lastinitial1 = *initial1;
            }
          else
            {
              lastinitial = initial;
            }
          lastinitiallength = initiallength;
        }

      /* Make the entry for the primary.  */
      if (nosecondary)
        fputs ("\\entry {", ostream);
      else
        fputs ("\\primary {", ostream);
      fwrite (primary, primarylength, 1, ostream);
      if (nosecondary)
        {
          fputs ("}{", ostream);
          pending = 1;
        }
      else
        fputs ("}\n", ostream);

      /* Record name of most recent primary. */
      if (lastprimarylength < primarylength)
        {
          lastprimarylength = primarylength + 100;
          lastprimary = (char *) xrealloc (lastprimary,
                                           1 + lastprimarylength);
        }
      strncpy (lastprimary, primary, primarylength);
      lastprimary[primarylength] = 0;

      /* There is no current secondary within this primary, now. */
      lastsecondary[0] = 0;
    }

  /* Should not have an entry with no subtopic following one with a
     subtopic. */

  if (nosecondary && *lastsecondary)
    error (_("entry %s follows an entry with a secondary name"), line);

  /* Start a new secondary entry if necessary. */
  if (!nosecondary && strncmp (secondary, lastsecondary, secondarylength))
    {
      if (pending)
        {
          fputs ("}\n", ostream);
          pending = 0;
        }

      /* Write the entry for the secondary.  */
      fputs ("\\secondary {", ostream);
      fwrite (secondary, secondarylength, 1, ostream);
      fputs ("}{", ostream);
      pending = 1;

      /* Record name of most recent secondary. */
      if (lastsecondarylength < secondarylength)
        {
          lastsecondarylength = secondarylength + 100;
          lastsecondary = (char *) xrealloc (lastsecondary,
                                             1 + lastsecondarylength);
        }
      strncpy (lastsecondary, secondary, secondarylength);
      lastsecondary[secondarylength] = 0;
    }

  /* Here to add one more page number to the current entry. */
  if (pending++ != 1)
    fputs (", ", ostream);  /* Punctuate first, if this is not the first. */
  fwrite (pagenumber, pagelength, 1, ostream);
}

/* Close out any unfinished output entry. */

void
finish_index (FILE *ostream)
{
  if (pending)
    fputs ("}\n", ostream);
  free (lastprimary);
  free (lastsecondary);
}

/* Copy the lines in the sorted order.
   Each line is copied out of the input file it was found in. */

void
writelines (char **linearray, int nlines, FILE *ostream)
{
  char **stop_line = linearray + nlines;
  char **next_line;

  init_index ();

  /* Output the text of the lines, and free the buffer space. */

  for (next_line = linearray; next_line != stop_line; next_line++)
    {
      /* Output the line only if distinct from previous one.  */
      if (next_line == linearray
      /* Compare previous line with this one, using only the
         explicitly specd keyfields. */
          || compare_general (*(next_line - 1), *next_line, 0L, 0L,
                              num_keyfields - 1))
        {
          char *p = *next_line;
          char c;

          while ((c = *p++) && c != '\n')
            /* Do nothing. */ ;
          *(p - 1) = 0;
          indexify (*next_line, ostream);
        }
    }

  finish_index (ostream);
}

/* Print error message and exit.  */

void
fatal (const char *format, const char *arg)
{
  error (format, arg);
  xexit (1);
}

/* Print error message.  FORMAT is printf control string, ARG is arg for it. */
void
error (const char *format, const char *arg)
{
  printf ("%s: ", program_name);
  printf (format, arg);
  if (format[strlen (format) -1] != '\n')
    printf ("\n");
}

void
perror_with_name (const char *name)
{
  fprintf (stderr, "%s: ", program_name);
  perror (name);
}

void
pfatal_with_name (const char *name)
{
  perror_with_name (name);
  xexit (1);
}


/* Return a newly-allocated string concatenating S1, S2, and S3.  */

static char *
concat3 (const char *s1, const char *s2, const char *s3)
{
  int len1 = strlen (s1), len2 = strlen (s2), len3 = strlen (s3);
  char *result = (char *) xmalloc (len1 + len2 + len3 + 1);

  strcpy (result, s1);
  strcpy (result + len1, s2);
  strcpy (result + len1 + len2, s3);
  *(result + len1 + len2 + len3) = 0;

  return result;
}
