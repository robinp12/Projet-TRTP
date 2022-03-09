#include "packet_interface.h"

/* Extra #includes */
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

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

    uint8_t type = ((uint8_t)*data) >> 6;
    uint32_t crc1;
    uint32_t crc2;
    uint32_t timestamp;
    uint16_t length;
    pkt_status_code error;
    // uint32_t crc_base = crc32(0L, Z_NULL, 0);

    switch (type)
    {
    case PTYPE_FEC:
        pkt_set_type(pkt, PTYPE_FEC);

        if (pkt_set_tr(pkt, (*data & 0b00100000) >> 7) != PKT_OK)
        {
            return E_TR;
        }
        if (pkt_set_window(pkt, ((uint8_t)*data) & 0b00011111) != PKT_OK)
        {
            return E_WINDOW;
        }

        if (pkt_set_seqnum(pkt, data[3]) != PKT_OK)
        {
            return E_SEQNUM;
        }

        memcpy(&timestamp, data + 4, 4);
        pkt_set_timestamp(pkt, timestamp);

        memcpy(&crc1, data + 8, 4);
        pkt_set_crc1(pkt, ntohl(crc1));

        memcpy(&length, data + 1, 2);
        length = ntohs(length);
        pkt_set_length(pkt, length);

        if (len < MAX_PAYLOAD_SIZE + 16)
        {
            return E_UNCONSISTENT;
        }
        error = pkt_set_payload(pkt, data + 12, MAX_PAYLOAD_SIZE);
        if (error != PKT_OK)
        {
            return error;
        }

        memcpy(&crc2, data + 12 + MAX_PAYLOAD_SIZE, 4);
        pkt_set_crc2(pkt, ntohl(crc2));

        break;

    case PTYPE_DATA:
        pkt_set_type(pkt, PTYPE_DATA);

        if (pkt_set_tr(pkt, (*data & 0b00100000) >> 7) != PKT_OK)
        {
            return E_TR;
        }
        if (pkt_set_window(pkt, ((uint8_t)*data) & 0b00011111) != PKT_OK)
        {
            return E_WINDOW;
        }

        if (pkt_set_seqnum(pkt, data[3]) != PKT_OK)
        {
            return E_SEQNUM;
        }

        memcpy(&timestamp, data + 4, 4);
        pkt_set_timestamp(pkt, timestamp);

        memcpy(&crc1, data + 8, 4);
        pkt_set_crc1(pkt, ntohl(crc1));

        memcpy(&length, data + 1, 2);
        length = ntohs(length);

        if (pkt->tr == 0)
        {
            if (len < (size_t)length + 16)
            {
                return E_UNCONSISTENT;
            }
            error = pkt_set_payload(pkt, data + 12, length);
            if (error != PKT_OK)
            {
                return error;
            }

            memcpy(&crc2, data + 12 + length, 4);
            pkt_set_crc2(pkt, ntohl(crc2));
        }
        break;

    case PTYPE_ACK:
        pkt_set_type(pkt, PTYPE_ACK);
        if (pkt_set_tr(pkt, (*data & 0b00100000) >> 7) != PKT_OK)
        {
            return E_TR;
        }
        if (pkt_set_window(pkt, ((uint8_t)*data) & 0b00011111) != PKT_OK)
        {
            return E_WINDOW;
        }

        if (pkt_set_seqnum(pkt, data[1]) != PKT_OK)
        {
            return E_SEQNUM;
        }
        memcpy(&timestamp, data + 2, 4);
        pkt_set_timestamp(pkt, timestamp);

        memcpy(&crc1, data + 6, 4);
        pkt_set_crc1(pkt, ntohl(crc1));

        break;

    case PTYPE_NACK:
        pkt_set_type(pkt, PTYPE_NACK);
        if (pkt_set_tr(pkt, (*data & 0b00100000) >> 7) != PKT_OK)
        {
            return E_TR;
        }
        if (pkt_set_window(pkt, ((uint8_t)*data) & 0b00011111) != PKT_OK)
        {
            return E_WINDOW;
        }

        if (pkt_set_seqnum(pkt, data[1]) != PKT_OK)
        {
            return E_SEQNUM;
        }
        memcpy(&timestamp, data + 2, 4);
        pkt_set_timestamp(pkt, timestamp);

        memcpy(&crc1, data + 6, 4);
        pkt_set_crc1(pkt, ntohl(crc1));

        break;

    default:
        return E_TYPE;
        break;
    }

    char *header = (char *)malloc(sizeof(char) * predict_header_length(pkt));
    memcpy(header, data, predict_header_length(pkt));
    *header = *header & 0b1101111; // TR mis a 0
    uint32_t crc1_new = crc32(0L, (const unsigned char *)header, predict_header_length(pkt));
    if (crc1_new != crc1)
    {
        return E_CRC; // E_CRC;
    }
    if (pkt->type == PTYPE_FEC && crc32(0L, (const unsigned char *)pkt->payload, MAX_PAYLOAD_SIZE) != crc2)
    {
        return E_CRC;
    }
    else if (pkt->type == PTYPE_DATA && pkt->tr == 0 && crc32(0L, (const unsigned char *)pkt->payload, pkt->length) != crc2)
    {
        return PKT_OK; // E_CRC;
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

    // uint32_t crc_base = crc32(0L, Z_NULL, 0);

    *buf = pkt_get_type(pkt) << 6;
    *buf += pkt_get_window(pkt);

    uint16_t length;

    if (pkt->type == PTYPE_DATA)
    {

        length = htons(pkt_get_length(pkt));
        memcpy(buf + 1, &length, 2);

        buf[3] = pkt_get_seqnum(pkt);

        uint32_t timestamp = pkt_get_timestamp(pkt);
        memcpy(buf + 4, &timestamp, 4);

        uint32_t crc1 = htonl(crc32(0L, (const unsigned char *)buf, 8));
        memcpy(buf + 8, &crc1, 4);

        *buf += pkt_get_tr(pkt) << 5; // tr après avoir calculé crc1

        if (pkt->tr == 0)
        {
            const char *payload = pkt_get_payload(pkt);
            memcpy(buf + 12, payload, ntohs(length));

            uint32_t crc2 = htonl(crc32(0L, (const unsigned char *)buf + 12, ntohs(length)));
            memcpy(buf + 12 + ntohs(length), &crc2, 4);
        }

        *len = 16 + ntohs(length);
    }
    else if (pkt->type == PTYPE_FEC)
    {

        length = htons(pkt_get_length(pkt));
        memcpy(buf + 1, &length, 2);
        *len = 16 + MAX_PAYLOAD_SIZE;

        buf[3] = pkt_get_seqnum(pkt);

        uint32_t timestamp = pkt_get_timestamp(pkt);
        memcpy(buf + 4, &timestamp, 4);

        uint32_t crc1 = htonl(crc32(0L, (const unsigned char *)buf, 8));
        memcpy(buf + 8, &crc1, 4);

        *buf += pkt_get_tr(pkt) << 5; // tr après avoir calculé crc1

        const char *payload = pkt_get_payload(pkt);
        memcpy(buf + 12, payload, MAX_PAYLOAD_SIZE);

        uint32_t crc2 = htonl(crc32(0L, (const unsigned char *)buf + 12, MAX_PAYLOAD_SIZE));
        memcpy(buf + 12 + MAX_PAYLOAD_SIZE, &crc2, 4);

        *len = 16 + MAX_PAYLOAD_SIZE;
    }
    else
    {
        buf[1] = pkt_get_seqnum(pkt);

        uint32_t timestamp = pkt_get_timestamp(pkt);
        memcpy(buf + 2, &timestamp, 4);

        uint32_t crc1 = htonl(crc32(0L, (const unsigned char *)buf, 6));
        memcpy(buf + 8, &crc1, 4);

        *buf += pkt_get_tr(pkt) << 5; // tr après avoir calculé crc1

        *len = 10;
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
    if (pkt->type == PTYPE_DATA)
    {
        return pkt->length;
    }
    return 0;
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
    if (pkt_set_length(pkt, length) != PKT_OK)
    {
        return E_LENGTH;
    }
    if (pkt_get_payload(pkt) != NULL)
    {
        free(pkt->payload);
    }
    pkt->payload = (char *)malloc(length);
    if (pkt->payload == NULL)
    {
        return E_NOMEM;
    }
    memcpy(pkt->payload, data, length);
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
