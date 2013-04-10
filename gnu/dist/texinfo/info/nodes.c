/*	$NetBSD: nodes.c,v 1.7 2008/09/02 08:00:24 christos Exp $	*/

/* nodes.c -- how to get an Info file and node.
   Id: nodes.c,v 1.4 2004/04/11 17:56:46 karl Exp

   Copyright (C) 1993, 1998, 1999, 2000, 2002, 2003, 2004 Free Software
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

   Originally written by Brian Fox (bfox@ai.mit.edu). */

#include "info.h"

#include "nodes.h"
#include "search.h"
#include "filesys.h"
#include "info-utils.h"

#if defined (HANDLE_MAN_PAGES)
#  include "man.h"
#endif /* HANDLE_MAN_PAGES */

static void forget_info_file (char *filename);
static void remember_info_file (FILE_BUFFER *file_buffer);
static void free_file_buffer_tags (FILE_BUFFER *file_buffer);
static void free_info_tag (TAG *tag);
static void get_nodes_of_tags_table (FILE_BUFFER *file_buffer,
    SEARCH_BINDING *buffer_binding);
static void get_nodes_of_info_file (FILE_BUFFER *file_buffer);
static void get_tags_of_indirect_tags_table (FILE_BUFFER *file_buffer,
    SEARCH_BINDING *indirect_binding, SEARCH_BINDING *tags_binding);
static void info_reload_file_buffer_contents (FILE_BUFFER *fb);
static char *adjust_nodestart (NODE *node, int min, int max);
static FILE_BUFFER *info_load_file_internal (char *filename, int get_tags);
static FILE_BUFFER *info_find_file_internal (char *filename, int get_tags);
static NODE *info_node_of_file_buffer_tags (FILE_BUFFER *file_buffer,
    char *nodename);

static long get_node_length (SEARCH_BINDING *binding);

/* Magic number that RMS used to decide how much a tags table pointer could
   be off by.  I feel that it should be much smaller, like 4.  */
#define DEFAULT_INFO_FUDGE 1000

/* Passed to *_internal functions.  INFO_GET_TAGS says to do what is
   neccessary to fill in the nodes or tags arrays in FILE_BUFFER. */
#define INFO_NO_TAGS  0
#define INFO_GET_TAGS 1

/* Global variables.  */

/* When non-zero, this is a string describing the recent file error. */
char *info_recent_file_error = NULL;

/* The list of already loaded nodes. */
FILE_BUFFER **info_loaded_files = NULL;

/* The number of slots currently allocated to LOADED_FILES. */
int info_loaded_files_slots = 0;

/* Public functions for node manipulation.  */

/* Used to build `dir' menu from `localdir' files found in INFOPATH. */
extern void maybe_build_dir_node (char *dirname);

/* Return a pointer to a NODE structure for the Info node (FILENAME)NODENAME.
   If FILENAME is NULL, `dir' is used.
   IF NODENAME is NULL, `Top' is used.
   If the node cannot be found, return NULL. */
NODE *
info_get_node (char *filename, char *nodename)
{
  NODE *node;
  FILE_BUFFER *file_buffer = NULL;

  info_recent_file_error = NULL;
  info_parse_node (nodename, DONT_SKIP_NEWLINES);
  nodename = NULL;

  if (info_parsed_filename)
    filename = info_parsed_filename;

  if (info_parsed_nodename)
    nodename = info_parsed_nodename;

  /* If FILENAME is not specified, it defaults to "dir". */
  if (!filename)
    filename = "dir";

  /* If the file to be looked up is "dir", build the contents from all of
     the "dir"s and "localdir"s found in INFOPATH. */
  if (is_dir_name (filename))
    maybe_build_dir_node (filename);

  /* Find the correct info file, or give up.  */
  file_buffer = info_find_file (filename);
  if (!file_buffer)
    {
      if (filesys_error_number)
        info_recent_file_error =
          filesys_error_string (filename, filesys_error_number);
      return NULL;
    }

  /* Look for the node.  */
  node = info_get_node_of_file_buffer (nodename, file_buffer);

  /* If the node not found was "Top", try again with different case.  */
  if (!node && (nodename == NULL || strcasecmp (nodename, "Top") == 0))
    {
      node = info_get_node_of_file_buffer ("Top", file_buffer);
      if (!node)
        node = info_get_node_of_file_buffer ("top", file_buffer);
      if (!node)
        node = info_get_node_of_file_buffer ("TOP", file_buffer);
    }

  return node;
}

/* Return a pointer to a NODE structure for the Info node NODENAME in
   FILE_BUFFER.  NODENAME can be passed as NULL, in which case the
   nodename of "Top" is used.  If the node cannot be found, return a
   NULL pointer. */
