#ifndef ES1371_H
#define ES1371_H

#include <sys/types.h>
#include "../../drivers.h"
#include "../../libpci/pci.h"
#include <sys/ioc_sound.h>

#define DAC1_CHAN 0
#define ADC1_CHAN 1
#define MIXER 2
#define DAC2_CHAN 3

/* set your vendor and device ID's here */
#define VENDOR_ID 0x1274
#define DEVICE_ID 0x1371

/* Concert97 direct register offset defines */
#define CONC_bDEVCTL_OFF        0x00    /* Device control/enable */
#define CONC_bMISCCTL_OFF       0x01    /* Miscellaneous control */
#define CONC_bGPIO_OFF          0x02    /* General purpose I/O control */
#define CONC_bJOYCTL_OFF        0x03    /* Joystick control (decode) */
#define CONC_bINTSTAT_OFF       0x04    /* Device interrupt status */
#define CONC_bCODECSTAT_OFF     0x05    /* CODEC interface status */
#define CONC_bINTSUMM_OFF       0x07    /* Interrupt summary status */
#define CONC_b4SPKR_OFF         0x07    /* Also 4 speaker config reg */
#define CONC_bSPDIF_ROUTE_OFF   0x07    /* Also S/PDIF route control reg */
#define CONC_bUARTDATA_OFF      0x08    /* UART data R/W - read clears RX int */
#define CONC_bUARTCSTAT_OFF     0x09    /* UART control and status */
#define CONC_bUARTTEST_OFF      0x0a    /* UART test control reg */
#define CONC_bMEMPAGE_OFF       0x0c    /* Memory page select */
#define CONC_dSRCIO_OFF         0x10    /* I/O ctl/stat/data for SRC RAM */
#define CONC_dCODECCTL_OFF      0x14    /* CODEC control - u32_t read/write */
#define CONC_wNMISTAT_OFF       0x18    /* Legacy NMI status */
#define CONC_bNMIENA_OFF        0x1a    /* Legacy NMI enable */
#define CONC_bNMICTL_OFF        0x1b    /* Legacy control */
#define CONC_bSERFMT_OFF        0x20    /* Serial device format */
#define CONC_bSERCTL_OFF        0x21    /* Serial device control */
#define CONC_bSKIPC_OFF         0x22    /* DAC skip count reg */
#define CONC_wSYNIC_OFF         0x24    /* Synth int count in sample frames */
#define CONC_wSYNCIC_OFF        0x26    /* Synth current int count */
#define CONC_wDACIC_OFF         0x28    /* DAC int count in sample frames */
#define CONC_wDACCIC_OFF        0x2a    /* DAC current int count */
#define CONC_wADCIC_OFF         0x2c    /* ADC int count in sample frames */
#define CONC_wADCCIC_OFF        0x2e    /* ADC current int count */
#define CONC_MEMBASE_OFF        0x30    /* Memory window base - 16 byte window */

/* Concert memory page-banked register offset defines */
#define CONC_dSYNPADDR_OFF  0x30    /* Synth host frame PCI phys addr */
#define CONC_wSYNFC_OFF     0x34    /* Synth host frame count in u32_t'S */
#define CONC_wSYNCFC_OFF    0x36    /* Synth host current frame count */
#define CONC_dDACPADDR_OFF  0x38    /* DAC host frame PCI phys addr */
#define CONC_wDACFC_OFF     0x3c    /* DAC host frame count in u32_t'S */
#define CONC_wDACCFC_OFF    0x3e    /* DAC host current frame count */
#define CONC_dADCPADDR_OFF  0x30    /* ADC host frame PCI phys addr */
#define CONC_wADCFC_OFF     0x34    /* ADC host frame count in u32_t'S */
#define CONC_wADCCFC_OFF    0x36    /* ADC host current frame count */

