/* Physical memory layout on IBM compatible PCs. Only the major, fixed memory 
 * areas are detailed here. Known addresses of the BIOS data area are defined
 * in <ibm/bios.h>. The map upper memory area (UMA) is only roughly defined 
 * since the UMA sections may vary in size and locus. 
 *
 * Created: March 2005, Jorrit N. Herder
 */

/* I/O-mapped peripherals. I/O addresses are different from memory addresses 
 * due to the I/O signal on the ISA bus. Individual I/O ports are defined by 
 * the drivers that use them or looked up with help of the BIOS. 
 */
#define IO_MEMORY_BEGIN             0x0000
#define IO_MEMORY_END               0xFFFF  

  
/* Physical memory layout. Design decisions made for the earliest PCs, caused
 * memory to be broken broken into the following four basic pieces:
 *  - Conventional or base memory: first 640 KB (incl. BIOS data, see below);
 *    The top of conventional memory is often used by the BIOS to store data.
 *  - Upper Memory Area (UMA): upper 384 KB of the first megabyte of memory;
 *  - High Memory Area (HMA): ~ first 64 KB of the second megabyte of memory;
 *  - Extended Memory: all the memory above first megabyte of memory.
 * The high memory area overlaps with the first 64 KB of extended memory, but
 * is different from the rest of extended memory because it can be accessed 
 * when the processor is in real mode. 
 */
#define BASE_MEM_BEGIN            0x000000
#define BASE_MEM_TOP		  0x090000	
#define BASE_MEM_END              0x09FFFF

#define UPPER_MEM_BEGIN           0x0A0000
#define UPPER_MEM_END             0x0FFFFF

#define HIGH_MEM_BEGIN            0x100000
#define HIGH_MEM_END              0x10FFEF

#define EXTENDED_MEM_BEGIN        0x100000
#define EXTENDED_MEM_END    ((unsigned) -1)
  

/* The logical memory map of the first 1.5 MB is as follows (hexadecimals): 
 *
 * offset [size]  (id) = memory usage
 * ------------------------------------------------------------------------
 * 000000 [00400] (I) = Real-Mode Interrupt Vector Table (1024 B)
 * 000400 [00100] (B) = BIOS Data Area (256 B)
 * 000800 [00066] (W) = 80286 Loadall workspace
 * 010000 [10000] (c) = Real-Mode Compatibility Segment (64 KB)
 * 020000 [70000] (.) = Program-accessible memory (free)
 * 090000 [10000] (E) = BIOS Extension
 * 0A0000 [10000] (G) = Graphics Mode Video RAM
 * 0B0000 [08000] (M) = Monochrome Text Mode Video RAM
 * 0B8000 [08000] (C) = Color Text Mode Video RAM
 * 0C0000 [08000] (V) = Video ROM BIOS (would be "a" in PS/2)
 * 0C8000 [18000] (a) = Adapter ROM + special-purpose RAM (free UMA space)
 * 0E0000 [10000] (r) = PS/2 Motherboard ROM BIOS (free UMA in non-PS/2)
 * 0F0000 [06000] (R) = Motherboard ROM BIOS
 * 0F6000 [08000] (b) = IBM Cassette BASIC ROM ("R" in IBM compatibles)
 * 0FD000 [02000] (R) = Motherboard ROM BIOS
 * 100000 [.....] (.) = Extended memory, program-accessible (free) 
 * 100000 [0FFEF] (h) = High Memory Area (HMA)
 *
 * 
 * Conventional (Base) Memory:
 *
 *       : [~~~~~16 KB~~~~][~~~~~16 KB~~~~][~~~~~16 KB~~~~][~~~~~16 KB~~~~]
 *       : 0---1---2---3---4---5---6---7---8---9---A---B---C---D---E---F---
 * 000000: IBW.............................................................
 * 010000: cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
 * 020000: ................................................................
 * 030000: ................................................................
 * 040000: ................................................................
 * 050000: ................................................................
 * 060000: ................................................................
 * 070000: ................................................................
 * 080000: ................................................................
 * 090000: EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
 * 
 * Upper Memory Area (UMA):
 *
 *       : 0---1---2---3---4---5---6---7---8---9---A---B---C---D---E---F---
 * 0A0000: GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG
 * 0B0000: MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
 * 0C0000: VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
 * 0D0000: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
 * 0E0000: rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr
 * 0F0000: RRRRRRRRRRRRRRRRRRRRRRRRbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbRRRRRRRR
 *
 * Extended Memory:
 * 
 *       : 0---1---2---3---4---5---6---7---8---9---A---B---C---D---E---F---
 * 100000: hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh.
 * 110000: ................................................................
 * 120000: ................................................................
 * 130000: ................................................................
 * 140000: ................................................................
 * 150000: ................................................................
 * 160000: ................................................................
 * 170000: ................................................................
 *
 * Source: The logical memory map was partly taken from the book "Upgrading 
 *         & Repairing PCs Eight Edition", Macmillan Computer Publishing.
 */ 

 
