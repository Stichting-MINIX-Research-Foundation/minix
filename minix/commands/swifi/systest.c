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
#include <string.h>
#include <errno.h>

#include "swifi.h"
#include "extra.h"

void
usage(char *name)
{
  printf("Usage: %s -f module_name pid fault-type fault-count seed\n", name);

  exit(EXIT_FAILURE);
}

int
main(int argc, char * argv[])
{
  char * module_name = NULL;
  int i;
  unsigned int cmd = 0;
  unsigned long arg = 0;
  unsigned long seed = 157;

  if (argc < 2) {
    usage(argv[0]);
  }

  for (i = 1; i < argc; i++ ) {
    if (strcmp(argv[i], "-f") == 0) {
      if (argc <= i+5) {
        usage(argv[0]);
      }
      module_name = victim_exe = argv[++i];
      sscanf(argv[++i],"%u", &victim_pid);
      sscanf(argv[++i],"%u", &cmd);
      sscanf(argv[++i],"%lu", &arg);
      sscanf(argv[++i],"%lu", &seed);
    } else {
      printf("Unknown command %s\n", argv[i]);
      usage(argv[0]);
    }
  }

  /* Do the injection. */
  swifi_inject_fault(module_name,
		     cmd,          /* fault type */
		     seed,         /* random seed */
		     arg);         /* numFaults */

  return EXIT_SUCCESS;
}
