/*	$NetBSD: info-utils.h,v 1.1.1.6 2008/09/02 07:49:40 christos Exp $	*/

/* info-utils.h -- Exported functions and variables from info-utils.c.
   Id: info-utils.h,v 1.4 2004/04/11 17:56:45 karl Exp   

   Copyright (C) 1993, 1996, 1998, 2002, 2003, 2004 Free Software
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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#ifndef INFO_UTILS_H
#define INFO_UTILS_H

#include "nodes.h"
#include "window.h"
#include "search.h"

/* Structure which describes a node reference, such as a menu entry or
   cross reference.  Arrays of such references can be built by calling
   info_menus_of_node () or info_xrefs_of_node (). */
typedef struct {
  char *label;          /* User Label. */
  char *filename;       /* File where this node can be found. */
  char *nodename;       /* Name of the node. */
  int start, end;       /* Offsets within the containing node of LABEL. */
  int line_number;      /* Specific line number a menu item points to.  */
} REFERENCE;

/* When non-zero, various display and input functions handle ISO Latin
   character sets correctly. */
extern int ISO_Latin_p;

/* Variable which holds the most recent filename parsed as a result of
   calling info_parse_xxx (). */
extern char *info_parsed_filename;

/* Variable which holds the most recent nodename parsed as a result of
   calling info_parse_xxx (). */
extern char *info_parsed_nodename;

/* Parse the filename and nodename out of STRING.  If STRING doesn't
   contain a filename (i.e., it is NOT (FILENAME)NODENAME) then set
   INFO_PARSED_FILENAME to NULL.  If second argument NEWLINES_OKAY is
   non-zero, it says to allow the nodename specification to cross a
   newline boundary (i.e., only `,', `.', or `TAB' can end the spec). */
void info_parse_node (char *string, int newlines_okay);

/* Return a NULL terminated array of REFERENCE * which represents the menu
   found in NODE.  If there is no menu in NODE, just return a NULL pointer. */
extern REFERENCE **info_menu_of_node (NODE *node);

/* Return a NULL terminated array of REFERENCE * which represents the cross
   refrences found in NODE.  If there are no cross references in NODE, just
   return a NULL pointer. */
extern REFERENCE **info_xrefs_of_node (NODE *node);

/* Glean cross references from BINDING->buffer + BINDING->start until
   BINDING->end.  Return an array of REFERENCE * that represents each
   cross reference in this range. */
extern REFERENCE **info_xrefs (SEARCH_BINDING *binding);

/* Get the entry associated with LABEL in REFERENCES.  Return a pointer to
   the reference if found, or NULL. */
extern REFERENCE *info_get_labeled_reference (char *label,
    REFERENCE **references);

/* Glean menu entries from BINDING->buffer + BINDING->start until we
   have looked at the entire contents of BINDING.  Return an array
   of REFERENCE * that represents each menu item in this range. */
extern REFERENCE **info_menu_items (SEARCH_BINDING *binding);

/* A utility function for concatenating REFERENCE **.  Returns a new
   REFERENCE ** which is the concatenation of REF1 and REF2.  The REF1
   and REF2 arrays are freed, but their contents are not. */
REFERENCE **info_concatenate_references (REFERENCE **ref1, REFERENCE **ref2);

/* Copy an existing reference into new memory.  */
extern REFERENCE *info_copy_reference (REFERENCE *src);

/* Free the data associated with REFERENCES. */
extern void info_free_references (REFERENCE **references);

/* Search for sequences of whitespace or newlines in STRING, replacing
   all such sequences with just a single space.  Remove whitespace from
   start and end of string. */
void canonicalize_whitespace (char *string);

/* Return a pointer to a string which is the printed representation
   of CHARACTER if it were printed at HPOS. */
extern char *printed_representation (unsigned char character, int hpos);

/* Return a pointer to the part of PATHNAME that simply defines the file. */
extern char *filename_non_directory (char *pathname);

/* Return non-zero if NODE is one especially created by Info. */
extern int internal_info_node_p (NODE *node);

/* Make NODE appear to be one especially created by Info, and give it NAME. */
extern void name_internal_node (NODE *node, char *name);

/* Return the window displaying NAME, the name of an internally created
   Info window. */
extern WINDOW *get_internal_info_window (char *name);

/* Return a window displaying the node NODE. */
extern WINDOW *get_window_of_node (NODE *node);

/* Return the node addressed by LABEL in NODE (usually one of "Prev:",
   "Next:", "Up:", "File:", or "Node:".  After a call to this function,
   the globals `info_parsed_nodename' and `info_parsed_filename' contain
   the information. */
extern void info_parse_label (char *label, NODE *node);

#define info_file_label_of_node(n) info_parse_label (INFO_FILE_LABEL, n)
#define info_next_label_of_node(n) info_parse_label (INFO_NEXT_LABEL, n)
#define info_up_label_of_node(n)   info_parse_label (INFO_UP_LABEL, n)
#define info_prev_label_of_node(n) \
  do { \
    info_parse_label (INFO_PREV_LABEL, n); \
    if (!info_parsed_nodename && !info_parsed_filename) \
      info_parse_label (INFO_ALTPREV_LABEL, n); \
  } while (0)

#endif /* not INFO_UTILS_H */
