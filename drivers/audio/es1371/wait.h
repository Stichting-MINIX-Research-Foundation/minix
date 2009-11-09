#ifndef WAIT_H
#define WAIT_H
/* WAIT.H
// General purpose waiting routines

// Function prototypes
*/
int WaitBitb (int paddr, int bitno, int state, long tmout);
int WaitBitw (int paddr, int bitno, int state, long tmout);
int WaitBitd (int paddr, int bitno, int state, long tmout);
int MemWaitw (unsigned int volatile *gaddr, int bitno, int state, long tmout);

#endif
