#ifndef __FEC_H__
#define __FEC_H__

#include "packet_implem.h"
#include <string.h>
#include <stdlib.h>

typedef struct fec_pkt
{
    pkt_t* pkts[4];
    uint8_t seqnum;
    uint8_t i;
} fec_pkt_t;

/*
* Generate a fec pkt
*/
pkt_t *fec_generation(pkt_t *pkt1, pkt_t *pkt2, pkt_t *pkt3, pkt_t *pkt4);


#endif