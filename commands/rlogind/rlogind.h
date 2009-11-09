/*
in.rld.h
*/

#define NMAX	30

#ifndef EXTERN
#define EXTERN extern
#endif

EXTERN char *prog_name;
EXTERN char hostname[256+1];
EXTERN char line[1024];
EXTERN char lusername[NMAX+1], rusername[NMAX+1];
EXTERN char term[64];
EXTERN int authenticated;

/* in.rld.c: */
void fatal(int fd, char *msg, int err);

/* setup.c: */
void authenticate(void);
int do_rlogin(void);
void tcp_urg(int fd, int on);