NODE *
info_get_node_of_file_buffer (char *nodename, FILE_BUFFER *file_buffer)
{
  NODE *node = NULL;

  /* If we are unable to find the file, we have to give up.  There isn't
     anything else we can do. */
  if (!file_buffer)
    return NULL;

  /* If the file buffer was gc'ed, reload the contents now. */
  if (!file_buffer->contents)
    info_reload_file_buffer_contents (file_buffer);

  /* If NODENAME is not specified, it defaults to "Top". */
  if (!nodename)
    nodename = "Top";

  /* If the name of the node that we wish to find is exactly "*", then the
     node body is the contents of the entire file.  Create and return such
     a node. */
  if (strcmp (nodename, "*") == 0)
    {
      node = (NODE *)xmalloc (sizeof (NODE));
      node->filename = file_buffer->fullpath;
      node->parent   = NULL;
      node->nodename = xstrdup ("*");
      node->contents = file_buffer->contents;
      node->nodelen = file_buffer->filesize;
      node->flags = 0;
      node->display_pos = 0;
    }
#if defined (HANDLE_MAN_PAGES)
  /* If the file buffer is the magic one associated with manpages, call
     the manpage node finding function instead. */
  else if (file_buffer->flags & N_IsManPage)
    {
        node = get_manpage_node (file_buffer, nodename);
    }
#endif /* HANDLE_MAN_PAGES */
  /* If this is the "main" info file, it might contain a tags table.  Search
     the tags table for an entry which matches the node that we want.  If
     there is a tags table, get the file which contains this node, but don't
     bother building a node list for it. */
  else if (file_buffer->tags)
    {
      node = info_node_of_file_buffer_tags (file_buffer, nodename);
    }

  /* Return the results of our node search. */
  return node;
}

/* Locate the file named by FILENAME, and return the information structure
   describing this file.  The file may appear in our list of loaded files
   already, or it may not.  If it does not already appear, find the file,
   and add it to the list of loaded files.  If the file cannot be found,
   return a NULL FILE_BUFFER *. */
FILE_BUFFER *
info_find_file (char *filename)
{
  return info_find_file_internal (filename, INFO_GET_TAGS);
}

/* Load the info file FILENAME, remembering information about it in a
   file buffer. */
FILE_BUFFER *
info_load_file (char *filename)
{
  return info_load_file_internal (filename, INFO_GET_TAGS);
}


/* Private functions implementation.  */

/* The workhorse for info_find_file ().  Non-zero 2nd argument says to
   try to build a tags table (or otherwise glean the nodes) for this
   file once found.  By default, we build the tags table, but when this
   function is called by info_get_node () when we already have a valid
   tags table describing the nodes, it is unnecessary. */
static FILE_BUFFER *
info_find_file_internal (char *filename, int get_tags)
{
  int i;
  FILE_BUFFER *file_buffer;

  /* First try to find the file in our list of already loaded files. */
  if (info_loaded_files)
    {
      for (i = 0; (file_buffer = info_loaded_files[i]); i++)
        if ((FILENAME_CMP (filename, file_buffer->filename) == 0)
            || (FILENAME_CMP (filename, file_buffer->fullpath) == 0)
            || (!IS_ABSOLUTE (filename)
                && FILENAME_CMP (filename,
                                filename_non_directory (file_buffer->fullpath))
                    == 0))
          {
            struct stat new_info, *old_info;

            /* This file is loaded.  If the filename that we want is
               specifically "dir", then simply return the file buffer. */
            if (is_dir_name (filename_non_directory (filename)))
              return file_buffer;

#if defined (HANDLE_MAN_PAGES)
            /* Do the same for the magic MANPAGE file. */
            if (file_buffer->flags & N_IsManPage)
              return file_buffer;
#endif /* HANDLE_MAN_PAGES */

            /* The file appears to be already loaded, and is not "dir".  Check
               to see if it's changed since the last time it was loaded.  */
            if (stat (file_buffer->fullpath, &new_info) == -1)
              {
                filesys_error_number = errno;
                return NULL;
              }

            old_info = &file_buffer->finfo;

            if (new_info.st_size != old_info->st_size
                || new_info.st_mtime != old_info->st_mtime)
              {
                /* The file has changed.  Forget that we ever had loaded it
                   in the first place. */
                forget_info_file (filename);
                break;
              }
            else
              {
                /* The info file exists, and has not changed since the last
                   time it was loaded.  If the caller requested a nodes list
                   for this file, and there isn't one here, build the nodes
                   for this file_buffer.  In any case, return the file_buffer
                   object. */
                if (!file_buffer->contents)
                  {
                    /* The file's contents have been gc'ed.  Reload it.  */
                    info_reload_file_buffer_contents (file_buffer);
                    if (!file_buffer->contents)
                      return NULL;
                  }

                if (get_tags && !file_buffer->tags)
                  build_tags_and_nodes (file_buffer);

                return file_buffer;
              }
          }
    }

  /* The file wasn't loaded.  Try to load it now. */
#if defined (HANDLE_MAN_PAGES)
  /* If the name of the file that we want is our special file buffer for
     Unix manual pages, then create the file buffer, and return it now. */
  if (strcasecmp (filename, MANPAGE_FILE_BUFFER_NAME) == 0)
    file_buffer = create_manpage_file_buffer ();
  else
#endif /* HANDLE_MAN_PAGES */
    file_buffer = info_load_file_internal (filename, get_tags);

  /* If the file was loaded, remember the name under which it was found. */
  if (file_buffer)
    remember_info_file (file_buffer);

  return file_buffer;
}

