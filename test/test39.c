
#include <stdio.h>
#include <minix/endpoint.h>
#include <minix/sys_config.h>

int main(int argc, char *argv[])
{
	int g, p;

	printf("Test 39 ");

	for(g = 0; g <= _ENDPOINT_MAX_GENERATION; g++) {
		for(p = -MAX_NR_TASKS; p < _NR_PROCS; p++) {
			endpoint_t e;
			int mg, mp;
			e = _ENDPOINT(g, p);
			mg = _ENDPOINT_G(e);
			mp = _ENDPOINT_P(e);
			if(mg != g || mp != p)  {
				printf("%d != %d || %d != %d\n", mg, g, mp, p);
				return 1;
			}
			if(g == 0 && e != p) {
				printf("%d != %d and g=0\n", e, p);
				return 1;
			}
			if(e == ANY || e == SELF || e == NONE) {
				printf("endpoint (%d,%d) is %d; ANY, SELF or NONE\n",
					g, p, e);
				return 1;
			}
		}
	}

	printf("ok\n");

	return 0;
}

