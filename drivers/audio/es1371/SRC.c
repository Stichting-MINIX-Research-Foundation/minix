#include "SRC.h"

#define SRC_RATE        48000U
#define reg(n) DSP->base + n


int SRCInit ( DEV_STRUCT * DSP )
{
    u32_t   i;
    int     retVal;

    /* Clear all SRC RAM then init - keep SRC disabled until done */
    if (WaitBitd (reg(CONC_dSRCIO_OFF), SRC_BUSY_BIT, 0, 1000))
        return (SRC_ERR_NOT_BUSY_TIMEOUT);

    pci_outl(reg(CONC_dSRCIO_OFF), SRC_DISABLE);

    for( i = 0; i < 0x80; ++i )
        if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, (u16_t)i, 0U)))
            return (retVal);

    if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, SRC_SYNTH_BASE + SRC_TRUNC_N_OFF, 16 << 4)))
        return (retVal);
    if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, SRC_SYNTH_BASE + SRC_INT_REGS_OFF, 16 << 10)))
        return (retVal);
    if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, SRC_DAC_BASE + SRC_TRUNC_N_OFF, 16 << 4)))
        return (retVal);
    if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, SRC_DAC_BASE + SRC_INT_REGS_OFF, 16 << 10)))
        return (retVal);
    if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, SRC_SYNTH_LVOL, 1 << 12)))
        return (retVal);
    if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, SRC_SYNTH_RVOL, 1 << 12)))
        return (retVal);
    if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, SRC_DAC_LVOL, 1 << 12)))
        return (retVal);
    if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, SRC_DAC_RVOL, 1 << 12)))
        return (retVal);
    if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, SRC_ADC_LVOL, 1 << 12)))
        return (retVal);
    if (SRC_SUCCESS != (retVal = SRCRegWrite(DSP, SRC_ADC_RVOL, 1 << 12)))
        return (retVal);

    /* default some rates */
    SRCSetRate(DSP, SRC_SYNTH_BASE, SRC_RATE);
    SRCSetRate(DSP, SRC_DAC_BASE,   SRC_RATE);
    SRCSetRate(DSP, SRC_ADC_BASE,   SRC_RATE);

    /* now enable the whole deal */
    if (WaitBitd (reg(CONC_dSRCIO_OFF), SRC_BUSY_BIT, 0, 1000))
        return (SRC_ERR_NOT_BUSY_TIMEOUT);

    pci_outl(reg(CONC_dSRCIO_OFF), 0UL);

    return 0;
}


int SRCRegRead(DEV_STRUCT * DSP, u16_t reg, u16_t *data)
{
    u32_t dtemp;

    /* wait for ready */
    if (WaitBitd (reg(CONC_dSRCIO_OFF), SRC_BUSY_BIT, 0, 1000))
        return (SRC_ERR_NOT_BUSY_TIMEOUT);

    dtemp = pci_inl(reg(CONC_dSRCIO_OFF));

    /* assert a read request */
    pci_outl(reg(CONC_dSRCIO_OFF),
            (dtemp & SRC_CTLMASK) | ((u32_t) reg << 25));

    /* now wait for the data */
    if (WaitBitd (reg(CONC_dSRCIO_OFF), SRC_BUSY_BIT, 0, 1000))
        return (SRC_ERR_NOT_BUSY_TIMEOUT);

    if (NULL != data)
        *data = (u16_t) pci_inl(reg(CONC_dSRCIO_OFF));

    return 0;
}


int SRCRegWrite(DEV_STRUCT * DSP, u16_t reg, u16_t val)
{
    u32_t dtemp;


    /* wait for ready */
    if (WaitBitd (reg(CONC_dSRCIO_OFF), SRC_BUSY_BIT, 0, 1000))
        return (SRC_ERR_NOT_BUSY_TIMEOUT);

    dtemp = pci_inl(reg(CONC_dSRCIO_OFF));

    /* assert the write request */
    pci_outl(reg(CONC_dSRCIO_OFF),
            (dtemp & SRC_CTLMASK) | SRC_WENABLE | ((u32_t) reg << 25) | val);

    return 0;
}