/* The workhorse function for info_load_file ().  Non-zero second argument
   says to build a list of tags (or nodes) for this file.  This is the
   default behaviour when info_load_file () is called, but it is not
   necessary when loading a subfile for which we already have tags. */
static FILE_BUFFER *
info_load_file_internal (char *filename, int get_tags)
{
  char *fullpath, *contents;
  long filesize;
  struct stat finfo;
  int retcode, compressed;
  FILE_BUFFER *file_buffer = NULL;

  /* Get the full pathname of this file, as known by the info system.
     That is to say, search along INFOPATH and expand tildes, etc. */
  fullpath = info_find_fullpath (filename);

  /* Did we actually find the file? */
  retcode = stat (fullpath, &finfo);

  /* If the file referenced by the name returned from info_find_fullpath ()
     doesn't exist, then try again with the last part of the filename
     appearing in lowercase. */
  /* This is probably not needed at all on those systems which define
     FILENAME_CMP to be strcasecmp.  But let's do it anyway, lest some
     network redirector supports case sensitivity.  */
  if (retcode < 0)
    {
      char *lowered_name;
      char *tmp_basename;

      lowered_name = xstrdup (filename);
      tmp_basename = filename_non_directory (lowered_name);

      while (*tmp_basename)
        {
          if (isupper (*tmp_basename))
            *tmp_basename = tolower (*tmp_basename);

          tmp_basename++;
        }

      fullpath = info_find_fullpath (lowered_name);

      retcode = stat (fullpath, &finfo);
      free (lowered_name);
    }

  /* If the file wasn't found, give up, returning a NULL pointer. */
  if (retcode < 0)
    {
      filesys_error_number = errno;
      return NULL;
    }

  /* Otherwise, try to load the file. */
  contents = filesys_read_info_file (fullpath, &filesize, &finfo, &compressed);

  if (!contents)
    return NULL;

  /* The file was found, and can be read.  Allocate FILE_BUFFER and fill
     in the various members. */
  file_buffer = make_file_buffer ();
  file_buffer->filename = xstrdup (filename);
  file_buffer->fullpath = xstrdup (fullpath);
  file_buffer->finfo = finfo;
  file_buffer->filesize = filesize;
  file_buffer->contents = contents;
  if (compressed)
    file_buffer->flags |= N_IsCompressed;

  /* If requested, build the tags and nodes for this file buffer. */
  if (get_tags)
    build_tags_and_nodes (file_buffer);

  return file_buffer;
}

/* Grovel FILE_BUFFER->contents finding tags and nodes, and filling in the
   various slots.  This can also be used to rebuild a tag or node table. */
void
build_tags_and_nodes (FILE_BUFFER *file_buffer)
{
  SEARCH_BINDING binding;
  long position;

  free_file_buffer_tags (file_buffer);
  file_buffer->flags &= ~N_HasTagsTable;

  /* See if there is a tags table in this info file. */
  binding.buffer = file_buffer->contents;
  binding.start = file_buffer->filesize;
  binding.end = binding.start - 1000;
  if (binding.end < 0)
    binding.end = 0;
  binding.flags = S_FoldCase;

  position = search_backward (TAGS_TABLE_END_LABEL, &binding);

  /* If there is a tag table, find the start of it, and grovel over it
     extracting tag information. */
  if (position != -1)
    while (1)
      {
        long tags_table_begin, tags_table_end;

        binding.end = position;
        binding.start = binding.end - 5 - strlen (TAGS_TABLE_END_LABEL);
        if (binding.start < 0)
          binding.start = 0;

        position = find_node_separator (&binding);

        /* For this test, (and all others here) failure indicates a bogus
           tags table.  Grovel the file. */
        if (position == -1)
          break;

        /* Remember the end of the tags table. */
        binding.start = position;
        tags_table_end = binding.start;
        binding.end = 0;

        /* Locate the start of the tags table. */
        position = search_backward (TAGS_TABLE_BEG_LABEL, &binding);

        if (position == -1)
          break;

        binding.end = position;
        binding.start = binding.end - 5 - strlen (TAGS_TABLE_BEG_LABEL);
        position = find_node_separator (&binding);

        if (position == -1)
          break;

        /* The file contains a valid tags table.  Fill the FILE_BUFFER's
           tags member. */
        file_buffer->flags |= N_HasTagsTable;
        tags_table_begin = position;

        /* If this isn't an indirect tags table, just remember the nodes
           described locally in this tags table.  Note that binding.end
           is pointing to just after the beginning label. */
        binding.start = binding.end;
        binding.end = file_buffer->filesize;

        if (!looking_at (TAGS_TABLE_IS_INDIRECT_LABEL, &binding))
          {
            binding.start = tags_table_begin;
            binding.end = tags_table_end;
            get_nodes_of_tags_table (file_buffer, &binding);
            return;
          }
        else
          {
            /* This is an indirect tags table.  Build TAGS member. */
            SEARCH_BINDING indirect;

            indirect.start = tags_table_begin;
            indirect.end = 0;
            indirect.buffer = binding.buffer;
            indirect.flags = S_FoldCase;

            position = search_backward (INDIRECT_TAGS_TABLE_LABEL, &indirect);

            if (position == -1)
              {
                /* This file is malformed.  Give up. */
                return;
              }

            indirect.start = position;
            indirect.end = tags_table_begin;
            binding.start = tags_table_begin;
            binding.end = tags_table_end;
            get_tags_of_indirect_tags_table (file_buffer, &indirect, &binding);
            return;
          }
      }

  /* This file doesn't contain any kind of tags table.  Grovel the
     file and build node entries for it. */
  get_nodes_of_info_file (file_buffer);
}

