/*	$NetBSD: am7930var.h,v 1.13 2011/11/23 23:07:32 jmcneill Exp $	*/

struct am7930_softc;

struct am7930_glue {
	uint8_t	(*codec_iread)(struct am7930_softc *sc, int);
	void	(*codec_iwrite)(struct am7930_softc *sc, int, uint8_t);
	uint16_t	(*codec_iread16)(struct am7930_softc *sc, int);
	void	(*codec_iwrite16)(struct am7930_softc *sc, int, uint16_t);
	void	(*onopen)(struct am7930_softc *sc);
	void	(*onclose)(struct am7930_softc *sc);
	int	factor;
	stream_filter_factory_t *input_conv;
	stream_filter_factory_t *output_conv;
};

struct am7930_softc {
	device_t sc_dev;	/* base device */

	uint8_t	sc_rlevel;	/* record level */
	uint8_t	sc_plevel;	/* play level */
	uint8_t	sc_mlevel;	/* monitor level */
	uint8_t	sc_out_port;	/* output port */
	uint8_t	sc_mic_mute;

	struct am7930_glue *sc_glue;

	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;
};

extern int     am7930debug;

void	am7930_init(struct am7930_softc *, int);

#define AM7930_IWRITE(x,y,z)	(*(x)->sc_glue->codec_iwrite)((x),(y),(z))
#define AM7930_IREAD(x,y)	(*(x)->sc_glue->codec_iread)((x),(y))
#define AM7930_IWRITE16(x,y,z)	(*(x)->sc_glue->codec_iwrite16)((x),(y),(z))
#define AM7930_IREAD16(x,y)	(*(x)->sc_glue->codec_iread16)((x),(y))

#define AUDIOAMD_POLL_MODE	0
#define AUDIOAMD_DMA_MODE	1

/*
 * audio channel definitions.
 */

#define AUDIOAMD_SPEAKER_VOL	0	/* speaker volume */
#define AUDIOAMD_HEADPHONES_VOL	1	/* headphones volume */
#define AUDIOAMD_OUTPUT_CLASS	2

#define AUDIOAMD_MONITOR_VOL	3	/* monitor input volume */
#define AUDIOAMD_MONITOR_OUTPUT	4	/* output selector */
#define AUDIOAMD_MONITOR_CLASS	5

#define AUDIOAMD_MIC_VOL	6	/* microphone volume */
#define AUDIOAMD_MIC_MUTE	7
#define AUDIOAMD_INPUT_CLASS	8

#define AUDIOAMD_RECORD_SOURCE	9	/* source selector */
#define AUDIOAMD_RECORD_CLASS	10

/*
 * audio(9) MI callbacks from upper-level audio layer.
 */

struct audio_device;
struct audio_encoding;
struct audio_params;

int	am7930_open(void *, int);
void	am7930_close(void *);
int	am7930_query_encoding(void *, struct audio_encoding *);
int	am7930_set_params(void *, int, int, audio_params_t *,
	    audio_params_t *, stream_filter_list_t *, stream_filter_list_t *);
int	am7930_commit_settings(void *);
int	am7930_round_blocksize(void *, int, int, const audio_params_t *);
int	am7930_halt_output(void *);
int	am7930_halt_input(void *);
int	am7930_getdev(void *, struct audio_device *);
int	am7930_get_props(void *);
int	am7930_set_port(void *, mixer_ctrl_t *);
int	am7930_get_port(void *, mixer_ctrl_t *);
int	am7930_query_devinfo(void *, mixer_devinfo_t *);
