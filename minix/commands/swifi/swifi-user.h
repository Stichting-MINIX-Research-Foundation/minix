#ifndef _SWIFI_USER_H
#define _SWIFI_USER_H


#define TEXT_FAULT	0
#define STACK_FAULT	1
#define HEAP_FAULT	2
#define INIT_FAULT      3
#define NOP_FAULT       4
#define DST_FAULT       5
#define SRC_FAULT       6
#define BRANCH_FAULT    7
#define PTR_FAULT       8
#define FREE_FAULT      9	
#define BCOPY_FAULT     10
#define SYNC_FAULT      11
#define LOOP_FAULT      12
#define MEM_LEAK_FAULT  13
#define INTERFACE_FAULT 14
#define DIRECT_FAULT	15
#define DIRECT_FAULT1	16
#define STATS           17
#define WP_FAULT	19
#define PANIC_FAULT	20
#define WHILE1_FAULT	21
#define DEBUGGER_FAULT	22
#define CPU_RESET_FAULT	23
#define PAGE_REG_DUMP	24
#define COW_FAULT	25
#define IRQ_FAULT       26
#define ALLOC_FAULT     27
#define DISK_TEST		100


#define SWIFI_MAX_FAULTS 1000

typedef struct swifi_result {
  unsigned long address;
  unsigned long old;
  unsigned long new;
} swifi_result_t, *pswifi_result_t;

long 
sys_inject_fault(char * module,
		 unsigned long argFaultType,
		 unsigned long argRandomSeed,
		 unsigned long argNumFaults,
		 pswifi_result_t result_record,
		 unsigned long argInjectFault);
 

#endif /* _SWIFI_USER_H */

