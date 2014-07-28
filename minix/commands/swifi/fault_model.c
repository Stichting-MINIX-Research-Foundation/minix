/*
 * fault-model.c -- fault injection code for drivers
 *
 * Copyright (C) 2003 Mike Swift
 * Copyright (c) 1999 Wee Teck Ng
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  No warranty 
 * is attached; * we cannot take responsibility for errors or 
 * fitness for use.
 *
 */


/*
 * Fault injector for testing the usefulness of NOOKS
 * 
 * Adapted from the SWIFI tools used by Wee Teck Ng to evaluate the RIO
 * file cache at the University of Michigan
 * 
 */

/* 
 * This tool can inject faults into modules, whether they are loaded into a 
 * nook or loaded into the kernel (for comparison testing).
 * 
 * There are several classes of faults emulated:
 * - Corruption of text
 *    - corruption
 *    - simulated programming faults
 *         - skip initialization (immediate write to EBP-x)
 *         - remove instruction (replace with NOP)
 *	   - incorrect source/destination (corrupted)
 *         - remove jmp or rep instruction
 *         - change address computation for memory access (not stack)
 *	   - change termination condition for loop (change repeat to repeat 
 *           -while equal, change condition to !condition
	   - remove instructions loading registers from arguments (ebp+x)
 *        
 * - Corruption of stack
 * - Corruption of heap
 * - copy overruns
 * - use after free
 */

#if 0
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include <asm/delay.h>
#include <asm/page.h>
#endif
#include "ddb.h"
#include "db_sym.h"
#include "swifi.h"

#include "extra.h"
#include <assert.h>

#define CRASH_INTERVAL	8192
#define FI_MASK			0xfff
#define P50     0x3fffffff      /* 50% of max rand */
#define P94     0x7851eb84      /* 94% of max rand */
#define NOP		0x90

unsigned long randomSeed=0;		/* random number */
unsigned long injectFault=1;		/* inject fault ? */
unsigned long diskTest=0;	        /* run disk test instead of rio */
unsigned long faultInjected=0;	        /* has fault been injected? */
unsigned long crashInterval=0;	        /* interval between injecting fault */
unsigned long crashCount=0;	        /* number of times fault is injected */
unsigned long faultType;			 
unsigned long numFaults;
char *crashAddr=0;		        /* track current malloc */
int crashToggle=1;
int text_fault(char *mod_name, pswifi_result_t res);
int stack_fault(pswifi_result_t res);
int heap_fault(pswifi_result_t res);
int direct_fault(int fault_address, int fault_content, pswifi_result_t res);
int direct_fault1(int fault_address, int fault_content, pswifi_result_t res);
int while1(void);

int *testVA;

