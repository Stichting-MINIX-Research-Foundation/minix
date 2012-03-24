
#include <stdio.h>
#include <minix/endpoint.h>
#include <minix/sys_config.h>
#define MAX_ERROR 1
#include "common.c"

void test39a(void);

int main(int argc, char *argv[])
{
  start(39);
  test39a();
  quit();
  return(-1);	/* Unreachable */
}

void test39a()
{
  int g, p;

  subtest = 1;

  for (g = 0; g <= _ENDPOINT_MAX_GENERATION; g++) {
	for (p = -MAX_NR_TASKS; p < _NR_PROCS; p++) {
		endpoint_t ept;
		int mg, mp;
		ept = _ENDPOINT(g, p);
		mg = _ENDPOINT_G(ept);
		mp = _ENDPOINT_P(ept);
		if (mg != g || mp != p) e(1);
		if (g == 0 && ept != p) e(2);
		if (ept == ANY || ept == SELF || ept == NONE) e(3);
	}
  }
}
