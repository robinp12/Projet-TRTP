#include "packet_implem.h"

/* Extra #includes */
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "log.h"


/* Your code will be inserted here */

struct __attribute__((__packed__)) pkt
{
    ptypes_t type : 2;
    uint8_t tr : 1;
    uint8_t window : 5;
    uint16_t length : 16;
    uint8_t seqnum : 8;
    uint32_t timestamp : 32;

    uint32_t crc1 : 32;
    char *payload;
    uint32_t crc2 : 32;
};

/* Extra code */
/* Your code will be inserted here */

pkt_t *pkt_new()
{
    pkt_t *pkt = (pkt_t *)malloc(sizeof(pkt_t));
    if (pkt == NULL)
    {
        fprintf(stderr, "Erreur lors du malloc du package");
        return NULL;
    }
    pkt->type = 0;
    pkt->tr = 0;
    pkt->window = 0;
    pkt->length = 0;
    pkt->seqnum = 0;
    pkt->timestamp = 0;
    pkt->crc1 = 0;
    pkt->payload = NULL;
    pkt->crc2 = 0;
    return pkt;
}

void pkt_del(pkt_t *pkt)
{
    if (pkt != NULL)
    {
        if (pkt->payload != NULL)
        {
            free(pkt->payload);
        }
        free(pkt);
    }
}

pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt)
{
    if (!len)
    {
        return E_UNCONSISTENT;
    }
    if (len < 6)
    {
        return E_NOHEADER;
    }
    if (len < 10)
    {
        return E_CRC;
    }

    pkt_status_code error;
    uint16_t length = 0;
    uint8_t type = ((uint8_t)*data) >> 6;

    error = pkt_set_type(pkt, type);
    if (error != PKT_OK)
    {
        return error;
    }

    uint8_t tr = (*data & 0b00100000) >> 7;
    error = pkt_set_tr(pkt, tr);
    if (error != PKT_OK)
    {
        return error;
    }

    uint8_t window = *data & 0b00011111;
    error = pkt_set_window(pkt, window);
    if (error != PKT_OK)
    {
        return error;
    }

    uint64_t offset = 1;
    if (pkt->type == PTYPE_DATA || pkt->type == PTYPE_FEC)
    {

        memcpy(&length, data + offset, 2);
        length = ntohs(length);

        error = pkt_set_length(pkt, length);
        if (error != PKT_OK)
        {
            return error;
        }
        offset += 2;

        if (length + 32 < (int)len)
        {
            return E_UNCONSISTENT;
        }
    }

    uint8_t seqnum = data[offset];
    offset++;
    error = pkt_set_seqnum(pkt, seqnum);
    if (error != PKT_OK)
    {
        return error;
    }

    uint32_t timestamp;
    memcpy(&timestamp, data + offset, 4);
    offset += 4;
    error = pkt_set_timestamp(pkt, timestamp);
    if (error != PKT_OK)
    {
        return error;
    }

    uint32_t crc1;
    memcpy(&crc1, data + offset, 4);
    offset += 4;
    crc1 = ntohl(crc1);
    error = pkt_set_crc1(pkt, crc1);
    if (error != PKT_OK)
    {
        return error;
    }

    size_t header_len = predict_header_length(pkt);
    if (header_len > PKT_MAX_HEADERLEN){
        return E_UNCONSISTENT;
    }
    char *header = (char *) malloc(header_len);
    
    memcpy(header, data, header_len);
    *header = *header & 0b11011111; // TR mis a 0
    uint32_t crc1_new = (uint32_t) crc32(0L, (const unsigned char *)header, header_len);
    if (crc1_new != crc1){
        pkt_set_crc1(pkt, crc1_new);
        free(header);
        return E_CRC;
    }
    pkt_set_crc1(pkt, crc1_new);
    free(header);

    if (pkt->type == PTYPE_ACK || pkt->type == PTYPE_NACK)
    {
        return PKT_OK;
    }

    if (MAX_PAYLOAD_SIZE + 32 < len)
    {
        return E_UNCONSISTENT;
    }

    error = pkt_set_payload(pkt, data + offset, length);
    if (error != PKT_OK)
    {
        return error;
    }
    if (pkt->type == PTYPE_FEC)
    {
        offset += MAX_PAYLOAD_SIZE;
    }
    else
    {
        offset += length;
    }

    uint32_t crc2;
    memcpy(&crc2, data + offset, 4);
    crc2 = ntohl(crc2);
    error = pkt_set_crc2(pkt, crc2);
    if (error != PKT_OK)
    {
        return error;
    }

    if (pkt->type == PTYPE_FEC && ((uint32_t) crc32(0L, (const unsigned char *)pkt->payload, MAX_PAYLOAD_SIZE)) != crc2)
    {
        return E_CRC;
    }
    else if (pkt->type == PTYPE_DATA && pkt->tr == 0 && ((uint32_t) crc32(0L, (const unsigned char *)pkt->payload, pkt->length)) != crc2)
    {
        return E_CRC;
    }
    return PKT_OK;
}