#if 0
#define PDEBUG(fmt, args...) \
do { \
      printk( KERN_ALERT "SWIFI: " fmt, ## args); \
} while (0)
#else
#include <stdio.h>
#define PDEBUG(args) /* (printf args) */
#endif

#define inline

#ifdef CONFIG_SWIFI

#if 0
static inline long
get_mod_name(const char *user_name, char **buf)
{
	unsigned long page;
	long retval;

	page = __get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	retval = strncpy_from_user((char *)page, user_name, PAGE_SIZE);
	if (retval > 0) {
		if (retval < PAGE_SIZE) {
			*buf = (char *)page;
			return retval;
		}
		retval = -ENAMETOOLONG;
	} else if (!retval)
		retval = -EINVAL;

	free_page(page);
	return retval;
}

static inline void
put_mod_name(char *buf)
{
	free_page((unsigned long)buf);
}
#endif

long 
sys_inject_fault(char * module_name,
		 unsigned long argFaultType,
		 unsigned long argRandomSeed,
		 unsigned long argNumFaults,
		 pswifi_result_t result_record,
		 unsigned long argInjectFault)
{   
  int result = 0;
  unsigned long fault_address = 0; 
  unsigned long fault_data = 0 ; 
  char * kern_name = NULL;
#if 0
  struct module * mod = NULL;
  int found = 0;
#endif
  pswifi_result_t res = NULL;

  if (argNumFaults > SWIFI_MAX_FAULTS) {
    result = -E2BIG;
    goto Cleanup;
  }
  res = (pswifi_result_t) malloc((1+argNumFaults) * sizeof(swifi_result_t));
  if (res == NULL) {
    result = -ENOMEM;
    goto Cleanup;
  }
  memset(res, 0, (1 + argNumFaults) * sizeof(swifi_result_t));
  
  /*
  // Capture the name of the module from usermode
  */

#if 0
  result = get_mod_name(module_name, &kern_name);
  if (result < 0) {
    goto Cleanup;
  }
#endif

  kern_name= module_name;




#if 0
    lock_kernel();

    for (mod = module_list; mod ; mod = mod->next) {
      if (strcmp(kern_name, mod->name) == 0) {
	found = 1;
	break;
      }
    }
    unlock_kernel(); 
    if (!found) {
      result = -ENOENT;
      goto Cleanup;
    }
#endif
		
  numFaults = argNumFaults;
  faultType = argFaultType;
  randomSeed = argRandomSeed;
  injectFault = argInjectFault;


  if(faultType>=DISK_TEST) {
    faultType=faultType-DISK_TEST;
    diskTest=1;
  }
  if(faultType==STATS) {
#if 0    
    extern long time_vmp, n_vmp;
    extern long time_pmp, n_pmp;

    PDEBUG("# vm_map_protect=%ld, total cycle=%ld\n", n_vmp, time_vmp);
    PDEBUG("# pmap_protect=%ld, total cycle=%ld\n", n_pmp, time_pmp);
    n_vmp=0; time_vmp=0;
    n_pmp=0; time_pmp=0;
#endif
  } else if (faultType == DIRECT_FAULT) {
    fault_address = numFaults;
    fault_data = randomSeed;
    PDEBUG(("sys inject fault, type %ld, addr=%lx, flip bit%lx\n", 
	   faultType, fault_address, fault_data));
  } else if (faultType == DIRECT_FAULT1) {
    fault_address = numFaults;
    fault_data = randomSeed;
    PDEBUG(("sys inject fault, type %ld, addr=%lx, zero bytes %lx\n", 
	   faultType, fault_address, fault_data));
  } else {
    PDEBUG(("sys inject fault, type %ld, seed=%ld, fault=%ld\n", 
	   faultType, randomSeed, numFaults));
  }
  faultInjected=1;
  
  srandom(randomSeed);
  /* set warm reboot, leave RAM unchanged  
   * 0 : don't inject fault
   * 1 : run POST, wipe out memory
   * 2 : don't test memory
   * 3 : don't change memory (doesn't work)
   * 4 : don't sync registry
   */
  
  /* default number of faults is 5 */
  if(numFaults<=0 || numFaults>100) numFaults=5;
  
  switch(faultType)
    {
    case TEXT_FAULT: 
      result = text_fault(module_name, res); 
      break;
    case STACK_FAULT: 
      result = stack_fault(res); 
      break;
    case HEAP_FAULT: 
      result = heap_fault(res); 
      break;
    case INIT_FAULT:
    case NOP_FAULT: 
    case DST_FAULT: 
    case SRC_FAULT: 
    case BRANCH_FAULT: 
    case PTR_FAULT: 
    case LOOP_FAULT: 
    case INTERFACE_FAULT: 
    case IRQ_FAULT:
      result = text_fault(module_name, res); 
      break;
    case FREE_FAULT: 
    case BCOPY_FAULT: 
    case SYNC_FAULT:
    case ALLOC_FAULT:
      crashInterval=CRASH_INTERVAL; 	/* interval between crash */
      break;
    case MEM_LEAK_FAULT: 
      crashToggle=0;
      crashInterval=CRASH_INTERVAL; 	/* interval between crash */
      break;
    case PANIC_FAULT: 
      panic("testing panic"); 
      result = 0;
      break;
      /*        case WP_FAULT: page_reg_fault(random()); break; */
    case DIRECT_FAULT:
      {
	direct_fault(fault_address, fault_data, res);
	break;
      }
    case DIRECT_FAULT1: 
      {
	result = direct_fault1(fault_address, fault_data, res);
	
	break;
      }
      /*    	case PAGE_REG_DUMP: rio_dump(); break; */
    case WHILE1_FAULT: 
      {
	
	result = while1();
	
	break;
      }
      /* case CPU_RESET_FAULT: cpu_reset(); break; */;
    case COW_FAULT: 
      {
				/* test writing to kernel text. freebsd currently do a COW on a
				 * write to kernel text.
				 */
	unsigned long *addr1, *addr2;
	
	addr1 = (unsigned long *) 0xf0212000;
	addr2 = (unsigned long *) 0xf0212010;
	PDEBUG(("%p=%lx, %p=%lx\n", addr1, *addr1, addr2, *addr2));
	/*
	__asm__ ("movl $0xf0212000, %eax\n\t" \
		 "movl $6, 0(%eax)\n\t" \
		 "movl $6, 4(%eax)\n\t");
	*/
	/* Not implemented on MINIX */
	assert(0);
	addr1 = (unsigned long *) 0xf0212000;
	addr2 = (unsigned long *) 0xf0212010;
	PDEBUG(("after injecting fault\n"));
	PDEBUG(("%p=%lx, %p=%lx\n", addr1, *addr1, addr2, *addr2));
	result = 0;
	break;
      }
    
    case DEBUGGER_FAULT: 
      PDEBUG(("Debugger fault"));
      /*
      __asm__ ("movl %cr4, %ecx\n\t" \
	       "movl $42, %ecx; .byte 0x0f, 0x32\n\t" \
	       "movl $377, %ecx; .byte 0x0f, 0x32\n\t");
      */
      /* Not implemented on MINIX */
      assert(0);
      result = 0;
      break;
    default: PDEBUG(("unknown fault type %ld\n", faultType)); break;
    }
  if (copy_to_user(result_record, res, argNumFaults * sizeof(swifi_result_t))) {
    result = -EFAULT;
  }
 Cleanup:
#if 0
  if (kern_name != NULL) {
    put_mod_name(kern_name);
  }
#endif
  if (res != NULL) {
    free(res);
  }

  return (result);
}

int while1(void)
{
  int i=0;

  PDEBUG(("entering into while 1 loop\n"));
  while(1) { 
    udelay(20000); 
    PDEBUG(("delay %4d secs, cpl=0x%x, ipend=0x%x\n", i+=5, 20, 30)); 
    if(i>(100 * 2500)) 
      break;
  }
  return(0);
}


int direct_fault(int fault_address, int fault_content, pswifi_result_t res)
{   
  unsigned long *addr;
  int flip_bit=0;


  addr = (unsigned long *) (PAGE_OFFSET + fault_address);

  PDEBUG(("%p:0x%lx => ", addr, *addr));
  
  flip_bit = 1 << fault_content;

  res[0].address = (unsigned long) addr;
  res[0].old = *addr;
  res[0].new = (*addr) ^ flip_bit;

  if (injectFault) {
    *addr = (*addr) ^ flip_bit; 
  }
  PDEBUG(("%lx\n", *addr));
  return(0);
}

int direct_fault1(int fault_address, int fault_content, pswifi_result_t res)
{   
  unsigned long *addr, data;


  addr = (unsigned long *) (PAGE_OFFSET + fault_address);
  
  PDEBUG(("%p:%lx => ", addr, *addr));
  
  
  data = *addr;
  if(fault_content==1) {
    data = data & 0xffffff00;
    data = data | 0x00000090;
  } else if(fault_content==2) {
    data = data & 0xffff0000;
    data = data | 0x00009090;
  } else if(fault_content==3) {
    data = data & 0xff000000;
    data = data | 0x00909090;
  } else if(fault_content==4) {
    data = 0x90909090;
  } 
  res[0].address = (unsigned long) addr;
  res[0].old = *addr;
  res[0].new = data;
  if (injectFault) {
    *addr = data;
  }

  PDEBUG(("%lx\n", *addr));
  
    
  return(0);
}




/*
#include <linux/sched.h>
*/

#define MAX_NUM_TASKS 20

struct task_struct *
find_task(void)
{
  struct task_struct * task = NULL, *result = NULL ;
  int i,j;
  i = 1 + (random() % MAX_NUM_TASKS);
  j = i;

  
  do {
#if 0
    read_lock(&tasklist_lock);
#endif
    for_each_task(task) { 
      if (--i == 0) {
	result = task;
	break;
      }
    }
#if 0
    read_unlock(&tasklist_lock);
#endif
  } while ((i > 0) && (i != j));

  return(result);
}

int
stack_fault(pswifi_result_t res)
{   
  unsigned long *addr, size, taddr;
  int flip_bit=0;
  int count=0;
  struct task_struct *task = NULL;

  while(count < numFaults) {
    task = find_task();
    if (task == NULL) {
      return(-1);
    }

    size = (unsigned long) task + TASK_SIZE - task->thread.esp; 

    PDEBUG(("stack range=%lx-%lx\n", 
	   (unsigned long) task->thread.esp, 
	   (unsigned long) task + TASK_SIZE));

    addr = (unsigned long *) ((long) task->thread.esp + 
			      (random()&~0x3)%size);  
    taddr=(unsigned long) addr;
    flip_bit = random() & 0x1f;
    PDEBUG(("%lx:%lx flip bit %d => ", taddr, *addr, flip_bit));
    flip_bit = 1 << flip_bit;
    res[count].address = taddr;
    res[count].old = *addr;
    res[count].new = (*addr) ^ flip_bit;
    if (injectFault) {
      *addr = ((*addr)^flip_bit); 
    }
    PDEBUG(("%lx\n", *addr));
    count++;
  }
  return(0);
}



/*
// Instead of dealing with heaps directly, we look at the area cache of pages 
// and vm pages and find an address there.
*/


int heap_fault(pswifi_result_t res)
{   
#ifdef notdef
  unsigned long *addr, taddr;
  int flip_bit=0;
  int count=0;
  unsigned long flags;
  struct list_head *next;

   addr = (unsigned long *) (map->address + (random()&~0xf)%map->size); 
   
   taddr=(unsigned long) addr;
   flip_bit = random() & 0x1f;
   PDEBUG("heap range=%lx-%lx ", map->address, map->address + map->size);
   PDEBUG("%lx:%lx flip bit %d => ", taddr, *addr, flip_bit);
   flip_bit = 1 << flip_bit;
   res[count].address = taddr;
   res[count].old = *addr;
   res[count].new = (*addr) ^ flip_bit;

   if (injectFault) {
     *addr = ((*addr)^flip_bit); 
   }
   PDEBUG("%lx\n", *addr);
   count++;   
 } while (count < numFaults);
#endif
  return(-1);
  
}


