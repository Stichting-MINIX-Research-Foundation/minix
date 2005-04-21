/* BIOS definitions. Each BIOS entry has an index that is to be used with the
 * sys_bioscopy() system call. The raw addresses, sizes, and magic numbers 
 * are defined here as well. The values that are defined here were collected 
 * from various kernel files in MINIX 2.0.4.
 *
 * Author: Jorrit N. Herder	
 */

#ifndef _BIOS_H
#define _BIOS_H

/* Memory check (is stopped on reboot). */
#define BIOS_MEM_CHECK		0	/* address to stop memory check */	
#define  ADR_MEM_CHECK 		0x472L 	
#define  LEN_MEM_CHECK  	1L
#define  STOP_MEM_CHECK 	0x1234	/* magic number to stop memory check */

/* Centronics printers. */
#define BIOS_PRN_PORTBASE     	1	/* base of printer ports */
#define  ADR_PRN_PORTBASE 	0x408L	
#define  LEN_PRN_PORTBASE 	2L

/* Hard disk parameter vectors. */
#define BIOS_WINI_PARAMS      	2	/* number of hard disk drives */		
#define  ADR_WINI_PARAMS 	0x475L
#define  LEN_WINI_PARAMS	1L
#define BIOS_WINI_0_PARM_VEC  	3	/* disk 0 parameters */ 
#define  ADR_WINI_0_PARM_VEC	0x41*4L
#define  LEN_WINI_0_PARM_VEC	4L
#define BIOS_WINI_1_PARM_VEC  	4	/* disk 1 parameters */
#define  ADR_WINI_1_PARM_VEC	0x46*4L
#define  LEN_WINI_1_PARM_VEC	4L

/* Video controller (VDU). */
#define	BIOS_VDU_COLUMNS	5 	
#define	 ADR_VDU_COLUMNS 	0x44AL
#define	 LEN_VDU_COLUMNS	2L 
#define BIOS_VDU_CRTBASE 	6
#define  ADR_VDU_CRTBASE	0x463L 
#define  LEN_VDU_CRTBASE 	2L
#define	BIOS_VDU_ROWS 		7
#define	 ADR_VDU_ROWS 		0x484L
#define	 LEN_VDU_ROWS		1L 
#define BIOS_VDU_FONTLINES 	8
#define  ADR_VDU_FONTLINES 	0x485L
#define  LEN_VDU_FONTLINES	2L 

/* Machine ID. */
#define BIOS_MACHINE_ID		9
#define  ADR_MACHINE_ID		0xFFFFEL
#define  LEN_MACHINE_ID		1L
#define  PS_386_MACHINE         0xF8	/* Machine ID byte for PS/2 model 80 */
#define  PC_AT_MACHINE		0xFC	/* PC/AT, PC/XT286, PS/2 models 50/60 */

#endif /* _BIOS_H */
