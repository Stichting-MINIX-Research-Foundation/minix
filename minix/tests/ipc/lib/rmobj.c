/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan/
 */

/**********************************************************
 *
 *    OS Testing - Silicon Graphics, Inc.
 *
 *    FUNCTION NAME     : rmobj()
 *
 *    FUNCTION TITLE    : Remove an object
 *
 *    SYNOPSIS:
 *      int rmobj(char *obj, char **errmsg)
 *
 *    AUTHOR            : Kent Rogers
 *
 *    INITIAL RELEASE   : UNICOS 7.0
 *
 *    USER DESCRIPTION
 *      This routine will remove the specified object.  If the specified
 *      object is a directory, it will recursively remove the directory
 *      and everything underneath it.  It assumes that it has privilege
 *      to remove everything that it tries to remove.  If rmobj() encounters
 *      any problems, and errmsg is not NULL, errmsg is set to point to a
 *      string explaining the error.
 *
 *    DETAILED DESCRIPTION
 *      Allocate space for the directory and its contents
 *      Open the directory to get access to what is in it
 *      Loop through the objects in the directory:
 *        If the object is not "." or "..":
 *          Determine the file type by calling lstat()
 *          If the object is not a directory:
 *            Remove the object with unlink()
 *         Else:
 *            Call rmobj(object) to remove the object's contents
 *            Determine the link count on object by calling lstat()
 *            If the link count >= 3:
 *              Remove the directory with unlink()
 *            Else
 *               Remove the directory with rmdir()
 *      Close the directory and free the pointers
 *
 *    RETURN VALUE
 *      If there are any problems, rmobj() will set errmsg (if it was not
 *      NULL) and return -1.  Otherwise it will return 0.
 *
 ************************************************************/
#include <errno.h>         /* for errno */
#include <stdio.h>         /* for NULL */
#include <stdlib.h>        /* for malloc() */
#include <string.h>        /* for string function */
#include <limits.h>        /* for PATH_MAX */
#include <sys/types.h>     /* for opendir(), readdir(), closedir(), stat() */
#include <sys/stat.h>      /* for [l]stat() */
#include <dirent.h>        /* for opendir(), readdir(), closedir() */
#include <unistd.h>        /* for rmdir(), unlink() */
#include "rmobj.h"

#define SYSERR strerror(errno)

int
rmobj(char *obj, char **errmsg)
{
   int           ret_val = 0;       /* return value from this routine */
   DIR           *dir;              /* pointer to a directory */
   struct dirent *dir_ent;          /* pointer to directory entries */
   char          dirobj[PATH_MAX];  /* object inside directory to modify */
   struct stat   statbuf;           /* used to hold stat information */
   static char   err_msg[1024];     /* error message */

   /* Determine the file type */
   if ( lstat(obj, &statbuf) < 0 ) {
      if ( errmsg != NULL ) {
         sprintf(err_msg, "lstat(%s) failed; errno=%d: %s",
                 obj, errno, SYSERR);
         *errmsg = err_msg;
      }
      return -1;
   }

   /* Take appropriate action, depending on the file type */
   if ( (statbuf.st_mode & S_IFMT) == S_IFDIR ) {
      /* object is a directory */

      /* Do NOT perform the request if the directory is "/" */
      if ( !strcmp(obj, "/") ) {
         if ( errmsg != NULL ) {
            sprintf(err_msg, "Cannot remove /");
            *errmsg = err_msg;
         }
         return -1;
      }

      /* Open the directory to get access to what is in it */
      if ( (dir = opendir(obj)) == NULL ) {
         if ( rmdir(obj) != 0 ) {
            if ( errmsg != NULL ) {
               sprintf(err_msg, "rmdir(%s) failed; errno=%d: %s",
                       obj, errno, SYSERR);
               *errmsg = err_msg;
            }
            return -1;
         } else {
            return 0;
         }
      }

      /* Loop through the entries in the directory, removing each one */
      for ( dir_ent = (struct dirent *)readdir(dir);
            dir_ent != NULL;
            dir_ent = (struct dirent *)readdir(dir)) {

         /* Don't remove "." or ".." */
         if ( !strcmp(dir_ent->d_name, ".") || !strcmp(dir_ent->d_name, "..") )
            continue;

         /* Recursively call this routine to remove the current entry */
         sprintf(dirobj, "%s/%s", obj, dir_ent->d_name);
         if ( rmobj(dirobj, errmsg) != 0 )
            ret_val = -1;
      }

      /* Close the directory */
      closedir(dir);

      /* If there were problems removing an entry, don't attempt to
         remove the directory itself */
      if ( ret_val == -1 )
         return -1;

      /* Get the link count, now that all the entries have been removed */
      if ( lstat(obj, &statbuf) < 0 ) {
         if ( errmsg != NULL ) {
            sprintf(err_msg, "lstat(%s) failed; errno=%d: %s",
                    obj, errno, SYSERR);
            *errmsg = err_msg;
         }
         return -1;
      }

      /* Remove the directory itself */
      if ( statbuf.st_nlink >= 3 ) {
         /* The directory is linked; unlink() must be used */
         if ( unlink(obj) < 0 ) {
            if ( errmsg != NULL ) {
               sprintf(err_msg, "unlink(%s) failed; errno=%d: %s",
                       obj, errno, SYSERR);
               *errmsg = err_msg;
            }
            return -1;
         }
      } else {
         /* The directory is not linked; rmdir() can be used */
         if ( rmdir(obj) < 0 ) {
            if ( errmsg != NULL ) {
               sprintf(err_msg, "remove(%s) failed; errno=%d: %s",
                       obj, errno, SYSERR);
               *errmsg = err_msg;
            }
            return -1;
         }
      }
   } else {
      /* object is not a directory; just use unlink() */
      if ( unlink(obj) < 0 ) {
         if ( errmsg != NULL ) {
            sprintf(err_msg, "unlink(%s) failed; errno=%d: %s",
                    obj, errno, SYSERR);
            *errmsg = err_msg;
         }
         return -1;
      }
   }  /* if obj is a directory */

   /*
    * Everything must have went ok.
    */
   return 0;
}  /* rmobj() */