/* Search through FILE_BUFFER->contents building an array of TAG *,
   one entry per each node present in the file.  Store the tags in
   FILE_BUFFER->tags, and the number of allocated slots in
   FILE_BUFFER->tags_slots. */
static void
get_nodes_of_info_file (FILE_BUFFER *file_buffer)
{
  long nodestart;
  int tags_index = 0;
  SEARCH_BINDING binding;

  binding.buffer = file_buffer->contents;
  binding.start = 0;
  binding.end = file_buffer->filesize;
  binding.flags = S_FoldCase;

  while ((nodestart = find_node_separator (&binding)) != -1)
    {
      int start, end;
      char *nodeline;
      TAG *entry;
      int anchor = 0;

      /* Skip past the characters just found. */
      binding.start = nodestart;
      binding.start += skip_node_separator (binding.buffer + binding.start);

      /* Move to the start of the line defining the node. */
      nodeline = binding.buffer + binding.start;

      /* Find "Node:" */
      start = string_in_line (INFO_NODE_LABEL, nodeline);
      /* No Node:.  Maybe it's a Ref:.  */
      if (start == -1)
        {
          start = string_in_line (INFO_REF_LABEL, nodeline);
          if (start != -1)
            anchor = 1;
        }

      /* If not there, this is not the start of a node. */
      if (start == -1)
        continue;

      /* Find the start of the nodename. */
      start += skip_whitespace (nodeline + start);

      /* Find the end of the nodename. */
      end = start +
        skip_node_characters (nodeline + start, DONT_SKIP_NEWLINES);

      /* Okay, we have isolated the node name, and we know where the
         node starts.  Remember this information. */
      entry = xmalloc (sizeof (TAG));
      entry->nodename = xmalloc (1 + (end - start));
      strncpy (entry->nodename, nodeline + start, end - start);
      entry->nodename[end - start] = 0;
      entry->nodestart = nodestart;
      if (anchor)
        entry->nodelen = 0;
      else
        {
          SEARCH_BINDING node_body;

          node_body.buffer = binding.buffer + binding.start;
          node_body.start = 0;
          node_body.end = binding.end - binding.start;
          node_body.flags = S_FoldCase;
          entry->nodelen = get_node_length (&node_body);
        }

      entry->filename = file_buffer->fullpath;

      /* Add this tag to the array of tag structures in this FILE_BUFFER. */
      add_pointer_to_array (entry, tags_index, file_buffer->tags,
                            file_buffer->tags_slots, 100, TAG *);
    }
}

/* Return the length of the node which starts at BINDING. */
static long
get_node_length (SEARCH_BINDING *binding)
{
  int i;
  char *body;

  /* [A node] ends with either a ^_, a ^L, or end of file.  */
  for (i = binding->start, body = binding->buffer; i < binding->end; i++)
    {
      if (body[i] == INFO_FF || body[i] == INFO_COOKIE)
        break;
    }
  return i - binding->start;
}

/* Build and save the array of nodes in FILE_BUFFER by searching through the
   contents of BUFFER_BINDING for a tags table, and groveling the contents. */
