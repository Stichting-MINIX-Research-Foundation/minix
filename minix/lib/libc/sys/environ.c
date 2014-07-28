/*
 * environ.c - define the variable environ
 */
/* $Header$ */
/*
 * This file defines the variable environ and initializes it with a magic
 * value.  The C run-time start-off routine tests whether the variable
 * environ is initialized with this value.  If it is not, it is assumed
 * that it is defined by the user.  Only two bytes are tested, since we
 * don't know the endian-ness and alignment restrictions of the machine.
 * This means that the low-order two-bytes should be equal to the
 * high-order two-bytes on machines with four-byte pointers.  In fact, all
 * the bytes in the pointer are the same, just in case.
 */

char **environ = (char **) 0x53535353;
