#ifndef _DDEKIT_ATTRIBS_H
#define _DDEKIT_ATTRIBS_H

#ifdef __ACK__


#else

#define DDEKIT_USED        __attribute__((used))
#define DDEKIT_CONSTRUCTOR __attribute__((constructor))


#define DDEKIT_PUBLIC PUBLIC
#define DDEKIT_PRIVATE static 
#endif
#endif
