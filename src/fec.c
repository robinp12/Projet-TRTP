#include "packet_implem.h"
#include <string.h>
#include <stdlib.h>

int fill_pkt(uint16_t larger, const char *payload)
{
    uint16_t len = strlen(payload) - larger + 1;
    char *zeros = malloc(sizeof(char) * len);
    for (size_t i = 0; i < len; i++)
    {
        zeros[i] = 0;
    }

    strcat((char *)payload, zeros);
    free(zeros);
}

pkt_t *fec_generation(
    pkt_t *pkt1,
    pkt_t *pkt2,
    pkt_t *pkt3,
    pkt_t *pkt4)
{
    // prend le premier numero de seq
    uint8_t seq_number = pkt_get_seqnum(pkt1);

    uint16_t length_pkt[4] = {pkt_get_length(pkt1), pkt_get_length(pkt2), pkt_get_length(pkt3), pkt_get_length(pkt4)};

    const char *payload_pkt[4] = {pkt_get_payload(pkt1), pkt_get_payload(pkt2), pkt_get_payload(pkt3), pkt_get_payload(pkt4)};

    // prendre le plus grand payload
    uint16_t larger_pkt;
    for (size_t i = 0; i < 4; i++)
    {
        if (length_pkt[i] > larger_pkt)
        {
            larger_pkt = length_pkt[i];
        }
    }

    // xor length
    uint16_t xor_length = length_pkt[0] ^ length_pkt[1] ^ length_pkt[2] ^ length_pkt[3];

    // remplir de zero les pkt plus petits
    fill_pkt(larger_pkt, payload_pkt[0]);
    fill_pkt(larger_pkt, payload_pkt[1]);
    fill_pkt(larger_pkt, payload_pkt[2]);
    fill_pkt(larger_pkt, payload_pkt[3]);

    // xor payload
    char xor_payload[larger_pkt];
    for (size_t i = 0; i < larger_pkt; i++)
    {
        xor_payload[i] = payload_pkt[0][i] ^ payload_pkt[1][i] ^ payload_pkt[2][i] ^ payload_pkt[3][i];
    }

    pkt_t *fec_pkt = pkt_new();
    pkt_set_length(fec_pkt, xor_length);
    pkt_set_payload(fec_pkt, xor_payload, sizeof(xor_payload));

    return fec_pkt;
}