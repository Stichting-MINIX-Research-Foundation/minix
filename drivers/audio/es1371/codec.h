#ifndef CODEC_H
#define CODEC_H

#include "es1371.h"
#include "wait.h" 
#include "../AC97.h"
#include "SRC.h"

#define CODEC_0DB_GAIN          0x0008
#define CODEC_MAX_ATTN          0x003f
#define CODEC_MUTE              0x8000U


/* Control function defines */
#define CODEC_CTL_4SPKR         0x00U   /* 4-spkr output mode enable */
#define CODEC_CTL_MICBOOST      0x01U   /* Mic boost (+30 dB) enable */
#define CODEC_CTL_PWRDOWN       0x02U   /* power-down mode */
#define CODEC_CTL_DOSMODE       0x03U   /* A/D sync to DAC1 */

                                                /* Timeout waiting for: */
#define CODEC_ERR_WIP_TIMEOUT           -1      /* write in progress complete */
#define CODEC_ERR_DATA_TIMEOUT          -2      /* data ready */
#define CODEC_ERR_SRC_NOT_BUSY_TIMEOUT  -3      /* SRC not busy */
#define CODEC_ERR_SRC_SYNC_TIMEOUT      -4      /* state #1 */

/* Function to inform CODEC module which AC97 vendor ID to expect */
void CodecSetVendorId (char *tbuf);

/* CODEC Mixer and Mode control function prototypes */

int  CodecRead (DEV_STRUCT * pCC, u16_t wAddr, u16_t *data);
int  CodecWrite (DEV_STRUCT * pCC, u16_t wAddr, u16_t wData);
void CodecSetSrcSyncState (int state);
int  CodecWriteUnsynced (DEV_STRUCT * pCC, u16_t wAddr, u16_t wData);
int  CodecReadUnsynced (DEV_STRUCT * pCC, u16_t wAddr, u16_t *data);

/*
  This function initializes the CODEC to a default mode.
*/
int CODECInit( DEV_STRUCT * pCC );



#endif /* CODEC_H */
