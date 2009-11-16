
#ifndef _MINIX_CPUFEATURE_H
#define _MINIX_CPUFEATURE_H 1

#define _CPUF_I386_PSE 		1	/* Page Size Extension */
#define _CPUF_I386_PGE 		2	/* Page Global Enable */
#define _CPUF_I386_APIC_ON_CHIP	3	/* APIC is present on the chip */
#define _CPUF_I386_TSC		4	/* Timestamp counter present */

_PROTOTYPE(int _cpufeature, (int featureno));

#endif