pkt_status_code pkt_encode(const pkt_t *pkt, char *buf, size_t *len)
{
    size_t size = predict_header_length(pkt);
    if (pkt->type == PTYPE_DATA && pkt_get_length(pkt) != 0)
    {
        size += pkt_get_length(pkt) + 4; // + 4 pour le crc2 associé
    }
    else if (pkt->type == PTYPE_FEC)
    {
        size += MAX_PAYLOAD_SIZE + 4;
    }

    size += 4; // pour le crc1
    if (size > *len)
    {
        return E_NOMEM;
    }
    
    *buf = 0;
    *buf = pkt_get_type(pkt) << 6;
    *buf += pkt_get_window(pkt);
    *len = 1;


    if (pkt->type == PTYPE_DATA)
    {
        uint16_t length = htons(pkt->length);
        memcpy(buf + (*len), &length, 2);
        *len += 2;
    }
    if (pkt->type == PTYPE_FEC)
    {
        uint16_t length = htons(MAX_PAYLOAD_SIZE);
        memcpy(buf + (*len), &length, 2);
        *len += 2;
    }

    uint8_t seqnum = pkt->seqnum;
    memcpy(buf + (*len), &seqnum, 1);
    *len += 1;

    uint32_t timestamp = pkt->timestamp;
    memcpy(buf + (*len), &timestamp, 4);
    *len += 4;

    *buf = *buf & 0b11011111;

    uint32_t crc1 = crc32(0L, (const unsigned char *)buf, predict_header_length(pkt));
    // pkt_set_crc1(pkt, crc1);
    crc1 = htonl(crc1);
    memcpy(buf + (*len), &crc1, 4);

    *len += 4;
    *buf += pkt_get_tr(pkt) << 5;

    if (pkt->type == PTYPE_ACK || pkt->type == PTYPE_NACK)
    {
        return PKT_OK;
    }

    if (pkt->type == PTYPE_DATA && pkt->tr == 0)
    {
        memcpy(buf + (*len), pkt_get_payload(pkt), pkt->length);

        uint32_t crc2 = crc32(0L, (const unsigned char *)buf + (*len), pkt->length);
        // pkt_set_crc2(pkt, crc2);
        crc2 = htonl(crc2);
        *len += pkt->length;
        memcpy(buf + (*len), &crc2, 4);
        *len += 4;
        return PKT_OK;
    }
    else if (pkt->type == PTYPE_FEC)
    {
        memcpy(buf + (*len), pkt_get_payload(pkt), MAX_PAYLOAD_SIZE);

        uint32_t crc2 = crc32(0L, (const unsigned char *)buf + (*len), MAX_PAYLOAD_SIZE);
        // pkt_set_crc2(pkt, crc2);
        crc2 = htonl(crc2);
        *len += MAX_PAYLOAD_SIZE;

        memcpy(buf + (*len), &crc2, 4);
        *len += 4;
        return PKT_OK;
    }
    return PKT_OK;
}

ptypes_t pkt_get_type(const pkt_t *pkt)
{
    return pkt->type;
}

