
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/statvfs.h>

#define e(errn) e_f(__FILE__, __LINE__, (errn))
#define em(errn,msg) do { fprintf(stderr, "%s\n", msg); e(errn); } while(0)

#define BIGVARNAME "BIGTEST"

void printprogress(char *msg, int i, int max);
void cleanup(void);
int does_fs_truncate(void);
void e_f(char *file, int lineno, int n);
int name_max(char *path);
void quit(void);
void rm_rf_dir(int test_nr);
void rm_rf_ppdir(int test_nr);
void start(int test_nr);
void getmem(u32_t *total, u32_t *free, u32_t *cached);

extern int common_test_nr, errct, subtest;