unsigned long	
do_fault_copy_from_user (void *kaddr, const void *udaddr, unsigned long len,
		      unsigned long (* copy_fn) (void *, const void *, unsigned long))
{   
  unsigned int prob, i=0;

  if ( faultInjected && (faultType==BCOPY_FAULT) ) {

    if (++crashCount == crashInterval) {   
      
      crashCount=0;
      prob = random();
      crashInterval = CRASH_INTERVAL + (random() & FI_MASK);
      
      if (prob < P50) {                    /* corrupt 1 QW         */
	i=1; 
      } else if (prob < P94) {               /* corrupt 2 - 1024 QW  */
	i = prob & 0x3fe;
	while(!i) {
	  i = random() & 0x3fe; 
	}
      } else {                            /* corrupt 2-4 pages    */
	i= prob & 0xc00;
	while(!i) {
	  i = random() & 0xc00; 
	}
      }
      PDEBUG(("copyin: %p to %p, len=%ld overrun=%d, Intvl=%ld, inj=%ld\n", 
	     udaddr, kaddr, len, i, crashInterval, faultInjected));
      if (faultInjected++ <numFaults) {
	len += i;
      } else {
	faultInjected = 0;
      }
      i = 1;
    }
    return(copy_fn(kaddr, udaddr, len));
  } else {
    return(copy_fn(kaddr, udaddr, len));
  }
}