static void
get_nodes_of_tags_table (FILE_BUFFER *file_buffer,
    SEARCH_BINDING *buffer_binding)
{
  int name_offset;
  SEARCH_BINDING *tmp_search;
  long position;
  int tags_index = 0;

  tmp_search = copy_binding (buffer_binding);

  /* Find the start of the tags table. */
  position = find_tags_table (tmp_search);

  /* If none, we're all done. */
  if (position == -1)
    return;

  /* Move to one character before the start of the actual table. */
  tmp_search->start = position;
  tmp_search->start += skip_node_separator
    (tmp_search->buffer + tmp_search->start);
  tmp_search->start += strlen (TAGS_TABLE_BEG_LABEL);
  tmp_search->start--;

  /* The tag table consists of lines containing node names and positions.
     Do each line until we find one that doesn't contain a node name. */
  while ((position = search_forward ("\n", tmp_search)) != -1)
    {
      TAG *entry;
      char *nodedef;
      unsigned p;
      int anchor = 0;

      /* Prepare to skip this line. */
      tmp_search->start = position;
      tmp_search->start++;

      /* Skip past informative "(Indirect)" tags table line. */
      if (!tags_index && looking_at (TAGS_TABLE_IS_INDIRECT_LABEL, tmp_search))
        continue;

      /* Find the label preceding the node name. */
      name_offset =
        string_in_line (INFO_NODE_LABEL, tmp_search->buffer + tmp_search->start);

      /* If no node label, maybe it's an anchor.  */
      if (name_offset == -1)
        {
          name_offset = string_in_line (INFO_REF_LABEL,
              tmp_search->buffer + tmp_search->start);
          if (name_offset != -1)
            anchor = 1;
        }

      /* If not there, not a defining line, so we must be out of the
         tags table.  */
      if (name_offset == -1)
        break;

      entry = xmalloc (sizeof (TAG));

      /* Find the beginning of the node definition. */
      tmp_search->start += name_offset;
      nodedef = tmp_search->buffer + tmp_search->start;
      nodedef += skip_whitespace (nodedef);

      /* Move past the node's name in this tag to the TAGSEP character. */
      for (p = 0; nodedef[p] && nodedef[p] != INFO_TAGSEP; p++)
        ;
      if (nodedef[p] != INFO_TAGSEP)
        continue;

      entry->nodename = xmalloc (p + 1);
      strncpy (entry->nodename, nodedef, p);
      entry->nodename[p] = 0;
      p++;
      entry->nodestart = atol (nodedef + p);

      /* If a node, we don't know the length yet, but if it's an
         anchor, the length is 0. */
      entry->nodelen = anchor ? 0 : -1;

      /* The filename of this node is currently known as the same as the
         name of this file. */
      entry->filename = file_buffer->fullpath;

      /* Add this node structure to the array of node structures in this
         FILE_BUFFER. */
      add_pointer_to_array (entry, tags_index, file_buffer->tags,
                            file_buffer->tags_slots, 100, TAG *);
    }
  free (tmp_search);
}

/* A structure used only in `get_tags_of_indirect_tags_table' to hold onto
   an intermediate value. */
typedef struct {
  char *filename;
  long first_byte;
} SUBFILE;

/* Remember in FILE_BUFFER the nodenames, subfilenames, and offsets within the
   subfiles of every node which appears in TAGS_BINDING.  The 2nd argument is
   a binding surrounding the indirect files list. */
static void
get_tags_of_indirect_tags_table (FILE_BUFFER *file_buffer,
    SEARCH_BINDING *indirect_binding, SEARCH_BINDING *tags_binding)
{
  int i;
  SUBFILE **subfiles = NULL;
  int subfiles_index = 0, subfiles_slots = 0;
  TAG *entry;

  /* First get the list of tags from the tags table.  Then lookup the
     associated file in the indirect list for each tag, and update it. */
  get_nodes_of_tags_table (file_buffer, tags_binding);

  /* We have the list of tags in file_buffer->tags.  Get the list of
     subfiles from the indirect table. */
  {
    char *start, *end, *line;
    SUBFILE *subfile;

    start = indirect_binding->buffer + indirect_binding->start;
    end = indirect_binding->buffer + indirect_binding->end;
    line = start;

    while (line < end)
      {
        int colon;

        colon = string_in_line (":", line);

        if (colon == -1)
          break;

        subfile = (SUBFILE *)xmalloc (sizeof (SUBFILE));
        subfile->filename = (char *)xmalloc (colon);
        strncpy (subfile->filename, line, colon - 1);
        subfile->filename[colon - 1] = 0;
        subfile->first_byte = (long) atol (line + colon);

        add_pointer_to_array
          (subfile, subfiles_index, subfiles, subfiles_slots, 10, SUBFILE *);

        while (*line++ != '\n');
      }
  }

  /* If we have successfully built the indirect files table, then
     merge the information in the two tables. */
  if (!subfiles)
    {
      free_file_buffer_tags (file_buffer);
      return;
    }
  else
    {
      int tags_index;
      long header_length;
      SEARCH_BINDING binding;

      /* Find the length of the header of the file containing the indirect
         tags table.  This header appears at the start of every file.  We
         want the absolute position of each node within each subfile, so
         we subtract the start of the containing subfile from the logical
         position of the node, and then add the length of the header in. */
      binding.buffer = file_buffer->contents;
      binding.start = 0;
      binding.end = file_buffer->filesize;
      binding.flags = S_FoldCase;

      header_length = find_node_separator (&binding);
      if (header_length == -1)
        header_length = 0;

      /* Build the file buffer's list of subfiles. */
      {
        char *containing_dir = xstrdup (file_buffer->fullpath);
        char *temp = filename_non_directory (containing_dir);
        int len_containing_dir;

        if (temp > containing_dir)
          {
            if (HAVE_DRIVE (file_buffer->fullpath) &&
                temp == containing_dir + 2)
              {
                /* Avoid converting "d:foo" into "d:/foo" below.  */
                *temp = '.';
                temp += 2;
              }
            temp[-1] = 0;
          }

        len_containing_dir = strlen (containing_dir);

        for (i = 0; subfiles[i]; i++);

        file_buffer->subfiles = (char **) xmalloc ((1 + i) * sizeof (char *));

        for (i = 0; subfiles[i]; i++)
          {
            char *fullpath;

            fullpath = (char *) xmalloc
              (2 + strlen (subfiles[i]->filename) + len_containing_dir);

            sprintf (fullpath, "%s/%s",
                     containing_dir, subfiles[i]->filename);

            file_buffer->subfiles[i] = fullpath;
          }
        file_buffer->subfiles[i] = NULL;
        free (containing_dir);
      }

      /* For each node in the file's tags table, remember the starting
         position. */
      for (tags_index = 0; (entry = file_buffer->tags[tags_index]);
           tags_index++)
        {
          for (i = 0;
               subfiles[i] && entry->nodestart >= subfiles[i]->first_byte;
               i++);

          /* If the Info file containing the indirect tags table is
             malformed, then give up. */
          if (!i)
            {
              /* The Info file containing the indirect tags table is
                 malformed.  Give up. */
              for (i = 0; subfiles[i]; i++)
                {
                  free (subfiles[i]->filename);
                  free (subfiles[i]);
                  free (file_buffer->subfiles[i]);
                }
              file_buffer->subfiles = NULL;
              free_file_buffer_tags (file_buffer);
              return;
            }

          /* SUBFILES[i] is the index of the first subfile whose logical
             first byte is greater than the logical offset of this node's
             starting position.  This means that the subfile directly
             preceding this one is the one containing the node. */

          entry->filename = file_buffer->subfiles[i - 1];
          entry->nodestart -= subfiles[i - 1]->first_byte;
          entry->nodestart += header_length;
        }

      /* We have successfully built the tags table.  Remember that it
         was indirect. */
      file_buffer->flags |= N_TagsIndirect;
    }

  /* Free the structures assigned to SUBFILES.  Free the names as well
     as the structures themselves, then finally, the array. */
  for (i = 0; subfiles[i]; i++)
    {
      free (subfiles[i]->filename);
      free (subfiles[i]);
    }
  free (subfiles);
}


