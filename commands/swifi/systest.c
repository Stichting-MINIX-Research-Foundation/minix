/*
 * systest.c -- Test code for nooks system calls
 *
 * Copyright (C) 2002 Mike Swift
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  
 * No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#if 0
#include <asm/unistd.h>
#endif
#include <string.h>
#include <errno.h>

#define swifi_inject_fault sys_inject_fault

#include "swifi-user.h"
#include "extra.h"


#if 0
_syscall6(long, swifi_inject_fault, 
	  char *, module_name,
	  unsigned long, faultType,
	  unsigned long, randSeed,
	  unsigned long, numFaults,
	  void *, result,
	  unsigned long, do_inject);
#endif

int
main(int argc, char * argv[])
{
  char * module_name = NULL;
  int i;
  long result = 0;
  unsigned int cmd = 0;
  unsigned long arg = 0;
  unsigned long seed = 157;
  swifi_result_t * res = NULL;

  if (argc < 2) {
    goto Usage;
  }

  for (i = 1; i < argc; i++ ) {
    if (strcmp(argv[i], "-f") == 0) {
      if (argc <= i+5) {
	goto Usage;
      }
      module_name = victim_exe = argv[++i];
      sscanf(argv[++i],"%u", &victim_pid);
      sscanf(argv[++i],"%u", &cmd);
      sscanf(argv[++i],"%lu", &arg);
      sscanf(argv[++i],"%lu", &seed);
    } else {
      printf("Unknown command %s\n", argv[i]);
      goto Usage;
    }
  }

  res = malloc(arg * sizeof(swifi_result_t));
  if (res == NULL) {
    printf("Out of memory\n");
    goto Cleanup;
  }

  memset(res, 0, sizeof(res));

  /*
  // Find out where the faults will be injected
  */
  
  result = swifi_inject_fault(module_name, 
			      cmd,         /* fault type */
			      seed,           /* random seed */
			      arg,         /* numFaults */
			      res,
			      0);  /* don't inject now */
  
  for (i = 0; (i < arg) && (res[i].address != 0) ; i++) {
    printf("Changed 0x%lx from 0x%lx to 0x%lx\n",
	   res[i].address,
	   res[i].old,
	   res[i].new);
  }
  
  /*
  // do the injection
  */
  

  result = swifi_inject_fault(module_name, 
			      cmd,         /* fault type */
			      seed,           /* random seed */
			      arg,         /* numFaults */
			      res,
			      1);  /* do inject now */

  printf("swifi_inject_fault returned %ld (%d)\n", result,errno);



 Cleanup:
  if (res != NULL) {
    free(res);
  }
  return(0);

 Usage:
  printf("Usage: %s -f module_name pid fault-type fault-count seed\n", argv[0]);
  goto Cleanup;
}