unsigned long
do_fault_copy_to_user(void *udaddr, const void *kaddr, unsigned long len,
		   unsigned long (* copy_fn) (void *, 
					      const void *, 
					      unsigned long))
{   
  unsigned int prob, i=0;

  if( faultInjected && (faultType==BCOPY_FAULT) ){
    crashCount++;
    if (crashCount == crashInterval) {
      crashCount=0;
      prob = random();
      crashInterval = CRASH_INTERVAL + (random() & FI_MASK);

      if ( prob < P50) {                    /* corrupt 1 QW         */
	i=1; 
      } else if(prob < P94) {               /* corrupt 2 - 1024 QW  */
	i = prob & 0x3fe;
	while (!i) {
	  i = random() & 0x3fe; 
	}
      } else {
	i = prob & 0xc00;
	while(!i) {
	  i = random() & 0xc00; 
	}
      }
      PDEBUG(("copyout: %p to %p, len=%ld overrun=%d, Intvl=%ld, inj=%ld\n",
	     kaddr, udaddr, len, i, crashInterval, faultInjected));
      if (faultInjected++ <numFaults) {
	len+=i;
      } else  {
	faultInjected = 0;
      }
      i=1;
    }
    return(copy_fn(udaddr, kaddr, len));
  } else 
    return(copy_fn(udaddr, kaddr, len));
}


unsigned long
swifi___generic_copy_from_user (void *kaddr, void *udaddr, unsigned long len)
{
  return(do_fault_copy_from_user(kaddr, 
				 udaddr, 
				 len, 
				 __generic_copy_from_user));
}