/* Return the node that contains TAG in FILE_BUFFER, else
   (pathologically) NULL.  Called from info_node_of_file_buffer_tags.  */
static NODE *
find_node_of_anchor (FILE_BUFFER *file_buffer, TAG *tag)
{
  int anchor_pos, node_pos;
  TAG *node_tag;
  NODE *node;

  /* Look through the tag list for the anchor.  */
  for (anchor_pos = 0; file_buffer->tags[anchor_pos]; anchor_pos++)
    {
      TAG *t = file_buffer->tags[anchor_pos];
      if (t->nodestart == tag->nodestart)
        break;
    }

  /* Should not happen, because we should always find the anchor.  */
  if (!file_buffer->tags[anchor_pos])
    return NULL;

  /* We've found the anchor.  Look backwards in the tag table for the
     preceding node (we're assuming the tags are given in order),
     skipping over any preceding anchors.  */
  for (node_pos = anchor_pos - 1;
       node_pos >= 0 && file_buffer->tags[node_pos]->nodelen == 0;
       node_pos--)
    ;

  /* An info file with an anchor before any nodes is pathological, but
     it's possible, so don't crash.  */
  if (node_pos < 0)
    return NULL;

  /* We have the tag for the node that contained the anchor tag.  */
  node_tag = file_buffer->tags[node_pos];

  /* Look up the node name in the tag table to get the actual node.
     This is a recursive call, but it can't recurse again, because we
     call it with a real node.  */
  node = info_node_of_file_buffer_tags (file_buffer, node_tag->nodename);

  /* Start displaying the node at the anchor position.  */
  if (node)
    { /* The nodestart for real nodes is three characters before the `F'
         in the `File:' line (a newline, the CTRL-_, and another
         newline).  The nodestart for anchors is the actual position.
         But we offset by only 2, rather than 3, because if an anchor is
         at the beginning of a paragraph, it's nicer for it to end up on
         the beginning of the first line of the paragraph rather than
         the blank line before it.  (makeinfo has no way of knowing that
         a paragraph is going to start, so we can't fix it there.)  */
      node->display_pos = file_buffer->tags[anchor_pos]->nodestart
                          - (node_tag->nodestart + 2);

      /* Otherwise an anchor at the end of a node ends up displaying at
         the end of the last line of the node (way over on the right of
         the screen), which looks wrong.  */
      if (node->display_pos >= (unsigned long) node->nodelen)
        node->display_pos = node->nodelen - 1;

      /* Don't search in the node for the xref text, it's not there.  */
      node->flags |= N_FromAnchor;
    }

  return node;
}


/* Return the node from FILE_BUFFER which matches NODENAME by searching
   the tags table in FILE_BUFFER, or NULL.  */
