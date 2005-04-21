/*  getlogin(3)
 *
 *  Author: Terrence W. Holm          Aug. 1988
 */

#include <lib.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#ifndef  L_cuserid
#define  L_cuserid   9
#endif

char *getlogin()
{
  PRIVATE char userid[L_cuserid];
  struct passwd *pw_entry;

  pw_entry = getpwuid(getuid());

  if (pw_entry == (struct passwd *)NULL) return((char *)NULL);

  strcpy(userid, pw_entry->pw_name);

  return(userid);
}
