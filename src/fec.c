#include "fec.h"

void fill_pkt(uint16_t larger, const char *payload)
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
    uint16_t larger_pkt = 0;
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
    // fill_pkt(larger_pkt, payload_pkt[0]);
    // fill_pkt(larger_pkt, payload_pkt[1]);
    // fill_pkt(larger_pkt, payload_pkt[2]);
    // fill_pkt(larger_pkt, payload_pkt[3]);

    // xor payload
    char xor_payload[MAX_PAYLOAD_SIZE];
    char payload1;
    char payload2;
    char payload3;
    char payload4;

    for (size_t i = 0; i < MAX_PAYLOAD_SIZE; i++)
    {
        payload1 = (i < length_pkt[0]) ? payload_pkt[0][i] : 0; // take the value in payload or 0 if i > payload size
        payload2 = (i < length_pkt[1]) ? payload_pkt[1][i] : 0;
        payload3 = (i < length_pkt[2]) ? payload_pkt[2][i] : 0;
        payload4 = (i < length_pkt[3]) ? payload_pkt[2][i] : 0;

        xor_payload[i] = payload1 ^ payload2 ^ payload3 ^ payload4;
    }

    pkt_t *fec_pkt = pkt_new();
    pkt_set_length(fec_pkt, xor_length);
    pkt_set_payload(fec_pkt, xor_payload, sizeof(xor_payload));
    pkt_set_seqnum(fec_pkt, seq_number);

    return fec_pkt;
}