static NODE *
info_node_of_file_buffer_tags (FILE_BUFFER *file_buffer, char *nodename)
{
  TAG *tag;
  int i;

  /* If no tags at all (possibly a misformatted info file), quit.  */
  if (!file_buffer->tags) {
    return NULL;
  }

  for (i = 0; (tag = file_buffer->tags[i]); i++)
    if (strcmp (nodename, tag->nodename) == 0)
      {
        FILE_BUFFER *subfile = info_find_file_internal (tag->filename,
                                                        INFO_NO_TAGS);
        if (!subfile)
          return NULL;

        if (!subfile->contents)
          {
            info_reload_file_buffer_contents (subfile);
            if (!subfile->contents)
              return NULL;
          }

        /* If we were able to find this file and load it, then return
           the node within it. */
        {
          NODE *node = xmalloc (sizeof (NODE));
          node->filename    = subfile->fullpath;
          node->parent      = NULL;
          node->nodename    = tag->nodename;
          node->contents    = subfile->contents + tag->nodestart;
          node->display_pos = 0;
          node->flags       = 0;

          if (file_buffer->flags & N_HasTagsTable)
            {
              node->flags |= N_HasTagsTable;

              if (file_buffer->flags & N_TagsIndirect)
                {
                  node->flags |= N_TagsIndirect;
                  node->parent = file_buffer->fullpath;
                }
            }

          if (subfile->flags & N_IsCompressed)
            node->flags |= N_IsCompressed;

          /* If TAG->nodelen hasn't been calculated yet, then we aren't
             in a position to trust the entry pointer.  Adjust things so
             that ENTRY->nodestart gets the exact address of the start of
             the node separator which starts this node, and NODE->contents
             gets the address of the line defining this node.  If we cannot
             do that, the node isn't really here. */
          if (tag->nodelen == -1)
            {
              int min, max;
              char *node_sep;
              SEARCH_BINDING node_body;
              char *buff_end;

              min = max = DEFAULT_INFO_FUDGE;

              if (tag->nodestart < DEFAULT_INFO_FUDGE)
                min = tag->nodestart;

              if (DEFAULT_INFO_FUDGE >
                  (subfile->filesize - tag->nodestart))
                max = subfile->filesize - tag->nodestart;

              /* NODE_SEP gets the address of the separator which defines
                 this node, or NULL if the node wasn't found.
                 NODE->contents is side-effected to point to right after
                 the separator. */
              node_sep = adjust_nodestart (node, min, max);
              if (node_sep == NULL)
                {
                  free (node);
                  return NULL;
                }
              /* Readjust tag->nodestart. */
              tag->nodestart = node_sep - subfile->contents;

              /* Calculate the length of the current node. */
              buff_end = subfile->contents + subfile->filesize;

              node_body.buffer = node->contents;
              node_body.start = 0;
              node_body.end = buff_end - node_body.buffer;
              node_body.flags = 0;
              tag->nodelen = get_node_length (&node_body);
              node->nodelen = tag->nodelen;
            }

          else if (tag->nodelen == 0) /* anchor, return containing node */
            {
              free (node);
              node = find_node_of_anchor (file_buffer, tag);
            }

          else
            {
              /* Since we know the length of this node, we have already
                 adjusted tag->nodestart to point to the exact start of
                 it.  Simply skip the node separator. */
              node->contents += skip_node_separator (node->contents);
              node->nodelen = tag->nodelen;
            }

          return node;
        }
      }

  /* There was a tag table for this file, and the node wasn't found.
     Return NULL, since this file doesn't contain the desired node. */
  return NULL;
}

/* Managing file_buffers, nodes, and tags.  */

/* Create a new, empty file buffer. */
FILE_BUFFER *
make_file_buffer (void)
{
  FILE_BUFFER *file_buffer = xmalloc (sizeof (FILE_BUFFER));

  file_buffer->filename = file_buffer->fullpath = NULL;
  file_buffer->contents = NULL;
  file_buffer->tags = NULL;
  file_buffer->subfiles = NULL;
  file_buffer->tags_slots = 0;
  file_buffer->flags = 0;

  return file_buffer;
}

/* Add FILE_BUFFER to our list of already loaded info files. */
static void
remember_info_file (FILE_BUFFER *file_buffer)
{
  int i;

  for (i = 0; info_loaded_files && info_loaded_files[i]; i++)
    ;

  add_pointer_to_array (file_buffer, i, info_loaded_files,
                        info_loaded_files_slots, 10, FILE_BUFFER *);
}

/* Forget the contents, tags table, nodes list, and names of FILENAME. */
static void
forget_info_file (char *filename)
{
  int i;
  FILE_BUFFER *file_buffer;

  if (!info_loaded_files)
    return;

  for (i = 0; (file_buffer = info_loaded_files[i]); i++)
    if (FILENAME_CMP (filename, file_buffer->filename) == 0
        || FILENAME_CMP (filename, file_buffer->fullpath) == 0)
      {
        free (file_buffer->filename);
        free (file_buffer->fullpath);

        if (file_buffer->contents)
          free (file_buffer->contents);

        /* free_file_buffer_tags () also kills the subfiles list, since
           the subfiles list is only of use in conjunction with tags. */
        free_file_buffer_tags (file_buffer);

        /* Move rest of list down.  */
        while (info_loaded_files[i + 1])
          {
            info_loaded_files[i] = info_loaded_files[i + 1];
            i++;
          }
        info_loaded_files[i] = 0;

        break;
      }
}

