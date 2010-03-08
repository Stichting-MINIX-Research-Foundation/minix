/* Definitions of several known BIOS addresses. The addresses listed here 
 * are found in three memory areas that have been defined in <ibm/memory.h>.
 *  - the BIOS interrupt vectors
 *  - the BIOS data area
 *  - the motherboard BIOS memory
 * 
 * Created: March 2005, Jorrit N. Herder	
 */

#ifndef _BIOS_H
#define _BIOS_H

/* PART I --
 * The BIOS interrupt vector table (IVT) area (1024 B as of address 0x0000). 
 * Although this area holds 256 interrupt vectors (with jump addresses), some 
 * vectors actually contain important BIOS data. Some addresses are below. 
 */
#define BIOS_EQUIP_CHECK_ADDR      0x0044 
#define BIOS_EQUIP_CHECK_SIZE      4L

#define BIOS_VIDEO_PARAMS_ADDR     0x0074        
#define BIOS_VIDEO_PARAMS_SIZE     4L

#define BIOS_FLOP_PARAMS_ADDR      0x0078     
#define BIOS_FLOP_PARAMS_SIZE      4L
 
#define BIOS_HD0_PARAMS_ADDR       0x0104 /* disk 0 parameters */
#define BIOS_HD0_PARAMS_SIZE       4L

#define BIOS_HD1_PARAMS_ADDR       0x0118 /* disk 1 parameters */
#define BIOS_HD1_PARAMS_SIZE       4L

/* PART I -- 
 * Addresses in the BIOS data area (256 B as of address 0x0400). The addresses 
 * listed below are the most important ones, and the ones that are currently 
 * used. Other addresses may be defined below when new features are added. 
 */

/* Serial ports (COM1-COM4). */
#define COM1_IO_PORT_ADDR       0x400   /* COM1 port address */
#define COM1_IO_PORT_SIZE       2L    
#define COM2_IO_PORT_ADDR       0x402   /* COM2 port address */
#define COM2_IO_PORT_SIZE       2L    
#define COM3_IO_PORT_ADDR       0x404   /* COM3 port address */
#define COM3_IO_PORT_SIZE       2L    
#define COM4_IO_PORT_ADDR       0x406   /* COM4 port address */
#define COM4_IO_PORT_SIZE       2L    
        
/* Parallel ports (LPT1-LPT4). */
#define LPT1_IO_PORT_ADDR       0x408   /* LPT1 port address */
#define LPT1_IO_PORT_SIZE       2L    
#define LPT2_IO_PORT_ADDR       0x40A   /* LPT2 port address */
#define LPT2_IO_PORT_SIZE       2L    
#define LPT3_IO_PORT_ADDR       0x40C   /* LPT3 port address */
#define LPT3_IO_PORT_SIZE       2L    
#define LPT4_IO_PORT_ADDR       0x40E   /* LPT4 port (except on PS/2) */
#define LPT4_IO_PORT_SIZE       2L    
        
/* Video controller (VDU). */
#define VDU_SCREEN_COLS_ADDR    0x44A   /* VDU nr of screen columns */
#define VDU_SCREEN_COLS_SIZE    2L  

/* Base I/O port address for active 6845 CRT controller. */
#define VDU_CRT_BASE_ADDR       0x463   /* 3B4h = mono, 3D4h = color */
#define VDU_CRT_BASE_SIZE       2L

/* Soft reset flags to control shutdown. */
#define SOFT_RESET_FLAG_ADDR    0x472   /* soft reset flag on Ctl-Alt-Del */
#define SOFT_RESET_FLAG_SIZE    2L  
#define   STOP_MEM_CHECK        0x1234  /* bypass memory tests & CRT init */
#define   PRESERVE_MEMORY       0x4321  /* preserve memory */
#define   SYSTEM_SUSPEND        0x5678  /* system suspend */
#define   MANUFACTURER_TEST     0x9ABC  /* manufacturer test */
#define   CONVERTIBLE_POST      0xABCD  /* convertible POST loop */
                            /* ... many other values are used during POST */

/* Hard disk parameters. (Also see BIOS interrupt vector table above.) */
#define NR_HD_DRIVES_ADDR       0x475  /* number of hard disk drives */ 
#define NR_HD_DRIVES_SIZE       1L

/* Parallel ports (LPT1-LPT4) timeout values. */
#define LPT1_TIMEOUT_ADDR       0x478   /* time-out value for LPT1 */
#define LPT1_TIMEOUT_SIZE       1L  
#define LPT2_TIMEOUT_ADDR       0x479   /* time-out value for LPT2 */
#define LPT2_TIMEOUT_SIZE       1L  
#define LPT3_TIMEOUT_ADDR       0x47A   /* time-out value for LPT3 */
#define LPT3_TIMEOUT_SIZE       1L  
#define LPT4_TIMEOUT_ADDR       0x47B   /* time-out for LPT4 (except PS/2) */
#define LPT4_TIMEOUT_SIZE       1L  

/* Serial ports (COM1-COM4) timeout values. */
#define COM1_TIMEOUT_ADDR       0x47C   /* time-out value for COM1 */
#define COM1_TIMEOUT_SIZE       1L  
#define COM2_TIMEOUT_ADDR       0x47D   /* time-out value for COM2 */
#define COM2_TIMEOUT_SIZE       1L  
#define COM3_TIMEOUT_ADDR       0x47E   /* time-out value for COM3 */
#define COM3_TIMEOUT_SIZE       1L  
#define COM4_TIMEOUT_ADDR       0x47F   /* time-out value for COM4 */
#define COM4_TIMEOUT_SIZE       1L  

/* Video controller (VDU). */
#define VDU_SCREEN_ROWS_ADDR    0x484   /* screen rows (less 1, EGA+)*/
#define VDU_SCREEN_ROWS_SIZE    1L  
#define VDU_FONTLINES_ADDR      0x485   /* point height of char matrix */
#define VDU_FONTLINES_SIZE      2L 

/* Video controller (VDU). */
#define VDU_VIDEO_MODE_ADDR     0x49A   /* current video mode */
#define VDU_VIDEO_MODE_SIZE     1L  

/* PART III --
 * The motherboard BIOS memory contains some known values that are currently 
 * in use. Other sections in the upper memory area (UMA) addresses vary in 
 * size and locus and are not further defined here. A rough map is given in 
 * <ibm/memory.h>. 
 */

/* Machine ID (we're interested in PS/2 and AT models). */
#define MACHINE_ID_ADDR         0xFFFFE /* BIOS machine ID byte */
#define MACHINE_ID_SIZE         1L
#define   PS_386_MACHINE        0xF8    /* ID byte for PS/2 modela 70/80 */
#define   PC_AT_MACHINE         0xFC    /* PC/AT, PC/XT286, PS/2 models 50/60 */

#endif /* _BIOS_H */