uint8_t pkt_get_tr(const pkt_t *pkt)
{
    return pkt->tr;
}

uint8_t pkt_get_window(const pkt_t *pkt)
{
    return pkt->window;
}

uint8_t pkt_get_seqnum(const pkt_t *pkt)
{
    return pkt->seqnum;
}

uint16_t pkt_get_length(const pkt_t *pkt)
{
    return pkt->length;
}

uint32_t pkt_get_timestamp(const pkt_t *pkt)
{
    return pkt->timestamp;
}

uint32_t pkt_get_crc1(const pkt_t *pkt)
{
    return pkt->crc1;
}

uint32_t pkt_get_crc2(const pkt_t *pkt)
{
    if ((pkt->type == PTYPE_DATA && pkt->tr == 0) || pkt->type == PTYPE_FEC)
    {
        return pkt->crc2;
    }
    return 0;
}

const char *pkt_get_payload(const pkt_t *pkt)
{
    if ((pkt->type == PTYPE_DATA && pkt->tr == 0) || pkt->type == PTYPE_FEC)
    {
        return pkt->payload;
    }
    return NULL;
}

pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type)
{
    if (type != PTYPE_DATA && type != PTYPE_ACK && type != PTYPE_NACK && type != PTYPE_FEC)
    {
        return E_TYPE;
    }
    pkt->type = type;
    return PKT_OK;
}

pkt_status_code pkt_set_tr(pkt_t *pkt, const uint8_t tr)
{
    if (tr != 0 && tr != 1)
    {
        return E_TR;
    }
    if (tr == 1 && pkt->type != PTYPE_DATA)
    {
        return E_TR;
    }
    pkt->tr = tr;
    return PKT_OK;
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window)
{
    if (window > MAX_WINDOW_SIZE)
    {
        return E_WINDOW;
    }
    pkt->window = window;
    return PKT_OK;
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
    pkt->seqnum = seqnum;
    return PKT_OK;
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
    if ((pkt->type != PTYPE_DATA && pkt->type != PTYPE_FEC) || length > MAX_PAYLOAD_SIZE)
    {
        return E_LENGTH;
    }
    pkt->length = length;
    return PKT_OK;
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp)
{
    pkt->timestamp = timestamp;
    return PKT_OK;
}

pkt_status_code pkt_set_crc1(pkt_t *pkt, const uint32_t crc1)
{
    pkt->crc1 = crc1;
    return PKT_OK;
}

pkt_status_code pkt_set_crc2(pkt_t *pkt, const uint32_t crc2)
{
    if (pkt->length == 0 || pkt->tr == 1)
    {
        return E_CRC;
    }
    pkt->crc2 = crc2;
    return PKT_OK;
}

pkt_status_code pkt_set_payload(pkt_t *pkt, const char *data, const uint16_t length)
{
    if (pkt->type == PTYPE_DATA && pkt_set_length(pkt, length) != PKT_OK)
    {
        return E_LENGTH;
    }
    if (pkt_get_payload(pkt) != NULL)
    {
        free(pkt->payload);
    }
    if (pkt->type == PTYPE_DATA)
    {
        if (length == 0){
            return E_LENGTH;
        }
        pkt->payload = (char *)malloc(sizeof(char)*length);
        if (pkt->payload == NULL)
        {
            return E_NOMEM;
        }
        memcpy(pkt->payload, data, length);
    }
    if (pkt->type == PTYPE_FEC)
    {
        pkt->payload = (char *)malloc(MAX_PAYLOAD_SIZE);
        if (pkt->payload == NULL)
        {
            return E_NOMEM;
        }
        memcpy(pkt->payload, data, MAX_PAYLOAD_SIZE);
    }
    return PKT_OK;
}

ssize_t predict_header_length(const pkt_t *pkt)
{
    if (pkt_get_type(pkt) == PTYPE_DATA || pkt_get_type(pkt) == PTYPE_FEC)
    {
        return 8;
    }
    if (pkt_get_type(pkt) == PTYPE_ACK || pkt_get_type(pkt) == PTYPE_NACK)
    {
        return 6;
    }
    return -1;
}