/* Free the tags (if any) associated with FILE_BUFFER. */
static void
free_file_buffer_tags (FILE_BUFFER *file_buffer)
{
  int i;

  if (file_buffer->tags)
    {
      TAG *tag;

      for (i = 0; (tag = file_buffer->tags[i]); i++)
        free_info_tag (tag);

      free (file_buffer->tags);
      file_buffer->tags = NULL;
      file_buffer->tags_slots = 0;
    }

  if (file_buffer->subfiles)
    {
      for (i = 0; file_buffer->subfiles[i]; i++)
        free (file_buffer->subfiles[i]);

      free (file_buffer->subfiles);
      file_buffer->subfiles = NULL;
    }
}

/* Free the data associated with TAG, as well as TAG itself. */
static void
free_info_tag (TAG *tag)
{
  free (tag->nodename);

  /* We don't free tag->filename, because that filename is part of the
     subfiles list for the containing FILE_BUFFER.  free_info_tags ()
     will free the subfiles when it is appropriate. */

  free (tag);
}

/* Load the contents of FILE_BUFFER->contents.  This function is called
   when a file buffer was loaded, and then in order to conserve memory, the
   file buffer's contents were freed and the pointer was zero'ed.  Note that
   the file was already loaded at least once successfully, so the tags and/or
   nodes members are still correctly filled. */
static void
info_reload_file_buffer_contents (FILE_BUFFER *fb)
{
  int is_compressed;

#if defined (HANDLE_MAN_PAGES)
  /* If this is the magic manpage node, don't try to reload, just give up. */
  if (fb->flags & N_IsManPage)
    return;
#endif

  fb->flags &= ~N_IsCompressed;

  /* Let the filesystem do all the work for us. */
  fb->contents =
    filesys_read_info_file (fb->fullpath, &(fb->filesize), &(fb->finfo),
                            &is_compressed);
  if (is_compressed)
    fb->flags |= N_IsCompressed;
}

/* Return the actual starting memory location of NODE, side-effecting
   NODE->contents.  MIN and MAX are bounds for a search if one is necessary.
   Because of the way that tags are implemented, the physical nodestart may
   not actually be where the tag says it is.  If that is the case, but the
   node was found anyway, set N_UpdateTags in NODE->flags.  If the node is
   found, return non-zero.  NODE->contents is returned positioned right after
   the node separator that precedes this node, while the return value is
   position directly on the separator that precedes this node.  If the node
   could not be found, return a NULL pointer. */
static char *
adjust_nodestart (NODE *node, int min, int max)
{
  long position;
  SEARCH_BINDING node_body;

  /* Define the node body. */
  node_body.buffer = node->contents;
  node_body.start = 0;
  node_body.end = max;
  node_body.flags = 0;

  /* Try the optimal case first.  Who knows?  This file may actually be
     formatted (mostly) correctly. */
  if (node_body.buffer[0] != INFO_COOKIE && min > 2)
    node_body.buffer -= 3;

  position = find_node_separator (&node_body);

  /* If we found a node start, then check it out. */
  if (position != -1)
    {
      int sep_len;

      sep_len = skip_node_separator (node->contents);

      /* If we managed to skip a node separator, then check for this node
         being the right one. */
      if (sep_len != 0)
        {
          char *nodedef, *nodestart;
          int offset;

          nodestart = node_body.buffer + position + sep_len;
          nodedef = nodestart;
          offset = string_in_line (INFO_NODE_LABEL, nodedef);

          if (offset != -1)
            {
              nodedef += offset;
              nodedef += skip_whitespace (nodedef);
              offset = skip_node_characters (nodedef, DONT_SKIP_NEWLINES);
              if (((unsigned int) offset == strlen (node->nodename)) &&
                  (strncmp (node->nodename, nodedef, offset) == 0))
                {
                  node->contents = nodestart;
                  return node_body.buffer + position;
                }
            }
        }
    }

  /* Oh well, I guess we have to try to find it in a larger area. */
  node_body.buffer = node->contents - min;
  node_body.start = 0;
  node_body.end = min + max;
  node_body.flags = 0;

  position = find_node_in_binding (node->nodename, &node_body);

  /* If the node couldn't be found, we lose big. */
  if (position == -1)
    return NULL;

  /* Otherwise, the node was found, but the tags table could need updating
     (if we used a tag to get here, that is).  Set the flag in NODE->flags. */
  node->contents = node_body.buffer + position;
  node->contents += skip_node_separator (node->contents);
  if (node->flags & N_HasTagsTable)
    node->flags |= N_UpdateTags;
  return node_body.buffer + position;
}
