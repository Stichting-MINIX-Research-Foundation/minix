#ifndef _SWIFI_H
#define _SWIFI_H

#include <stdlib.h>

#define TEXT_FAULT	0
#define INIT_FAULT      3
#define NOP_FAULT       4
#define DST_FAULT       5
#define SRC_FAULT       6
#define BRANCH_FAULT    7
#define PTR_FAULT       8
#define LOOP_FAULT      12
#define INTERFACE_FAULT 14
#define IRQ_FAULT       26
#define STOP_FAULT      50
#define RANDOM_FAULT    99

void
swifi_inject_fault(char * module,
		 unsigned long faultType,
		 unsigned long randomSeed,
		 unsigned long numFaults);

#endif /* _SWIFI_H */
