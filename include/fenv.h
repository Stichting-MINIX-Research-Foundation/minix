#ifndef _FENV_H
#define _FENV_H

#include <stdint.h>

#define FE_TONEAREST  1
#define FE_DOWNWARD   2
#define FE_UPWARD     3
#define FE_TOWARDZERO 4

typedef struct 
{
	u16_t cw;
	u16_t sw;
} fenv_t;

int feholdexcept(fenv_t *envp);
int fegetround(void);
int fesetround(int round);

#endif
