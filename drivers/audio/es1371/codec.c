#include "codec.h"


/* Timeouts in milliseconds */
#define WIP_TIMEOUT     250UL
#define DRDY_TIMEOUT    250UL

/* The default SRC syncronization state number is 1.  This state occurs
   just after de-assertion of SYNC.  This is supposed to be the safest
   state for accessing the codec with an ES1371 Rev 1.  Later versions
   of the chip allegedly don't require syncronization.  Be very careful
   if you change this ! */
   
#define SRC_UNSYNCED 0xffffffffUL
static u32_t SrcSyncState = 0x00010000UL;


void CodecSetSrcSyncState (int state)
{
    if (state < 0)
        SrcSyncState = SRC_UNSYNCED;
    else {
        SrcSyncState = (u32_t)state << 16;
        SrcSyncState &= 0x00070000Ul;
    }
}


int CodecWrite (DEV_STRUCT * pCC, u16_t wAddr, u16_t wData)
{
u32_t dtemp, i;
u16_t  wBaseAddr = pCC->base;

    /* wait for WIP bit (Write In Progress) to go away */
    /* remember, register CONC_dCODECCTL_OFF (0x14) 
       is a pseudo read-write register */
    if (WaitBitd (wBaseAddr + CONC_dCODECCTL_OFF, 30, 0, WIP_TIMEOUT)){
        printf("CODEC_ERR_WIP_TIMEOUT\n");
        return (CODEC_ERR_WIP_TIMEOUT);
    }
    if (SRC_UNSYNCED != SrcSyncState)
    {
        /* enable SRC state data in SRC mux */
        if (WaitBitd (wBaseAddr + CONC_dSRCIO_OFF, SRC_BUSY_BIT, 0, 1000))
            return (CODEC_ERR_SRC_NOT_BUSY_TIMEOUT);

        /* todo: why are we writing an undefined register? */
        dtemp = pci_inl(wBaseAddr + CONC_dSRCIO_OFF);
        pci_outl(wBaseAddr + CONC_dSRCIO_OFF, (dtemp & SRC_CTLMASK) |
                0x00010000UL);

        /* wait for a SAFE time to write addr/data and then do it */
        /*_disable(); */
        for( i = 0; i < 0x1000UL; ++i )
            if( (pci_inl(wBaseAddr + CONC_dSRCIO_OFF) & 0x00070000UL) ==
                    SrcSyncState )
            break;

        if (i >= 0x1000UL) {
            /* _enable(); */
            return (CODEC_ERR_SRC_SYNC_TIMEOUT);
        }
    }

    /* A test for 5880 - prime the PCI data bus */
    {
        u32_t dat = ((u32_t) wAddr << 16) | wData;
        char page = pci_inb(wBaseAddr + CONC_bMEMPAGE_OFF);

        pci_outl (wBaseAddr + CONC_bMEMPAGE_OFF, dat);

        /* write addr and data */
        pci_outl(wBaseAddr + CONC_dCODECCTL_OFF, dat);

        pci_outb(wBaseAddr + CONC_bMEMPAGE_OFF, page);  /* restore page reg */
    }

    if (SRC_UNSYNCED != SrcSyncState)
    {
         /* _enable(); */

        /* restore SRC reg */
        if (WaitBitd (wBaseAddr + CONC_dSRCIO_OFF, SRC_BUSY_BIT, 0, 1000))
            return (CODEC_ERR_SRC_NOT_BUSY_TIMEOUT);

        pci_outl(wBaseAddr + CONC_dSRCIO_OFF, dtemp & 0xfff8ffffUL);
    }

    return 0;
}

