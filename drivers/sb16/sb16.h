#ifndef SB16_H
#define SB16_H

#include "../drivers.h"
#include <sys/ioc_sound.h>
#include <minix/sound.h>


#define SB_TIMEOUT		32000   /* timeout count */

/* IRQ, base address and DMA channels */
#define SB_IRQ		7
#define SB_BASE_ADDR	0x220		/* 0x210, 0x220, 0x230, 0x240,
                                         * 0x250, 0x260, 0x280        
                                         */
#define SB_DMA_8	1		/* 0, 1, 3 */
#define SB_DMA_16	5		/* 5, 6, 7 */
#if _WORD_SIZE == 2
#define DMA_SIZE	8192		/* Dma buffer MUST BE MULTIPLE OF 2 */
#else
#define DMA_SIZE	(64 * 1024)		/* Dma buffer MUST BE MULTIPLE OF 2 */
#endif

/* Some defaults for the DSP */
#define DEFAULT_SPEED		22050      /* Sample rate */
#define DEFAULT_BITS		8	   /* Nr. of bits */
#define DEFAULT_SIGN		0	   /* 0 = unsigned, 1 = signed */
#define DEFAULT_STEREO		0	   /* 0 = mono, 1 = stereo */

/* DMA port addresses */
#define DMA8_ADDR	((SB_DMA_8 & 3) << 1) + 0x00
#define DMA8_COUNT	((SB_DMA_8 & 3) << 1) + 0x01 
#define DMA8_MASK	0x0A
#define DMA8_MODE	0x0B
#define DMA8_CLEAR	0x0C


/* If after this preprocessing stuff DMA8_PAGE is not defined
 * the 8-bit DMA channel specified is not valid
 */
#if SB_DMA_8 == 0
#  define DMA8_PAGE	0x87	
#else 
#  if SB_DMA_8 == 1
#    define DMA8_PAGE	0x83	
#  else 
#    if SB_DMA_8 == 3
#      define DMA8_PAGE	0x82
#    endif
#  endif
#endif

	
#define DMA16_ADDR	((SB_DMA_16 & 3) << 2) + 0xC0
#define DMA16_COUNT	((SB_DMA_16 & 3) << 2) + 0xC2 
#define DMA16_MASK	0xD4
#define DMA16_MODE	0xD6
#define DMA16_CLEAR	0xD8


/* If after this preprocessing stuff DMA16_PAGE is not defined
 * the 16-bit DMA channel specified is not valid
 */
#if SB_DMA_16 == 5
#  define DMA16_PAGE	  0x8B	
#else 
#  if SB_DMA_16 == 6
#    define DMA16_PAGE	  0x89	
#  else 
#    if SB_DMA_16 == 7
#      define DMA16_PAGE  0x8A
#    endif
#  endif
#endif


/* DMA modes */
#define DMA16_AUTO_PLAY		0x58 + (SB_DMA_16 & 3)
#define DMA16_AUTO_REC		0x54 + (SB_DMA_16 & 3)
#define DMA8_AUTO_PLAY		0x58 + SB_DMA_8
#define DMA8_AUTO_REC		0x54 + SB_DMA_8


/* IO ports for soundblaster */
#define DSP_RESET	0x6 + SB_BASE_ADDR
#define DSP_READ	0xA + SB_BASE_ADDR
#define DSP_WRITE	0xC + SB_BASE_ADDR
#define DSP_COMMAND	0xC + SB_BASE_ADDR
#define DSP_STATUS	0xC + SB_BASE_ADDR
#define DSP_DATA_AVL	0xE + SB_BASE_ADDR
#define DSP_DATA16_AVL	0xF + SB_BASE_ADDR
#define MIXER_REG	0x4 + SB_BASE_ADDR
#define MIXER_DATA	0x5 + SB_BASE_ADDR
#define OPL3_LEFT	0x0 + SB_BASE_ADDR
#define OPL3_RIGHT	0x2 + SB_BASE_ADDR
#define OPL3_BOTH	0x8 + SB_BASE_ADDR


