
#include <stdio.h>
#include <minix/endpoint.h>

int main(int argc, char *argv[])
{
	int g, p;

	printf("Test 41 ");

	for(g = 0; g <= _ENDPOINT_MAX_GENERATION; g++) {
		for(p = -NR_TASKS; p <= _ENDPOINT_MAX_PROC; p++) {
			int e, mg, mp;
			e = _ENDPOINT(g, p);
			mg = _ENDPOINT_G(e);
			mp = _ENDPOINT_P(e);
			if(mg != g || mp != p)  {
				printf("%d != %d || %d != %d\n", mg, g, mp, p);
				return 1;
			}
			if(e == ANY || e == SELF || e == NONE) {
				printf("endpoint is %d; ANY, SELF or NONE\n",
					e);
			}
		}
	}

	printf("ok\n");

	return 0;
}