int CodecRead (DEV_STRUCT * pCC, u16_t wAddr, u16_t *data)
{
u32_t dtemp, i;
u16_t  base = pCC->base;

    /* wait for WIP to go away */
    if (WaitBitd (base + CONC_dCODECCTL_OFF, 30, 0, WIP_TIMEOUT))
        return (CODEC_ERR_WIP_TIMEOUT);

    if (SRC_UNSYNCED != SrcSyncState) 
    {
        /* enable SRC state data in SRC mux */
        if (WaitBitd (base + CONC_dSRCIO_OFF, SRC_BUSY_BIT, 0, 1000))
            return (CODEC_ERR_SRC_NOT_BUSY_TIMEOUT);

        dtemp = pci_inl(base + CONC_dSRCIO_OFF);
        pci_outl(base + CONC_dSRCIO_OFF, (dtemp & SRC_CTLMASK) |
                0x00010000UL);

        /* wait for a SAFE time to write a read request and then do it */
        /* todo: how do we solve the lock() problem? */
        /* _disable(); */
        for( i = 0; i < 0x1000UL; ++i )
            if( (pci_inl(base + CONC_dSRCIO_OFF) & 0x00070000UL) ==
                    SrcSyncState )
            break;

        if (i >= 0x1000UL) {
            /*_enable();*/
            return (CODEC_ERR_SRC_SYNC_TIMEOUT);
        }
    }

    /* A test for 5880 - prime the PCI data bus */
    { 
        /* set bit 23, this means read in stead of write. */
        u32_t dat = ((u32_t) wAddr << 16) | (1UL << 23);
        char page = pci_inb(base + CONC_bMEMPAGE_OFF);

        /* todo: why are we putting data in the mem page register??? */
        pci_outl(base + CONC_bMEMPAGE_OFF, dat);

        /* write addr w/data=0 and assert read request */
        pci_outl(base + CONC_dCODECCTL_OFF, dat);

        pci_outb(base + CONC_bMEMPAGE_OFF, page);  /* restore page reg */
    
    }
    if (SRC_UNSYNCED != SrcSyncState) 
    {
    
        /*_enable();*/

        /* restore SRC reg */
        if (WaitBitd (base + CONC_dSRCIO_OFF, SRC_BUSY_BIT, 0, 1000))
            return (CODEC_ERR_SRC_NOT_BUSY_TIMEOUT);

        pci_outl(base + CONC_dSRCIO_OFF, dtemp & 0xfff8ffffUL);
    }

    /* now wait for the stinkin' data (DRDY = data ready) */
    if (WaitBitd (base + CONC_dCODECCTL_OFF, 31, 1, DRDY_TIMEOUT))
        return (CODEC_ERR_DATA_TIMEOUT);

    dtemp = pci_inl(base + CONC_dCODECCTL_OFF);

    if (data)
        *data = (u16_t) dtemp;

    return 0;
}


int CodecWriteUnsynced (DEV_STRUCT * pCC, u16_t wAddr, u16_t wData)
{
    /* wait for WIP to go away */
    if (WaitBitd (pCC->base + CONC_dCODECCTL_OFF, 30, 0, WIP_TIMEOUT))
        return (CODEC_ERR_WIP_TIMEOUT);

    /* write addr and data */
    pci_outl(pCC->base + CONC_dCODECCTL_OFF, ((u32_t) wAddr << 16) | wData);
    return 0;
}


int CodecReadUnsynced (DEV_STRUCT * pCC, u16_t wAddr, u16_t *data)
{
u32_t dtemp;

    /* wait for WIP to go away */
    if (WaitBitd (pCC->base + CONC_dCODECCTL_OFF, 30, 0, WIP_TIMEOUT))
        return (CODEC_ERR_WIP_TIMEOUT);

    /* write addr w/data=0 and assert read request */
    pci_outl(pCC->base + CONC_dCODECCTL_OFF, ((u32_t) wAddr << 16) | (1UL << 23));

    /* now wait for the stinkin' data (RDY) */
    if (WaitBitd (pCC->base + CONC_dCODECCTL_OFF, 31, 1, DRDY_TIMEOUT))
        return (CODEC_ERR_DATA_TIMEOUT);

    dtemp = pci_inl(pCC->base + CONC_dCODECCTL_OFF);

    if (data)
        *data = (u16_t) dtemp;

    return 0;
}

int CODECInit( DEV_STRUCT * pCC )
{
int retVal;
    /* All powerdown modes: off */
    
    retVal = CodecWrite (pCC, AC97_POWERDOWN_CONTROL_STAT,  0x0000U);   
    if (OK != retVal)
        return (retVal);
        
    /* Mute Line Out & set to 0dB attenuation */

    retVal = CodecWrite (pCC, AC97_MASTER_VOLUME, 0x0000U);
    if (OK != retVal)
        return (retVal);


    retVal = CodecWrite (pCC, AC97_MONO_VOLUME,   0x8000U);
    if (OK != retVal)
        return (retVal);
    
    retVal = CodecWrite (pCC, AC97_PHONE_VOLUME,  0x8008U);
    if (OK != retVal)
        return (retVal);

    retVal = CodecWrite (pCC, AC97_MIC_VOLUME,    0x0008U);
    if (OK != retVal)
        return (retVal);

    retVal = CodecWrite (pCC, AC97_LINE_IN_VOLUME,   0x0808U);
    if (OK != retVal)
        return (retVal);

    retVal = CodecWrite (pCC, AC97_CD_VOLUME,     0x0808U);
    if (OK != retVal)
        return (retVal);

    retVal = CodecWrite (pCC, AC97_AUX_IN_VOLUME,    0x0808U);
    if (OK != retVal)
        return (retVal);

    retVal = CodecWrite (pCC, AC97_PCM_OUT_VOLUME,    0x0808U);
    if (OK != retVal)
        return (retVal);

    retVal = CodecWrite (pCC, AC97_RECORD_GAIN_VOLUME, 0x0000U);
    if (OK != retVal)
        return (retVal);
    
    /* Connect Line In to ADC */
    retVal = CodecWrite (pCC, AC97_RECORD_SELECT, 0x0404U);  
    if (OK != retVal)
        return (retVal);

    retVal = CodecWrite (pCC, AC97_GENERAL_PURPOSE, 0x0000U);
    if (OK != retVal)
        return (retVal);

    return OK;
}





  