/* DSP Commands */
#define DSP_INPUT_RATE		0x42  /* set input sample rate */
#define DSP_OUTPUT_RATE		0x41  /* set output sample rate */
#define DSP_CMD_SPKON		0xD1  /* set speaker on */
#define DSP_CMD_SPKOFF		0xD3  /* set speaker off */
#define DSP_CMD_DMA8HALT	0xD0  /* halt DMA 8-bit operation */  
#define DSP_CMD_DMA8CONT	0xD4  /* continue DMA 8-bit operation */
#define DSP_CMD_DMA16HALT	0xD5  /* halt DMA 16-bit operation */
#define DSP_CMD_DMA16CONT	0xD6  /* continue DMA 16-bit operation */
#define DSP_GET_VERSION		0xE1  /* get version number of DSP */
#define DSP_CMD_8BITAUTO_IN	0xCE  /* 8 bit auto-initialized input */
#define DSP_CMD_8BITAUTO_OUT	0xC6  /* 8 bit auto-initialized output */
#define DSP_CMD_16BITAUTO_IN	0xBE  /* 16 bit auto-initialized input */
#define DSP_CMD_16BITAUTO_OUT	0xB6  /* 16 bit auto-initialized output */
#define DSP_CMD_IRQREQ8		0xF2  /* Interrupt request 8 bit        */
#define DSP_CMD_IRQREQ16	0xF3  /* Interrupt request 16 bit        */


/* DSP Modes */
#define DSP_MODE_MONO_US	0x00  /* Mono unsigned */
#define DSP_MODE_MONO_S		0x10  /* Mono signed */
#define DSP_MODE_STEREO_US	0x20  /* Stereo unsigned */
#define DSP_MODE_STEREO_S	0x30  /* Stereo signed */


/* MIXER commands */
#define MIXER_RESET		0x00  /* Reset */
#define MIXER_DAC_LEVEL		0x04  /* Used for detection only */
#define MIXER_MASTER_LEFT	0x30  /* Master volume left */
#define MIXER_MASTER_RIGHT	0x31  /* Master volume right */
#define MIXER_DAC_LEFT		0x32  /* Dac level left */
#define MIXER_DAC_RIGHT		0x33  /* Dac level right */
#define MIXER_FM_LEFT		0x34  /* Fm level left */
#define MIXER_FM_RIGHT		0x35  /* Fm level right */
#define MIXER_CD_LEFT		0x36  /* Cd audio level left */
#define MIXER_CD_RIGHT		0x37  /* Cd audio level right */
#define MIXER_LINE_LEFT		0x38  /* Line in level left */
#define MIXER_LINE_RIGHT	0x39  /* Line in level right */
#define MIXER_MIC_LEVEL		0x3A  /* Microphone level */
#define MIXER_PC_LEVEL		0x3B  /* Pc speaker level */
#define MIXER_OUTPUT_CTRL	0x3C  /* Output control */
#define MIXER_IN_LEFT		0x3D  /* Input control left */
#define MIXER_IN_RIGHT		0x3E  /* Input control right */
#define MIXER_GAIN_IN_LEFT	0x3F  /* Input gain control left */
#define MIXER_GAIN_IN_RIGHT	0x40  /* Input gain control right */
#define MIXER_GAIN_OUT_LEFT	0x41  /* Output gain control left */
#define MIXER_GAIN_OUT_RIGHT	0x42  /* Output gain control rigth */
#define MIXER_AGC		0x43  /* Automatic gain control */
#define MIXER_TREBLE_LEFT	0x44  /* Treble left */
#define MIXER_TREBLE_RIGHT	0x45  /* Treble right */
#define MIXER_BASS_LEFT		0x46  /* Bass left */
#define MIXER_BASS_RIGHT	0x47  /* Bass right */
#define MIXER_SET_IRQ		0x80  /* Set irq number */
#define MIXER_SET_DMA		0x81  /* Set DMA channels */
#define MIXER_IRQ_STATUS	0x82  /* Irq status */

/* Mixer constants */
#define MIC				0x01  /* Microphone */
#define CD_RIGHT		0x02   
#define CD_LEFT			0x04
#define LINE_RIGHT		0x08
#define LINE_LEFT		0x10
#define FM_RIGHT		0x20
#define FM_LEFT			0x40

/* DSP constants */
#define DMA_NR_OF_BUFFERS		2
#define DSP_MAX_SPEED			44100      /* Max sample speed in KHz */
#define DSP_MIN_SPEED			4000       /* Min sample speed in KHz */
#define DSP_MAX_FRAGMENT_SIZE	DMA_SIZE /  DMA_NR_OF_BUFFERS /* Maximum fragment size */
#define DSP_MIN_FRAGMENT_SIZE	1024 	   /* Minimum fragment size */
#define DSP_NR_OF_BUFFERS		8


/* Number of bytes you can DMA before hitting a 64K boundary: */
#define dma_bytes_left(phys)    \
   ((unsigned) (sizeof(int) == 2 ? 0 : 0x10000) - (unsigned) ((phys) & 0xFFFF))


_PROTOTYPE(int mixer_set, (int reg, int data));
_PROTOTYPE( int sb16_inb, (int port) );
_PROTOTYPE( void sb16_outb, (int port, int value) );


#endif /* SB16_H */
