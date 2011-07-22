#ifndef __minix
#define DEFAULT_CC		"cc"
#else
#define DEFAULT_CC		"clang"
#endif

char *findcc(const char *);
