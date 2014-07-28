/*  getlogin(3)
 *
 *  Author: Terrence W. Holm          Aug. 1988
 */

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "extern.h"



int __getlogin(char *logname, size_t sz)
{
  struct passwd *pw_entry;

  pw_entry = getpwuid(getuid());

  if (pw_entry == (struct passwd *)NULL)
    return 0; 
    
  strncpy(logname, pw_entry->pw_name, sz);
  return sz;
}
