#ifndef AC97_H
#define AC97_H


#include "es1371.h"
#include "wait.h" 
#include "pci_helper.h"
#include "sample_rate_converter.h"


/*
  This function initializes the AC97 to a default mode.
*/
int AC97_init( DEV_STRUCT * pCC );

int AC97_get_set_volume(struct volume_level *level, int flag);



/* This is a main memory cache copy of the codec's ac97 configuration 
   registers. See Intel's Audio Codec 97 standard (rev2.3) for info. */
   
typedef struct ac97_struct {
  u16_t Reset;                    /* 0x00 */
  u16_t MasterVolume;             /* 0x02 */
  u16_t AUXOutVolume;             /* 0x04 */
  u16_t MonoVolume;               /* 0x06 */
  u16_t MasterTone;               /* 0x08 */
  u16_t PCBeepVolume;             /* 0x0A */
  u16_t PhoneVolume;              /* 0x0C */
  u16_t MicVolume;                /* 0x0E */
  u16_t LineInVolume;             /* 0x10 */
  u16_t CDVolume;                 /* 0x12 */
  u16_t VideoVolume;              /* 0x14 */
  u16_t AUXInVolume;              /* 0x16 */
  u16_t PCMOutVolume;             /* 0x18 */
  u16_t RecordSelect;             /* 0x1A */
  u16_t RecordGain;               /* 0x1C */
  u16_t RecordGainMic;            /* 0x1E */
  u16_t GeneralPurpose;           /* 0x20 */
  u16_t Control3D;                /* 0x22 */
  u16_t AudioIntAndPaging;        /* 0x24 */
  u16_t PowerdownControlAndStat;  /* 0x26 */
  u16_t ExtendedAudio1;           /* 0x28 */
  u16_t ExtendedAudio2;           /* 0x2A */
                                  /* ...  */
  u16_t VendorID1;                /* 0x7C */
  u16_t VendorID2;                /* 0x7E */
} ac97_t;



/* Source and output volume control register defines */
#define AC97_MASTER_VOLUME        0x02U   /* Master out */
#define AC97_AUX_OUT_VOLUME       0x04U   /* Auxiliary out volume */
#define AC97_MONO_VOLUME          0x06U   /* Mono out volume */
#define AC97_MASTER_TONE          0x08U   /* high byte= bass, low byte= treble*/
#define AC97_PC_BEEP_VOLUME       0x0aU   /* PC speaker volume */
#define AC97_PHONE_VOLUME         0x0cU   /* Phone volume */
#define AC97_MIC_VOLUME           0x0eU   /* Mic, mono */
#define AC97_LINE_IN_VOLUME       0x10U   /* Line volume */
#define AC97_CD_VOLUME            0x12U   /* CD audio volume */
#define AC97_VIDEO_VOLUME         0x14U   /* Video (TV) volume */
#define AC97_AUX_IN_VOLUME        0x16U   /* Aux line source, left */
#define AC97_PCM_OUT_VOLUME       0x18U   /* The DACs - wav+synth */
#define AC97_RECORD_GAIN_VOLUME   0x1cU   /* Record input level */
#define AC97_RECORD_GAIN_MIC_VOL  0x1eU   /* Record input level */

/* Other AC97 control register defines */
#define AC97_RESET                     0x00U   /* any write here to reset AC97 */
#define AC97_GENERAL_PURPOSE           0x20U   /*  */
#define AC97_POWERDOWN_CONTROL_STAT    0x26U   /*  */
#define AC97_RECORD_SELECT             0x1aU   /* record mux select */
#define AC97_VENDOR_ID1                0x7cU   /* 1st two Vendor ID bytes */
#define AC97_VENDOR_ID2                0x7eU   /* last Vendor ID byte plus rev.
												  number */

/* Record Select defines */
#define AC97_RECORD_MIC         0
#define AC97_RECORD_CD          1
#define AC97_RECORD_VIDEO       2
#define AC97_RECORD_AUX         3
#define AC97_RECORD_LINE        4
#define AC97_RECORD_STEREO_MIX  5
#define AC97_RECORD_MONO_MIX    6
#define AC97_RECORD_PHONE       7

#define MASTER_VOL_MASK     0x1F
#define DAC_VOL_MASK        0x1F
#define AUX_IN_VOL_MASK     0x1F
#define MUTE_MASK           0x8000






#endif /* AC97_H */
