#ifndef CONFIG_H_
#define CONFIG_H_ 20140308

#ifndef __UNCONST
#define __UNCONST(a)   ((void *)(unsigned long)(const void *)(a))
#endif /* __UNCONST */

#ifndef USE_ARG
#define USE_ARG(x)       /*LINTED*/(void)&(x)
#endif /* USE_ARG */

#endif /* CONFIG_H_ */