void SRCSetRate(DEV_STRUCT * DSP, char base, u16_t rate)
{
  u32_t    freq, dtemp, i;
  u16_t     N, truncM, truncStart, wtemp;


  if( base != SRC_ADC_BASE )
  {
      /* freeze the channel */
      dtemp = base == SRC_SYNTH_BASE ? SRC_SYNTHFREEZE : SRC_DACFREEZE;
      for( i = 0; i < SRC_IOPOLL_COUNT; ++i )
          if( !(pci_inl(reg(CONC_dSRCIO_OFF)) & SRC_BUSY) )
              break;
      pci_outl(reg(CONC_dSRCIO_OFF),
              (pci_inl(reg(CONC_dSRCIO_OFF)) & SRC_CTLMASK) |
              dtemp);
  
      /* calculate new frequency and write it - preserve accum */
      /* please don't try to understand. */
      freq = ((u32_t) rate << 16) / 3000U;
      SRCRegRead(DSP, base + SRC_INT_REGS_OFF, &wtemp);
      
      SRCRegWrite(DSP, base + SRC_INT_REGS_OFF,
              (wtemp & 0x00ffU) |
              (u16_t) (freq >> 6) & 0xfc00);
              
      SRCRegWrite(DSP, base + SRC_VFREQ_FRAC_OFF, (u16_t) freq >> 1);
      
      /* un-freeze the channel */
      dtemp = base == SRC_SYNTH_BASE ? SRC_SYNTHFREEZE : SRC_DACFREEZE;
      for( i = 0; i < SRC_IOPOLL_COUNT; ++i )
          if( !(pci_inl(reg(CONC_dSRCIO_OFF)) & SRC_BUSY) )
              break;
      pci_outl(reg(CONC_dSRCIO_OFF),
              (pci_inl(reg(CONC_dSRCIO_OFF)) & SRC_CTLMASK) &
              ~dtemp);
  }
  else /* setting ADC rate */
  {
    /* freeze the channel */
    for( i = 0; i < SRC_IOPOLL_COUNT; ++i )
        if( !(pci_inl(reg(CONC_dSRCIO_OFF)) & SRC_BUSY) )
            break;
    pci_outl(reg(CONC_dSRCIO_OFF),
            (pci_inl(reg(CONC_dSRCIO_OFF)) & SRC_CTLMASK) |
            SRC_ADCFREEZE);



    /* derive oversample ratio */
    N = rate/3000U;
    if( N == 15 || N == 13 || N == 11 || N == 9 )
        --N;
    SRCRegWrite(DSP, SRC_ADC_LVOL, N << 8);
    SRCRegWrite(DSP, SRC_ADC_RVOL, N << 8);

    /* truncate the filter and write n/trunc_start */
    truncM = (21*N - 1) | 1;
    if( rate >= 24000U )
    {
        if( truncM > 239 )
            truncM = 239;
        truncStart = (239 - truncM) >> 1;
        SRCRegWrite(DSP, base + SRC_TRUNC_N_OFF,
                (truncStart << 9) | (N << 4));
    }
    else
    {
        if( truncM > 119 )
            truncM = 119;
        truncStart = (119 - truncM) >> 1;
        SRCRegWrite(DSP, base + SRC_TRUNC_N_OFF,
                0x8000U | (truncStart << 9) | (N << 4));
    }

    /* calculate new frequency and write it - preserve accum */
    freq = ((48000UL << 16) / rate) * N;
    SRCRegRead(DSP, base + SRC_INT_REGS_OFF, &wtemp);
    SRCRegWrite(DSP, base + SRC_INT_REGS_OFF,
            (wtemp & 0x00ffU) |
            (u16_t) (freq >> 6) & 0xfc00);
    SRCRegWrite(DSP, base + SRC_VFREQ_FRAC_OFF, (u16_t) freq >> 1);

    /* un-freeze the channel */
    for( i = 0; i < SRC_IOPOLL_COUNT; ++i )
        if( !(pci_inl(reg(CONC_dSRCIO_OFF)) & SRC_BUSY) )
            break;
    pci_outl(reg(CONC_dSRCIO_OFF),
            (pci_inl(reg(CONC_dSRCIO_OFF)) & SRC_CTLMASK) &
            ~SRC_ADCFREEZE);
  }
  return;
}