unsigned long	
swifi___generic_copy_to_user(void *udaddr, void *kaddr, unsigned long len)
{
  return(do_fault_copy_to_user(udaddr, 
			       kaddr, 
			       len, 
			       __generic_copy_to_user));
}



void *
swifi_memcpy_fn (void *to, void *from, size_t len)
{   
  unsigned int prob, i=0;

  if( faultInjected && (faultType==BCOPY_FAULT) ) {
    crashCount++;
    if (crashCount == crashInterval) {
      crashCount=0;
      prob = random();
      crashInterval = CRASH_INTERVAL + (random() & FI_MASK);

      if (prob < P50) {                    /* corrupt 1 QW         */
               i=1; 
      } else if (prob < P94) {               /* corrupt 2 - 1024 QW  */
	i= prob & 0x3fe;
	while(!i) {
	  i = random() & 0x3fe; 
	}
      } else {                            /* corrupt 2-4 pages    */
	i=prob&0xc00;
	while(!i) {
	  i = random() & 0xc00; 
	}
      }
    
      PDEBUG(("memcpy: %p to %p, len=%d overrun=%d, Intvl=%ld, inj=%ld\n", 
	     from, to, len, i, crashInterval, faultInjected));
      if(faultInjected++ <numFaults) len+=i;
      else faultInjected=0;
      i=1;
    }
    return(memcpy(to, from, len));
  } else 
    return(memcpy(to, from, len));
}


void *
swifi_memmove_fn (void *to, void *from, size_t len)
{   
  unsigned int prob, i=0;

  if( faultInjected && (faultType==BCOPY_FAULT) ) {
    crashCount++;
    if (crashCount == crashInterval) {
      crashCount=0;
      prob = random();
      crashInterval = CRASH_INTERVAL + (random() & FI_MASK);

      if (prob < P50) {                    /* corrupt 1 QW         */
               i=1; 
      } else if (prob < P94) {               /* corrupt 2 - 1024 QW  */
	i= prob & 0x3fe;
	while(!i) {
	  i = random() & 0x3fe; 
	}
      } else {                            /* corrupt 2-4 pages    */
	i=prob&0xc00;
	while(!i) {
	  i = random() & 0xc00; 
	}
      }
    
      PDEBUG(("memmove: %p to %p, len=%d overrun=%d, Intvl=%ld, inj=%ld\n", 
	     from, to, len, i, crashInterval, faultInjected));
      if(faultInjected++ <numFaults) len+=i;
      else faultInjected=0;
      i=1;
    }
    return(memmove(to, from, len));
  } else 
    return(memmove(to, from, len));
}


void *
memmove_fn(void *to, void *from, size_t len)
{
  return(memmove(to, from, len));
}



void *
memcpy_fn(void *to, void *from, size_t len)
{
  return(memcpy(to, from, len));
}




void
do_fault_kfree(void *addr, void (* kfree_fn)(const void *))
{   
  if(addr == crashAddr) {
    crashAddr=0;
  }
  if (faultInjected && (faultType==FREE_FAULT || 
			faultType==MEM_LEAK_FAULT)) {
    crashCount++;
    if(crashCount>=crashInterval) {   
      
      /* alternate between premature freeing and non-free */
      if(crashToggle) {
	if(crashAddr) { 
	  PDEBUG(("malloc : freeing %p prematurely\n", 
		 crashAddr));
	  kfree_fn(crashAddr);
	  kfree_fn(addr);
	  crashAddr=0;
	  crashToggle=0;
	  crashCount=0;
	  crashInterval = CRASH_INTERVAL + (random()&FI_MASK);
	  if (faultInjected++ > numFaults) {
	    faultInjected=0;
	  }
	} 
      } else {
	PDEBUG(("free: don't free %p\n", addr));
	if(faultInjected++ > numFaults) {
	  faultInjected=0;
	}
	if(faultType==FREE_FAULT) {
	  crashToggle=1;
	}
	crashCount=0;
	crashInterval = CRASH_INTERVAL + (random()&FI_MASK);
      }
    }
  } else {
    kfree_fn(addr);
  }
}

#if 0
void
swifi_kfree(const void *addr)
{
  do_fault_kfree((void *) addr, kfree);
}
#endif


void do_vfree(const void * addr)
{
  vfree((void *) addr);
}


void
swifi_vfree(void *addr)
{
  do_fault_kfree(addr, do_vfree);
}