/* The bottom part of conventional or base memory is occupied by BIOS data. 
 * The BIOS memory can be distinguished in two parts:
 * o The first the first 1024 bytes of addressable memory contains the BIOS 
 *   real-mode interrupt vector table (IVT). The table is used to access BIOS
 *   hardware services in real-mode by loading a interrupt vector and issuing 
 *   an INT instruction. Some vectors contain BIOS data that can be retrieved 
 *   directly and are useful in protected-mode as well. 
 * o The BIOS data area is located directly above the interrupt vectors. It
 *   comprises 256 bytes of memory. These data are used by the device drivers
 *   to retrieve hardware details, such as I/O ports to be used. 
 */  
#define BIOS_MEM_BEGIN             0x00000      /* all BIOS memory */
#define BIOS_MEM_END               0x004FF
#define   BIOS_IVT_BEGIN           0x00000      /* BIOS interrupt vectors */
#define   BIOS_IVT_END             0x003FF
#define   BIOS_DATA_BEGIN          0x00400      /* BIOS data area */
#define   BIOS_DATA_END            0x004FF

/* The base memory is followed by 384 KB reserved memory located at the top of
 * the first MB of physical memory. This memory is known as the upper memory 
 * area (UMA). It is used for memory-mapped peripherals, such as video RAM, 
 * adapter BIOS (adapter ROM and special purpose RAM), and the motherboard 
 * BIOS (I/O system, Power-On Self Test, bootstrap loader). The upper memory
 * can roughly be distinguished in three parts:
 * 
 * o The first 128K of the upper memory area (A0000-BFFFF) is reserved for use 
 *   by memory-mapped video adapters. Hence, it is also called Video RAM. The
 *   display driver can directly write to this memory and request the hardware
 *   to show the data on the screen.
 */ 
#define UMA_VIDEO_RAM_BEGIN        0xA0000      /* video RAM */
#define UMA_VIDEO_RAM_END          0xBFFFF
#define   UMA_GRAPHICS_RAM_BEGIN   0xA0000      /* graphics RAM */
#define   UMA_GRAPHICS_RAM_END     0xAFFFF
#define   UMA_MONO_TEXT_BEGIN      0xB0000      /* monochrome text */
#define   UMA_MONO_TEXT_END        0xB7FFF
#define   UMA_COLOR_TEXT_BEGIN     0xB8000      /* color text */
#define   UMA_COLOR_TEXT_END       0xBFFFF

/* o The next 128K (the memory range C0000-DFFFF) is reserved for the adapter 
 *   BIOS that resides in the ROM on some adapter boards. Most VGA-compatible 
 *   video adapters use the first 32 KB of this area for their on-board BIOS. 
 *   The rest can be used by any other adapters. The IDE controller often 
 *   occupies the second 32 KB. 
 */
#define UMA_ADAPTER_BIOS_BEGIN     0xC0000      /* adapter BIOS */
#define UMA_ADAPTER_BIOS_END       0xDFFFF
#define   UMA_VIDEO_BIOS_BEGIN     0xC0000      /* video adapter */
#define   UMA_VIDEO_BIOS_END       0xC7FFF
#define   UMA_IDE_HD_BIOS_BEGIN    0xC8000      /* IDE hard disk */
#define   UMA_IDE_HD_BIOS_END      0xCBFFF

/* o The last 128K of the upper memory area (E0000-FFFFF) is reserved for 
 *   motherboard BIOS (Basic I/O System). The POST (Power-On Self Test) and 
 *   bootstrap loader also reside in  this space. The memory falls apart in 
 *   two areas: Plug & Play BIOS data and the system BIOS data. 
 */ 
#define UMA_MB_BIOS_BEGIN          0xE0000      /* motherboard BIOS */
#define UMA_MB_BIOS_END            0xFFFFF
#define   UMA_PNP_ESCD_BIOS_BEGIN  0xE0000      /* PnP extended data */
#define   UMA_PNP_ESCD_BIOS_END    0xEFFFF
#define   UMA_SYSTEM_BIOS_BEGIN    0xF0000      /* system BIOS */
#define   UMA_SYSTEM_BIOS_END      0xFFFFF

 
