/*	$NetBSD: printf.c,v 1.1.1.1 2004/07/12 23:27:15 wiz Exp $	*/

/* Formatted output to strings, using POSIX/XSI format strings with positions.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2003.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef __GNUC__
# define alloca __builtin_alloca
# define HAVE_ALLOCA 1
#else
# ifdef _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# else
#  if defined HAVE_ALLOCA_H || defined _LIBC
#   include <alloca.h>
#  else
#   ifdef _AIX
 #pragma alloca
#   else
#    ifndef alloca
char *alloca ();
#    endif
#   endif
#  endif
# endif
#endif

#include <stdio.h>

#if !HAVE_POSIX_PRINTF

#include <stdlib.h>
#include <string.h>

/* When building a DLL, we must export some functions.  Note that because
   the functions are only defined for binary backward compatibility, we
   don't need to use __declspec(dllimport) in any case.  */
#if defined _MSC_VER && BUILDING_DLL
# define DLL_EXPORTED __declspec(dllexport)
#else
# define DLL_EXPORTED
#endif

#define STATIC static

/* Define auxiliary functions declared in "printf-args.h".  */
#include "printf-args.c"

/* Define auxiliary functions declared in "printf-parse.h".  */
#include "printf-parse.c"

/* Define functions declared in "vasnprintf.h".  */
#define vasnprintf libintl_vasnprintf
#include "vasnprintf.c"
#if 0 /* not needed */
#define asnprintf libintl_asnprintf
#include "asnprintf.c"
#endif

DLL_EXPORTED
int
libintl_vfprintf (FILE *stream, const char *format, va_list args)
{
  if (strchr (format, '$') == NULL)
    return vfprintf (stream, format, args);
  else
    {
      size_t length;
      char *result = libintl_vasnprintf (NULL, &length, format, args);
      int retval = -1;
      if (result != NULL)
	{
	  if (fwrite (result, 1, length, stream) == length)
	    retval = length;
	  free (result);
	}
      return retval;
    }
}

DLL_EXPORTED
int
libintl_fprintf (FILE *stream, const char *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = libintl_vfprintf (stream, format, args);
  va_end (args);
  return retval;
}

DLL_EXPORTED
int
libintl_vprintf (const char *format, va_list args)
{
  return libintl_vfprintf (stdout, format, args);
}

DLL_EXPORTED
int
libintl_printf (const char *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = libintl_vprintf (format, args);
  va_end (args);
  return retval;
}

DLL_EXPORTED
int
libintl_vsprintf (char *resultbuf, const char *format, va_list args)
{
  if (strchr (format, '$') == NULL)
    return vsprintf (resultbuf, format, args);
  else
    {
      size_t length = (size_t) ~0 / (4 * sizeof (char));
      char *result = libintl_vasnprintf (resultbuf, &length, format, args);
      if (result != resultbuf)
	{
	  free (result);
	  return -1;
	}
      else
	return length;
    }
}

DLL_EXPORTED
int
libintl_sprintf (char *resultbuf, const char *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = libintl_vsprintf (resultbuf, format, args);
  va_end (args);
  return retval;
}

#if HAVE_SNPRINTF

# if HAVE_DECL__SNPRINTF
   /* Windows.  */
#  define system_vsnprintf _vsnprintf
# else
   /* Unix.  */
#  define system_vsnprintf vsnprintf
# endif

DLL_EXPORTED
int
libintl_vsnprintf (char *resultbuf, size_t length, const char *format, va_list args)
{
  if (strchr (format, '$') == NULL)
    return system_vsnprintf (resultbuf, length, format, args);
  else
    {
      size_t maxlength = length;
      char *result = libintl_vasnprintf (resultbuf, &length, format, args);
      if (result != resultbuf)
	{
	  if (maxlength > 0)
	    {
	      if (length < maxlength)
		abort ();
	      memcpy (resultbuf, result, maxlength - 1);
	      resultbuf[maxlength - 1] = '\0';
	    }
	  free (result);
	  return -1;
	}
      else
	return length;
    }
}

DLL_EXPORTED
int
libintl_snprintf (char *resultbuf, size_t length, const char *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = libintl_vsnprintf (resultbuf, length, format, args);
  va_end (args);
  return retval;
}

#endif

#if HAVE_ASPRINTF

DLL_EXPORTED
int
libintl_vasprintf (char **resultp, const char *format, va_list args)
{
  size_t length;
  char *result = libintl_vasnprintf (NULL, &length, format, args);
  if (result == NULL)
    return -1;
  *resultp = result;
  return length;
}

DLL_EXPORTED
int
libintl_asprintf (char **resultp, const char *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = libintl_vasprintf (resultp, format, args);
  va_end (args);
  return retval;
}

#endif

#if HAVE_FWPRINTF

#include <wchar.h>

#define WIDE_CHAR_VERSION 1

/* Define auxiliary functions declared in "wprintf-parse.h".  */
#include "printf-parse.c"

/* Define functions declared in "vasnprintf.h".  */
#define vasnwprintf libintl_vasnwprintf
#include "vasnprintf.c"
#if 0 /* not needed */
#define asnwprintf libintl_asnwprintf
#include "asnprintf.c"
#endif

# if HAVE_DECL__SNWPRINTF
   /* Windows.  */
#  define system_vswprintf _vsnwprintf
# else
   /* Unix.  */
#  define system_vswprintf vswprintf
# endif

DLL_EXPORTED
int
libintl_vfwprintf (FILE *stream, const wchar_t *format, va_list args)
{
  if (wcschr (format, '$') == NULL)
    return vfwprintf (stream, format, args);
  else
    {
      size_t length;
      wchar_t *result = libintl_vasnwprintf (NULL, &length, format, args);
      int retval = -1;
      if (result != NULL)
	{
	  size_t i;
	  for (i = 0; i < length; i++)
	    if (fputwc (result[i], stream) == WEOF)
	      break;
	  if (i == length)
	    retval = length;
	  free (result);
	}
      return retval;
    }
}

DLL_EXPORTED
int
libintl_fwprintf (FILE *stream, const wchar_t *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = libintl_vfwprintf (stream, format, args);
  va_end (args);
  return retval;
}

DLL_EXPORTED
int
libintl_vwprintf (const wchar_t *format, va_list args)
{
  return libintl_vfwprintf (stdout, format, args);
}

DLL_EXPORTED
int
libintl_wprintf (const wchar_t *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = libintl_vwprintf (format, args);
  va_end (args);
  return retval;
}

DLL_EXPORTED
int
libintl_vswprintf (wchar_t *resultbuf, size_t length, const wchar_t *format, va_list args)
{
  if (wcschr (format, '$') == NULL)
    return system_vswprintf (resultbuf, length, format, args);
  else
    {
      size_t maxlength = length;
      wchar_t *result = libintl_vasnwprintf (resultbuf, &length, format, args);
      if (result != resultbuf)
	{
	  if (maxlength > 0)
	    {
	      if (length < maxlength)
		abort ();
	      memcpy (resultbuf, result, (maxlength - 1) * sizeof (wchar_t));
	      resultbuf[maxlength - 1] = 0;
	    }
	  free (result);
	  return -1;
	}
      else
	return length;
    }
}

DLL_EXPORTED
int
libintl_swprintf (wchar_t *resultbuf, size_t length, const wchar_t *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = libintl_vswprintf (resultbuf, length, format, args);
  va_end (args);
  return retval;
}

#endif

#endif
