/*
 * Headers for rd.c
 */

#include "out.h"

extern void rd_string(char *addr, long len);
extern void rd_name(struct outname name[], unsigned int cnt);
extern int rd_fdopen(int fd);
extern void rd_ohead(register struct outhead head[]);
