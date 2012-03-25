/*
 * Changes:
 *   Mar 07, 2010:  Created  (Cristiano Giuffrida)
 */

#include "inc.h"

/* A single error entry. */
struct errentry {
    int errnum;
    char* errstr;
};

/* Initialization errors. */
static struct errentry init_errlist[] = {
  { ENOSYS,     "service does not support the requested initialization type"  }
};
static const int init_nerr = sizeof(init_errlist) / sizeof(init_errlist[0]);

/* Live update errors. */
static struct errentry lu_errlist[] = {
  { ENOSYS,     "service does not support live update"                        },
  { EINVAL,     "service does not support the required state"                 },
  { EBUSY,      "service is not able to prepare for the update now"           },
  { EGENERIC,   "generic error occurred while preparing for the update"       }
};
static const int lu_nerr = sizeof(lu_errlist) / sizeof(lu_errlist[0]);

/*===========================================================================*
 *				  rs_strerror				     *
 *===========================================================================*/
static char * rs_strerror(int errnum, struct errentry *errlist, const int nerr)
{
  int i;

  for(i=0; i < nerr; i++) {
      if(errnum == errlist[i].errnum)
          return errlist[i].errstr;
  }

  return strerror(-errnum);
}

/*===========================================================================*
 *				  init_strerror				     *
 *===========================================================================*/
char * init_strerror(int errnum)
{
  return rs_strerror(errnum, init_errlist, init_nerr);
}

/*===========================================================================*
 *				   lu_strerror				     *
 *===========================================================================*/
char * lu_strerror(int errnum)
{
  return rs_strerror(errnum, lu_errlist, lu_nerr);
}
