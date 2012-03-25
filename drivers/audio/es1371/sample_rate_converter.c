#include "sample_rate_converter.h"




#define SRC_RATE        48000U
#define reg(n) DSP->base + n


/* register/base and control equates for the SRC RAM */
#define SRC_SYNTH_FIFO      0x00
#define SRC_DAC_FIFO        0x20
#define SRC_ADC_FIFO        0x40

#define SRC_SYNTH_LVOL      0x7c
#define SRC_SYNTH_RVOL      0x7d
#define SRC_DAC_LVOL        0x7e
#define SRC_DAC_RVOL        0x7f
#define SRC_ADC_LVOL        0x6c
#define SRC_ADC_RVOL        0x6d

#define SRC_TRUNC_N_OFF     0x00
#define SRC_INT_REGS_OFF    0x01
#define SRC_ACCUM_FRAC_OFF  0x02
#define SRC_VFREQ_FRAC_OFF  0x03

/* miscellaneous control defines */
#define SRC_IOPOLL_COUNT    0x1000UL

#define SRC_SYNTHFREEZE     (1UL << 21)
#define SRC_DACFREEZE       (1UL << 20)
#define SRC_ADCFREEZE       (1UL << 19)




static int src_reg_read(const DEV_STRUCT * DSP, u16_t reg, u16_t
	*data);
static int src_reg_write(const DEV_STRUCT * DSP, u16_t reg, u16_t val);


int src_init ( DEV_STRUCT * DSP ) {
	u32_t   i;
	int     retVal;

	/* Clear all SRC RAM then init - keep SRC disabled until done */
	/* Wait till SRC_RAM_BUSY is 0 */
	if (WaitBitd (reg(SAMPLE_RATE_CONV), SRC_BUSY_BIT, 0, 1000))
		return (SRC_ERR_NOT_BUSY_TIMEOUT);

	pci_outl(reg(SAMPLE_RATE_CONV), SRC_DISABLE);

	/* from the opensound system driver, no idea where the specification is */
	/* there are indeed 7 bits for the addresses of the SRC */
	for( i = 0; i < 0x80; ++i ) {
		if (SRC_SUCCESS != (retVal = src_reg_write(DSP, (u16_t)i, 0U)))
			return (retVal);
	}

	if (SRC_SUCCESS != (retVal = 
				src_reg_write(DSP, SRC_SYNTH_BASE + SRC_TRUNC_N_OFF, 16 << 4)))
		return (retVal);
	if (SRC_SUCCESS != (retVal = src_reg_write(DSP, 
					SRC_SYNTH_BASE + SRC_INT_REGS_OFF, 16 << 10)))
		return (retVal);
	if (SRC_SUCCESS != (retVal = 
				src_reg_write(DSP, SRC_DAC_BASE + SRC_TRUNC_N_OFF, 16 << 4)))
		return (retVal);
	if (SRC_SUCCESS != (retVal = 
				src_reg_write(DSP, SRC_DAC_BASE + SRC_INT_REGS_OFF, 16 << 10)))
		return (retVal);
	if (SRC_SUCCESS != (retVal = 
				src_reg_write(DSP, SRC_SYNTH_LVOL, 1 << 12)))
		return (retVal);
	if (SRC_SUCCESS != (retVal = 
				src_reg_write(DSP, SRC_SYNTH_RVOL, 1 << 12)))
		return (retVal);
	if (SRC_SUCCESS != (retVal = 
				src_reg_write(DSP, SRC_DAC_LVOL, 1 << 12)))
		return (retVal);
	if (SRC_SUCCESS != (retVal = 
				src_reg_write(DSP, SRC_DAC_RVOL, 1 << 12)))
		return (retVal);
	if (SRC_SUCCESS != (retVal = 
				src_reg_write(DSP, SRC_ADC_LVOL, 1 << 12)))
		return (retVal);
	if (SRC_SUCCESS != (retVal = 
				src_reg_write(DSP, SRC_ADC_RVOL, 1 << 12)))
		return (retVal);

	/* default some rates */
	src_set_rate(DSP, SRC_SYNTH_BASE, SRC_RATE);
	src_set_rate(DSP, SRC_DAC_BASE,   SRC_RATE);
	src_set_rate(DSP, SRC_ADC_BASE,   SRC_RATE);

	/* now enable the whole deal */
	if (WaitBitd (reg(SAMPLE_RATE_CONV), SRC_BUSY_BIT, 0, 1000))
		return (SRC_ERR_NOT_BUSY_TIMEOUT);

	pci_outl(reg(SAMPLE_RATE_CONV), 0UL);

	return 0;
}


static int src_reg_read(const DEV_STRUCT * DSP, u16_t reg, u16_t *data) {
	u32_t dtemp;

	/* wait for ready */
	if (WaitBitd (reg(SAMPLE_RATE_CONV), SRC_BUSY_BIT, 0, 1000))
		return (SRC_ERR_NOT_BUSY_TIMEOUT);

	dtemp = pci_inl(reg(SAMPLE_RATE_CONV));

	/* assert a read request */
	/*pci_outl(reg(SAMPLE_RATE_CONV),
	  (dtemp & SRC_CTLMASK) | ((u32_t) reg << 25));*/
	pci_outl(reg(SAMPLE_RATE_CONV), (dtemp & 
				(DIS_REC|DIS_P2|DIS_P1|SRC_DISABLE)) | ((u32_t) reg << 25));

	/* now wait for the data */
	if (WaitBitd (reg(SAMPLE_RATE_CONV), SRC_BUSY_BIT, 0, 1000))
		return (SRC_ERR_NOT_BUSY_TIMEOUT);

	if (NULL != data)
		*data = (u16_t) pci_inl(reg(SAMPLE_RATE_CONV));

	return 0;
}


