/* The eth_stat struct is used in a DL_GETSTAT request the the ehw_task. */

#ifndef _ETH_HW_H
#define _ETH_HW_H

typedef struct eth_stat
{
  unsigned long ets_recvErr,	/* # receive errors */
	ets_sendErr,		/* # send error */
	ets_OVW,		/* # buffer overwrite warnings */
	ets_CRCerr,		/* # crc errors of read */
	ets_frameAll,		/* # frames not alligned (# bits % 8 != 0) */
	ets_missedP,		/* # packets missed due to slow processing */
	ets_packetR,		/* # packets received */
	ets_packetT,		/* # packets transmitted */
	ets_transDef,		/* # transmission defered (Tx was busy) */
	ets_collision,		/* # collissions */
	ets_transAb,		/* # Tx aborted due to excess collisions */
	ets_carrSense,		/* # carrier sense lost */
	ets_fifoUnder,		/* # FIFO underruns (processor too busy) */
	ets_fifoOver,		/* # FIFO overruns (processor too busy) */
	ets_CDheartbeat,	/* # times unable to transmit collision sig*/
	ets_OWC;		/* # times out of window collision */
} eth_stat_t;

#endif /* _ETH_HW_H */
