/*	$NetBSD: filesys.h,v 1.1.1.5 2008/09/02 07:49:38 christos Exp $	*/

/* filesys.h -- external declarations for filesys.c.
   Id: filesys.h,v 1.3 2004/04/11 17:56:45 karl Exp

   Copyright (C) 1993, 1997, 1998, 2002, 2004 Free Software Foundation, Inc.

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

#ifndef INFO_FILESYS_H
#define INFO_FILESYS_H

/* The path on which we look for info files.  You can initialize this
   from the environment variable INFOPATH if there is one, or you can
   call info_add_path () to add paths to the beginning or end of it. */
extern char *infopath;

/* Make INFOPATH have absolutely nothing in it. */
extern void zap_infopath (void);

/* Add PATH to the list of paths found in INFOPATH.  2nd argument says
   whether to put PATH at the front or end of INFOPATH. */
extern void info_add_path (char *path, int where);

/* Defines that are passed along with the pathname to info_add_path (). */
#define INFOPATH_PREPEND 0
#define INFOPATH_APPEND  1

/* Expand the filename in PARTIAL to make a real name for this operating
   system.  This looks in INFO_PATHS in order to find the correct file.
   If it can't find the file, it returns NULL. */
extern char *info_find_fullpath (char *partial);

/* Given a chunk of text and its length, convert all CRLF pairs at the
   EOLs into a single Newline character.  Return the length of produced
   text.  */
long convert_eols (char *text, long textlen);

/* Read the contents of PATHNAME, returning a buffer with the contents of
   that file in it, and returning the size of that buffer in FILESIZE.
   FINFO is a stat struct which has already been filled in by the caller.
   If the file cannot be read, return a NULL pointer. */
extern char *filesys_read_info_file (char *pathname, long int *filesize,
    struct stat *finfo, int *is_compressed);

extern char *filesys_read_compressed (char *pathname, long int *filesize);

/* Return the command string that would be used to decompress FILENAME. */
extern char *filesys_decompressor_for_file (char *filename);
extern int compressed_filename_p (char *filename);

/* A function which returns a pointer to a static buffer containing
   an error message for FILENAME and ERROR_NUM. */
extern char *filesys_error_string (char *filename, int error_num);

/* The number of the most recent file system error. */
extern int filesys_error_number;

/* Given a string containing units of information separated by colons,
   return the next one pointed to by IDX, or NULL if there are no more.
   Advance IDX to the character after the colon. */
extern char *extract_colon_unit (char *string, int *idx);

/* Return true if FILENAME is `dir', with a possible compression suffix.  */
extern int is_dir_name (char *filename);

/* The default value of INFOPATH. */
#if !defined (DEFAULT_INFOPATH)
#  define DEFAULT_INFOPATH "/usr/local/info:/usr/info:/usr/local/lib/info:/usr/lib/info:/usr/local/gnu/info:/usr/local/gnu/lib/info:/usr/gnu/info:/usr/gnu/lib/info:/opt/gnu/info:/usr/share/info:/usr/share/lib/info:/usr/local/share/info:/usr/local/share/lib/info:/usr/gnu/lib/emacs/info:/usr/local/gnu/lib/emacs/info:/usr/local/lib/emacs/info:/usr/local/emacs/info:."
#endif /* !DEFAULT_INFOPATH */

#if !defined (S_ISREG) && defined (S_IFREG)
#  define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif /* !S_ISREG && S_IFREG */

#if !defined (S_ISDIR) && defined (S_IFDIR)
#  define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif /* !S_ISDIR && S_IFDIR */

#endif /* not INFO_FILESYS_H */
