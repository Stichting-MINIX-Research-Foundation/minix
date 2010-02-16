/*  cuserid(3)
 *
 *  Author: Terrence W. Holm          Sept. 1987
 */

#include <lib.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#ifndef  L_cuserid
#define  L_cuserid   9
#endif

char *cuserid(user_name)
char *user_name;
{
  PRIVATE char userid[L_cuserid];
  struct passwd *pw_entry;

  if (user_name == (char *)NULL) user_name = userid;

  pw_entry = getpwuid(geteuid());

  if (pw_entry == (struct passwd *)NULL) {
	*user_name = '\0';
	return((char *)NULL);
  }
  strcpy(user_name, pw_entry->pw_name);

  return(user_name);
}