static int src_reg_write(const DEV_STRUCT * DSP, u16_t reg, u16_t val) {
	u32_t dtemp;

	/* wait for ready */
	if (WaitBitd (reg(SAMPLE_RATE_CONV), SRC_BUSY_BIT, 0, 1000))
		return (SRC_ERR_NOT_BUSY_TIMEOUT);

	dtemp = pci_inl(reg(SAMPLE_RATE_CONV));

	/* assert the write request */
	pci_outl(reg(SAMPLE_RATE_CONV),
	  (dtemp & SRC_CTLMASK) | SRC_RAM_WE | ((u32_t) reg << 25) | val); 

	return 0;
}


void src_set_rate(const DEV_STRUCT * DSP, char base, u16_t rate) {
	u32_t    freq, dtemp, i;
	u16_t     N, truncM, truncStart, wtemp;


	if( base != SRC_ADC_BASE )
	{
		/* freeze the channel */
		dtemp = base == SRC_SYNTH_BASE ? SRC_SYNTHFREEZE : SRC_DACFREEZE;
		for( i = 0; i < SRC_IOPOLL_COUNT; ++i )
			if( !(pci_inl(reg(SAMPLE_RATE_CONV)) & SRC_RAM_BUSY) )
				break;
		pci_outl(reg(SAMPLE_RATE_CONV),
				(pci_inl(reg(SAMPLE_RATE_CONV)) & SRC_CTLMASK) |
				dtemp);

		/* calculate new frequency and write it - preserve accum */
		/* please don't try to understand. */
		freq = ((u32_t) rate << 16) / 3000U;
		src_reg_read(DSP, base + SRC_INT_REGS_OFF, &wtemp);

		src_reg_write(DSP, base + SRC_INT_REGS_OFF,
				(wtemp & 0x00ffU) |
				((u16_t) (freq >> 6) & 0xfc00));

		src_reg_write(DSP, base + SRC_VFREQ_FRAC_OFF, (u16_t) freq >> 1);

		/* un-freeze the channel */
		dtemp = base == SRC_SYNTH_BASE ? SRC_SYNTHFREEZE : SRC_DACFREEZE;
		for( i = 0; i < SRC_IOPOLL_COUNT; ++i )
			if( !(pci_inl(reg(SAMPLE_RATE_CONV)) & SRC_RAM_BUSY) )
				break;
		pci_outl(reg(SAMPLE_RATE_CONV),
				(pci_inl(reg(SAMPLE_RATE_CONV)) & SRC_CTLMASK) &
				~dtemp);
	}
	else /* setting ADC rate */
	{
		/* freeze the channel */
		for( i = 0; i < SRC_IOPOLL_COUNT; ++i )
			if( !(pci_inl(reg(SAMPLE_RATE_CONV)) & SRC_RAM_BUSY) )
				break;
		pci_outl(reg(SAMPLE_RATE_CONV),
				(pci_inl(reg(SAMPLE_RATE_CONV)) & SRC_CTLMASK) |
				SRC_ADCFREEZE);



		/* derive oversample ratio */
		N = rate/3000U;
		if( N == 15 || N == 13 || N == 11 || N == 9 )
			--N;
		src_reg_write(DSP, SRC_ADC_LVOL, N << 8);
		src_reg_write(DSP, SRC_ADC_RVOL, N << 8);

		/* truncate the filter and write n/trunc_start */
		truncM = (21*N - 1) | 1;
		if( rate >= 24000U )
		{
			if( truncM > 239 )
				truncM = 239;
			truncStart = (239 - truncM) >> 1;
			src_reg_write(DSP, base + SRC_TRUNC_N_OFF,
					(truncStart << 9) | (N << 4));
		}
		else
		{
			if( truncM > 119 )
				truncM = 119;
			truncStart = (119 - truncM) >> 1;
			src_reg_write(DSP, base + SRC_TRUNC_N_OFF,
					0x8000U | (truncStart << 9) | (N << 4));
		}

		/* calculate new frequency and write it - preserve accum */
		freq = ((48000UL << 16) / rate) * N;
		src_reg_read(DSP, base + SRC_INT_REGS_OFF, &wtemp);
		src_reg_write(DSP, base + SRC_INT_REGS_OFF,
				(wtemp & 0x00ffU) |
				((u16_t) (freq >> 6) & 0xfc00));
		src_reg_write(DSP, base + SRC_VFREQ_FRAC_OFF, (u16_t) freq >> 1);

		/* un-freeze the channel */
		for( i = 0; i < SRC_IOPOLL_COUNT; ++i )
			if( !(pci_inl(reg(SAMPLE_RATE_CONV)) & SRC_RAM_BUSY) )
				break;
		pci_outl(reg(SAMPLE_RATE_CONV),
				(pci_inl(reg(SAMPLE_RATE_CONV)) & SRC_CTLMASK) &
				~SRC_ADCFREEZE);
	}
	return;
}
