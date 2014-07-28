#ifndef ES1371_H
#define ES1371_H
/* best viewed with tabsize=4 */

#include <minix/audio_fw.h>
#include <sys/types.h>
#include <sys/ioc_sound.h>
#include <minix/sound.h>


/* set your vendor and device ID's here */
#define VENDOR_ID				0x1274
#define DEVICE_ID				0x1371
#define DRIVER_NAME				"ES1371"


/* channels or subdevices */
#define DAC1_CHAN				0
#define ADC1_CHAN				1
#define MIXER					2
#define DAC2_CHAN				3


/* PCI command register defines */
#define SERR_EN					0x0100
#define PCI_MASTER				0x0004
#define IO_ACCESS				0x0001


/* Interrupt/Chip Select Control */
#define CHIP_SEL_CTRL			0x00		
#define ADC1_EN					0x0010
#define DAC1_EN					0x0040
#define DAC2_EN					0x0020
#define CCB_INTRM				0x0400	


/* Interrupt/Chip Select Status */
#define INTERRUPT_STATUS		0x04		
#define ADC						0x0001
#define DAC2					0x0002
#define DAC1					0x0004
#define INTR					0x80000000


/* Sample Rate Converter */
#define SAMPLE_RATE_CONV		0x10


/* CODEC Write/Read register */
#define CODEC_WRITE				0x14
#define CODEC_READ				0x14


/* Legacy address */
#define LEGACY					0x18		


/* Memory related defines */
#define MEM_PAGE				0x0c
#define ADC_MEM_PAGE			0x0d
#define DAC_MEM_PAGE			0x0c		/* for DAC1 and DAC2 */

#define MEMORY					0x30
#define ADC_BUFFER_SIZE			0x34
#define DAC1_BUFFER_SIZE		0x34
#define DAC2_BUFFER_SIZE		0X3c
#define ADC_PCI_ADDRESS			0x30
#define DAC1_PCI_ADDRESS		0x30
#define DAC2_PCI_ADDRESS		0x38


/* Serial Interface Control */
#define SERIAL_INTERFACE_CTRL	0x20
#define P1_S_MB					0x0001		/* DAC1 Stereo/Mono bit */
#define P1_S_EB					0x0002		/* DAC1 Sixteen/Eight bit */
#define P2_S_MB					0x0004		/* DAC2 Stereo/Mono bit */
#define P2_S_EB					0x0008		/* DAC2 Sixteen/Eight bit */
#define R1_S_MB					0x0010		/* ADC Stereo/Mono bit */
#define R1_S_EB					0x0020		/* ADC Sixteen/Eight bit */
#define P1_INTR_EN				0x0100
#define P2_INTR_EN				0x0200
#define R1_INT_EN				0x0400
#define P1_PAUSE				0x0800
#define P2_PAUSE				0x1000


#define DAC1_SAMP_CT			0x24
#define DAC1_CURR_SAMP_CT		0x26
#define DAC2_SAMP_CT			0x28
#define DAC2_CURR_SAMP_CT		0x2a
#define ADC_SAMP_CT				0x2c
#define ADC_CURR_SAMP_CT		0x2e


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
#define DEFAULT_NR_OF_BITS		16	       /* Nr. of bits per sample per chan */
#define DEFAULT_SIGNED			0          /* 0 = unsigned, 1 = signed */
#define DEFAULT_STEREO			1     	   /* 0 = mono, 1 = stereo */
#define MAX_RATE				44100      /* Max sample speed in KHz */
#define MIN_RATE				4000       /* Min sample speed in KHz */


typedef struct DEVSTRUCT {
	char*     name;
	u16_t     v_id;							/* vendor id */
	u16_t     d_id;							/* device id */
	u32_t     devind;						/* minix pci device id, for 
											 * pci configuration space */
	u32_t     base;							/* changed to 32 bits */
	char      irq; 
	char      revision;						/* version of the device */
} DEV_STRUCT;

#define SRC_ERR_NOT_BUSY_TIMEOUT            -1       /* SRC not busy */
#define SRC_SUCCESS							0

#endif /* ES1371_H */