void *
do_fault_kmalloc(size_t size, 
		 int flags,
		 void * (* kmalloc_fn)(size_t size, int flags))
{
  if (faultInjected && (faultType==ALLOC_FAULT)) {
    crashCount++;
    if(crashCount>=crashInterval) {   
      PDEBUG(("kmalloc : returning null\n"));
      crashCount=0;
      crashInterval = CRASH_INTERVAL + (random()&FI_MASK);
      if (faultInjected++ > numFaults) {
	faultInjected=0;
	return(NULL);
      }

    }
  }

  return(kmalloc_fn(size, flags));
}


#if 0
void *
swifi_kmalloc(size_t size, int flags)
{
  return(do_fault_kmalloc(size, flags, kmalloc));
}
#endif



void * do_fault_vmalloc(unsigned long size, 
			int gfp_mask, 
			pgprot_t prot,
			void * (*vmalloc_fn)(unsigned long size, 
					     int gfp_mask, 
					     pgprot_t prot))
{
  if (faultInjected && (faultType==ALLOC_FAULT)) {
    crashCount++;
    if(crashCount>=crashInterval) {   
      PDEBUG(("vmalloc : returning null\n"));
      crashCount=0;
      crashInterval = CRASH_INTERVAL + (random()&FI_MASK);
      if (faultInjected++ > numFaults) {
	faultInjected=0;
	return(NULL);
      }

    }
  }
  return(vmalloc_fn(size, gfp_mask, prot));
}

void *
swifi___vmalloc(unsigned long size, int gfp_mask, pgprot_t prot)
{
  return(do_fault_vmalloc(size, gfp_mask, prot, __vmalloc));
}
 


#if 0
typedef struct section_callback {
  const char * module_name;
  const char * section_name;
  unsigned long sec_start;
  unsigned long sec_end;
} section_callback_t;

static int
text_section_callback(void *token, 
		      const char *modname, 
		      const char *secname,
		      ElfW(Addr) secstart, 
		      ElfW(Addr) secend, 
		      ElfW(Word) secflags)
{
  section_callback_t * info = (section_callback_t *) token;
  
  if ((strcmp(modname, info->module_name) == 0) &&
      (strcmp(secname, info->section_name) == 0)) {
    info->sec_start = secstart;
    info->sec_end = secend;
    return(1);
  }
  return(0);
}
#endif


