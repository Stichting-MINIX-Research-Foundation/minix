/* whoami - print the current user name 	Author: Terrence W. Holm */

#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

_PROTOTYPE(int main, (void));

int main()
{
  struct passwd *pw_entry;

  pw_entry = getpwuid(geteuid());
  if (pw_entry == NULL) exit(1);
  puts(pw_entry->pw_name);
  return(0);
}