/*  memory page number defines */
#define CONC_SYNRAM_PAGE    0x00    /* Synth host/serial I/F RAM */
#define CONC_DACRAM_PAGE    0x04    /* DAC host/serial I/F RAM */
#define CONC_ADCRAM_PAGE    0x08    /* ADC host/serial I/F RAM */
#define CONC_SYNCTL_PAGE    0x0c    /* Page bank for synth host control */
#define CONC_DACCTL_PAGE    0x0c    /* Page bank for DAC host control */
#define CONC_ADCCTL_PAGE    0x0d    /* Page bank for ADC host control */
#define CONC_FIFO0_PAGE     0x0e    /* page 0 of UART "FIFO" (rx stash) */
#define CONC_FIFO1_PAGE     0x0f    /* page 1 of UART "FIFO" (rx stash) */



/* bits for Interrupt/Chip Select Control Register (offset 0x00)*/
#define DAC1_EN_BIT           bit(6)
#define DAC2_EN_BIT           bit(5)
#define ADC1_EN_BIT           bit(4)
#define SYNC_RES_BIT          bit(14)

/* bits for Interrupt/Chip Select Status Register (offset 0x04)*/
#define DAC1_INT_STATUS_BIT   bit(2)
#define DAC2_INT_STATUS_BIT   bit(1)
#define ADC1_INT_STATUS_BIT   bit(0)

/* some bits for Serial Interface Control Register (CONC_bSERFMT_OFF 20H) */
#define DAC1_STEREO_BIT         bit(0)   /* stereo or mono format */
#define DAC1_16_8_BIT           bit(1)   /* 16 or 8 bit format */
#define DAC2_STEREO_BIT         bit(2)
#define DAC2_16_8_BIT           bit(3)
#define ADC1_STEREO_BIT         bit(4)
#define ADC1_16_8_BIT           bit(5)
#define DAC1_INT_EN_BIT         bit(8)   /* interupt enable bits */
#define DAC2_INT_EN_BIT         bit(9)
#define ADC1_INT_EN_BIT         bit(10)
#define DAC1_PAUSE_BIT          bit(11)
#define DAC2_PAUSE_BIT          bit(12)

/* Some return values */
#define SRC_SUCCESS                 0
#define CONC_SUCCESS                0
                                                /* Timeout waiting for: */
#define SRC_ERR_NOT_BUSY_TIMEOUT            -1       /* SRC not busy */
#define CONC_ERR_NO_PCI_BIOS                -2       
#define CONC_ERR_DEVICE_NOT_FOUND           -3       
#define CONC_ERR_SPDIF_NOT_AVAIL            -4       
#define CONC_ERR_SPDIF_ROUTING_NOT_AVAIL    -5       
#define CONC_ERR_4SPEAKER_NOT_AVAIL         -6       
#define CONC_ERR_ECHO_NOT_AVAIL             -7       

typedef struct {
  u32_t stereo;
  u16_t sample_rate; 
  u32_t nr_of_bits;
  u32_t sign;
  u32_t busy;
  u32_t fragment_size;
} aud_sub_dev_conf_t;

/* Some defaults for the aud_sub_dev_conf_t*/
#define DEFAULT_RATE		    44100      /* Sample rate */
#define DEFAULT_NR_OF_BITS	16	       /* Nr. of bits per sample per channel*/
#define DEFAULT_SIGNED		  0          /* 0 = unsigned, 1 = signed */
#define DEFAULT_STEREO		  1     	   /* 0 = mono, 1 = stereo */
#define MAX_RATE			      44100      /* Max sample speed in KHz */
#define MIN_RATE			      4000       /* Min sample speed in KHz */


typedef struct DEVSTRUCT {
    char*     name;
    u16_t     v_id;    /* vendor id */
    u16_t     d_id;    /* device id */
    u32_t     devind;  /* minix pci device id, for pci configuration space */
    u32_t     base;    /* changed to 32 bits */
    char      irq; 
    char      revision;/* version of the device */
} DEV_STRUCT;

#define bit(n) 1UL << n


#endif /* ES1371_H */
