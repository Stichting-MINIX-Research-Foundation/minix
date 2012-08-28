#ifndef _COMPAT_MACHINE_JMP_BUF_H
#define _COMPAT_MACHINE_JMP_BUF_H

/* This file is strictly dependant on the libc's
 * setjmp/longjmp code! Keep it in sync! */

/* This is used only by libddekit's src/thread.c. 
 * Being incredibly fragile (not to mention hardly
 * portable, it would be a good idea to replace 
 * that code. */

#define JB_PC 148
#define JB_SP 144
#define JB_BP 136

#endif /* _COMPAT_MACHINE_JMP_BUF_H */
