#ifndef _RMOBJ_H_
#define _RMOBJ_H_

/*
 * rmobj() - Remove the specified object.  If the specified object is a
 *           directory, recursively remove everything inside of it.  If
 *           there are any problems, set errmsg (if it is not NULL) and
 *           return -1.  Otherwise return 0.
 */
int rmobj( char *object , char **errmesg );

#endif