int text_fault(char *mod_name, pswifi_result_t res)
{   
  unsigned long *addr, text_size, offset, page, taddr;
  unsigned long btext, etext;

  int count, flip_bit=0, len, rc;
  unsigned char *c;
#if 0
  struct module * module;
  section_callback_t info;
#endif

#define MAX_NUM_MODULES 10

  /* inject faults into text space */

  for(count=0; count<numFaults; count++) {
    int i = 1 + (random() % MAX_NUM_MODULES);
    int j = i;
#if 0
    module = mod;
#endif

#if 0
    info.module_name = module->name;
    info.module_name = "<module-name>";
    info.section_name = ".text";

    kallsyms_sections(&info, text_section_callback);
    if (info.sec_start == 0 ) {
      return(-1);
    }
#endif

    load_nlist(mod_name, &btext, &etext);

#if 0
    btext = info.sec_start;
    etext = info.sec_end;
#endif
    text_size = etext - btext;
    
    PDEBUG(("text=%lx-%lx, size=%lx\n", btext, etext, text_size));
    
    addr = (unsigned long *) 
      (btext + ((unsigned long) (random()&~0xf) % text_size)); 
    
    /* now the tricky part */

    taddr=(unsigned long) addr;
    if( faultType==INIT_FAULT || 
	faultType==NOP_FAULT || 
	faultType==DST_FAULT || 
	faultType==SRC_FAULT ||
	faultType==BRANCH_FAULT || 
	faultType==PTR_FAULT || 
	faultType==LOOP_FAULT || 
	faultType==INTERFACE_FAULT ||
	faultType==IRQ_FAULT ) {
      addr = (unsigned long *) find_faulty_instr(taddr, faultType, &len);
      /* do it over again if we can't find the right instruction */
      if(!addr || !len ) {
	i--;
	continue;
      }
    }

printf("len = %d\n", len);

    PDEBUG(("target addr=%lx, instr addr=%p, %lx=>", taddr, addr,
	text_read_ul(addr))); 
      
    offset = (unsigned long) addr&PAGE_MASK;
    page = (unsigned long) addr&~PAGE_MASK;
    
    /* it doesn't matter what we used here to unprotect page,
     * as this routine will not be in production code.
     */
      
    res[count].address = taddr;
    res[count].old = text_read_ul(addr);
    res[count].new = text_read_ul(addr);

    if (faultType==TEXT_FAULT) {

      flip_bit = random() & 0x1f;
      PDEBUG(("flip bit %d => ", flip_bit));
      flip_bit = 1 << flip_bit;

      res[count].new = text_read_ul(addr) ^ flip_bit;
      
      if (injectFault) {
	text_write_ul(addr, text_read_ul(addr)^flip_bit); 
      }

    } else if (faultType==NOP_FAULT || 
	       faultType==INIT_FAULT ||
	       faultType==BRANCH_FAULT || 
	       faultType==INTERFACE_FAULT ||
	       faultType==IRQ_FAULT) {
      c = (unsigned char *) addr;

      for (j = 0; j < len; j++) {
	/* replace these bytes with NOP (*c=NOP) */
	if (j < sizeof(unsigned long)) {
	  ((unsigned char *) &res[count].new)[j] = NOP;
	}
	if (injectFault) {
	  text_write_ub(c, NOP);
	}	

	c++;
      }
    } else if (faultType==DST_FAULT || faultType==SRC_FAULT) {
      /* skip thru the prefix and opcode, and flip bits in following bytes */
      int prefix;
      c=(unsigned char *) addr;
      do {
	switch (text_read_ub(c)) {
	case 0x66: case 0x67: case 0x26: case 0x36:
	case 0x2e: case 0x3e: case 0x64: case 0x65:
	case 0xf0: case 0xf2: case 0xf3:
	  prefix = 1;
	  break;
	default:
	  prefix = 0;
	  break;
	}
	if (prefix) {
	  c++;
	}
      } while (prefix);
      if(text_read_ub(c)>=0xd8 && text_read_ub(c)<=0xdf) {
	/* don't mess with fp instruction, yet.
	 * but there shouldn't be any fp instr in kernel.
	 */
	PDEBUG(("floating point instruction, bailing out\n"));
	i--;
	continue;
      } else if(text_read_ub(c)==0x0f) {
	c++;
      }
      if(text_read_ub(c)==0x0f) {
	c++;
      }
      c++;
      len = len-((long) c - (long) addr);
      if (len == 0)
      {
	printf("tex_fault: len = %d\n", len);
	count--;
	continue;
      }
if (len == 0)
{
	int i;

	printf(
	"text_fault: bad length at address %p, c = %p, fault type %ld\n",
		addr, c, faultType);
	printf("bytes:");
	for (i= 0; i<16; i++)
		printf(" 0x%02x", text_read_ub((char *)addr+i));
	printf("\n");
	abort();
	*(int *)-4 = 0;
}
      flip_bit = random() % (len*8);
      PDEBUG(("flip bit %d (len=%d) => ", flip_bit, len));
      for(j=0; j<len; j++) {
	/* go to the right byte */
	if(flip_bit<8) {
	  flip_bit = 1 << flip_bit;

	  if (j < sizeof(unsigned long)) {
	    ((unsigned char *) &res[count].new)[j] =
		(text_read_ub(c) ^ flip_bit);
	  }


	  if (injectFault) {
	    text_write_ub(c, (text_read_ub(c)^flip_bit));
	  }

	  j=len;
	}
	c++;
	flip_bit = flip_bit-8;
      }
    } else if(faultType==PTR_FAULT) {
      /* 5f) ptr: if instruction has regmodrm byte (i_has_modrm),
       *     flip 1 bit in lower byte (0x0f) or any bit in following
       *     bytes (sib, imm or disp).
       */
      int prefix;
      c=(unsigned char *) addr;
      do {
	switch (text_read_ub(c)) {
	case 0x66: case 0x67: case 0x26: case 0x36:
	case 0x2e: case 0x3e: case 0x64: case 0x65:
	case 0xf0: case 0xf2: case 0xf3:
	  prefix = 1;
	  break;
	default:
	  prefix = 0;
	  break;
	}
	if (prefix) {
	  c++;
	}
      } while (prefix);
      if(text_read_ub(c)>=0xd8 && text_read_ub(c)<=0xdf) {
	/* don't mess with fp instruction, yet */
	PDEBUG(("floating point instruction, bailing out\n"));
	i--;
	continue;
      } else if(text_read_ub(c)==0x0f) {
	c++;
      }
      if(text_read_ub(c)==0x0f) {
	c++;
      }
      c++;
      len = len-((long) c - (long) addr);
      flip_bit = random() % (len*8-4);
      PDEBUG(("flip bit %d (len=%d) => ", flip_bit, len));

      /* mod/rm byte is special */

      if (flip_bit < 4) {
	flip_bit = 1 << flip_bit;

	rc = c - (unsigned char *) addr;
	if (rc < sizeof(unsigned long)) {
	  ((unsigned char *) &res[count].new)[rc] = text_read_ub(c) ^ flip_bit;
	  
	}
	if (injectFault) {
	  text_write_ub(c, text_read_ub(c)^flip_bit);
	}

      }
      c++; 
      flip_bit=flip_bit-4;

      for(j=1; j<len; j++) {
	/* go to the right byte */
	if (flip_bit<8) {
	  flip_bit = 1 << flip_bit;

	  rc = (c - (unsigned char *) addr);
	  if (rc < sizeof(unsigned long)) {
	    ((unsigned char *) &res[count].new)[rc] =
		text_read_ub(c) ^ flip_bit;
	    
	  }
	  if (injectFault) {
	    text_write_ub(c, text_read_ub(c)^flip_bit);
	  }

	  j=len;
	}
	c++;
	flip_bit = flip_bit-8;
      }
    } else if(faultType==LOOP_FAULT) {
      c=(unsigned char *) addr;
      /* replace rep with repe, and vice versa */
	if(text_read_ub(c)==0xf3) {
	  if (j < sizeof(unsigned long)) {
	    ((unsigned char *) &res[count].new)[j] = NOP;
	  }

	  rc = (c - (unsigned char *) addr);
	  if (rc < sizeof(unsigned long)) {
	    ((unsigned char *) &res[count].new)[rc] = 0xf2;
	    
	  }
	  if (injectFault) {
	    text_write_ub(c, 0xf2);
	  }
	} else if(text_read_ub(c)==0xf2) {
	  rc = (c - (unsigned char *) addr);
	  if (rc < sizeof(unsigned long)) {
	    ((unsigned char *) &res[count].new)[rc] = 0xf3;
	    
	  }
	  if (injectFault) {
	    text_write_ub(c, 0xf3);
	  }
	} else if( (text_read_ub(c)&0xf0)==0x70 ) {
	  /* if we've jxx imm8 instruction, 
	   * incl even byte instruction, eg jo (70) to jno (71)
	   * decl odd byte instruction,  eg jnle (7f) to jle (7e)
	   */ 
	  if(text_read_ub(c)%2 == 0) { 
	    rc = (c - (unsigned char *) addr);
	    if (rc < sizeof(unsigned long)) {
	      ((unsigned char *) &res[count].new)[rc] = text_read_ub(c) + 1;
	    
	    }

	    if (injectFault) {
	      text_write_ub(c, text_read_ub(c)+1);
	    }
	  }  else {

	    rc = (c - (unsigned char *) addr);
	    if (rc < sizeof(unsigned long)) {
	      ((unsigned char *) &res[count].new)[rc] = text_read_ub(c) - 1;
	      
	    }

	    if (injectFault) {
	      text_write_ub(c, text_read_ub(c)-1);
	    }
	  }
	} else if(text_read_ub(c)==0x66 || text_read_ub(c)==0x67)	{ 
		/* override prefix */
	  c++;
	} else if(text_read_ub(c++)==0xf && (text_read_ub(c)&0xf0)==0x80 ) {
	  /* if we've jxx imm16/32 instruction, 
	   * incl even byte instruction, eg jo (80) to jno (81)
	   * decl odd byte instruction,  eg jnle (8f) to jle (8e)
	   */ 
	  if(text_read_ub(c)%2 == 0) {
	    rc = (c - (unsigned char *) addr);
	    if (rc < sizeof(unsigned long)) {
	      ((unsigned char *) &res[count].new)[rc] = text_read_ub(c) + 1;
	      
	    }
	    if (injectFault) {
	      text_write_ub(c, text_read_ub(c)+1);
	    }
	  } else {
	    rc = (c - (unsigned char *) addr);
	    if (rc < sizeof(unsigned long)) {
	      ((unsigned char *) &res[count].new)[rc] = text_read_ub(c) -1;
	      
	    }

	    if (injectFault) {
	      text_write_ub(c, text_read_ub(c)-1);
	    }
	  }
	}
      
    }
    PDEBUG(("%lx\n", text_read_ul(addr)));
  }
  return(0);
}


#else /* CONFIG_SWIFI */

long
sys_inject_fault(char * module_name,
		 unsigned long argFaultType,
		 unsigned long argRandomSeed,
		 unsigned long argNumFaults,
		 pswifi_result_t result_record,
		 unsigned long do_inject)
{
  return(0);
}

#endif /* CONFIG_SWIFI */
