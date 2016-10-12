/*
**  File:	devio.c		Jun. 11, 2005
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  This file contains the routines for reading/writing
**  from/to the device registers.
*/

#include <minix/drivers.h>
#include <minix/netdriver.h>
#include "dp.h"

#if (USE_IOPL == 0)

static void warning(const char *type, int err)
{

  printf("Warning: eth#0 sys_%s failed (%d)\n", type, err);
}

/*
**  Name:	inb
**  Function:	Reads a byte from specified i/o port.
*/
unsigned int inb(unsigned short port)
{
  u32_t value;
  int rc;

  if ((rc = sys_inb(port, &value)) != OK) warning("inb", rc);
  return value;
}

/*
**  Name:	inw
**  Function:	Reads a word from specified i/o port.
*/
unsigned int inw(unsigned short port)
{
  u32_t value;
  int rc;

  if ((rc = sys_inw(port, &value)) != OK) warning("inw", rc);
  return value;
}

/*
**  Name:	insb
**  Function:	Reads a sequence of bytes from an i/o port.
*/
void insb(unsigned short int port, void *buffer, int count)
{
  int rc;

  if ((rc = sys_insb(port, SELF, buffer, count)) != OK)
	warning("insb", rc);
}

/*
**  Name:	insw
**  Function:	Reads a sequence of words from an i/o port.
*/
void insw(unsigned short int port, void *buffer, int count)
{
  int rc;

  if ((rc = sys_insw(port, SELF, buffer, count)) != OK)
	warning("insw", rc);
}

/*
**  Name:	outb
**  Function:	Writes a byte to specified i/o port.
*/
void outb(unsigned short port, unsigned long value)
{
  int rc;

  if ((rc = sys_outb(port, value)) != OK) warning("outb", rc);
}

/*
**  Name:	outw
**  Function:	Writes a word to specified i/o port.
*/
void outw(unsigned short port, unsigned long value)
{
  int rc;

  if ((rc = sys_outw(port, value)) != OK) warning("outw", rc);
}

/*
**  Name:	outsb
**  Function:	Writes a sequence of bytes to an i/o port.
*/
void outsb(unsigned short port, void *buffer, int count)
{
  int rc;

  if ((rc = sys_outsb(port, SELF, buffer, count)) != OK)
	warning("outsb", rc);
}

#else
#error To be implemented
#endif				/* USE_IOPL */
/**  devio.c  **/
