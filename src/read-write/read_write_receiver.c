#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

#include "read_write_loop.h"
#include "../packet_implem.h"
#include "../linkedList/linkedList.h"
#include "../log.h"

#define TIMEOUT 2000000

char *buffer;
int eof_reached_receiver;
uint8_t lastSeqnum = 0;
uint32_t timestamp;

/* Variables de stats */
int data_sent = 0;
int data_received = 0;
int data_truncated_received = 0;
int fec_sent = 0;
int fec_received = 0;
int ack_sent = 0;
int ack_received = 0;
int nack_sent = 0;
int nack_received = 0;
int packet_ignored = 0;

int packet_duplicated = 0;
int packet_recovered = 0;

typedef struct window_pkt
{
    uint8_t seqnum;
    int windowsize;

    int offset;
    uint64_t pktnum;

    linkedList_t *linkedList;
} window_pkt_t;

int send_response(const int sfd, int type, uint8_t seqnum, window_pkt_t *window, const uint32_t timestamp)
{
    pkt_t *pkt = pkt_new();
    if (pkt_set_type(pkt, type) != PKT_OK)
    {
        ERROR("Set type failed : %d", pkt_set_type(pkt, type));
        return EXIT_FAILURE;
    }

    if (pkt_set_window(pkt, window->windowsize) != PKT_OK)
    {
        ERROR("Set windows failed : %d", pkt_set_window(pkt, window->windowsize));
        return EXIT_FAILURE;
    }

    if (pkt_set_seqnum(pkt, seqnum) != PKT_OK)
    {
        ERROR("Seqnum failed : %d", pkt_set_seqnum(pkt, seqnum));
        return EXIT_FAILURE;
    }

    if (pkt_set_timestamp(pkt, timestamp) != PKT_OK)
    {
        ERROR("Seqnum failed : %d", pkt_set_timestamp(pkt, timestamp));
        return EXIT_FAILURE;
    }

    size_t len = PKT_MAX_LEN;
    if (pkt_encode(pkt, buffer, &len) != PKT_OK)
    {
        ERROR("Encode failed : %d", pkt_encode(pkt, buffer, &len));
        return EXIT_FAILURE;
    }

    if (write(sfd, buffer, len) == -1)
    {
        ERROR("Failed to send packet : %s", strerror(errno));
        return EXIT_FAILURE;
    }

    pkt_del(pkt);
    return PKT_OK;
}

/*
 * Fill the window with packet and sent them
 */
int fill_packet_window(const int sfd, window_pkt_t *window)
{
    size_t bytes_read;
    int n_filled = 0;
    int retval;

    linkedList_t *list = window->linkedList;

    while (eof_reached_receiver == 0)
    {
        bytes_read = read(sfd, buffer, PKT_MAX_LEN);

        pkt_t *pkt = pkt_new();
        pkt_decode(buffer, bytes_read, pkt);
        DEBUG("Receiving packet %ld", window->pktnum);

        if (pkt_get_type(pkt) == PTYPE_DATA)
        {
            // window->windowsize = pkt_get_window(pkt);
            window->windowsize = 4; /* Temporaire */
            // printf("linkedlist size : %d\n",window->linkedList->size);

            if (window->linkedList->size <= window->windowsize)
            {
                linkedList_add_pkt(list, pkt);
            }
            if (window->linkedList->size > window->windowsize)
            {
                linkedList_remove(list);
            }

            if (pkt_get_length(pkt) <= MAX_PAYLOAD_SIZE)
            {
                data_received++;

                if (pkt_get_tr(pkt) == 1)
                { /* Paquet tronqué */
                    lastSeqnum = pkt_get_seqnum(pkt);
                    timestamp = pkt_get_timestamp(pkt);

                    retval = send_response(sfd, PTYPE_NACK, (lastSeqnum) % 255, window, pkt_get_timestamp(pkt));
                    if (retval != PKT_OK)
                    {
                        ERROR("Sending nack failed : %d", retval);
                        return EXIT_FAILURE;
                    }
                    DEBUG("send_nack : %d", lastSeqnum);
                    nack_sent++;
                    data_truncated_received++;
                }

                if (pkt_get_tr(pkt) == 0)
                { /* Paquet non tronqué (OK) */
                    lastSeqnum = pkt_get_seqnum(pkt) + 1;
                    timestamp = pkt_get_timestamp(pkt);
                    int wr = write(STDOUT_FILENO, pkt_get_payload(pkt), pkt_get_length(pkt));
                    if (wr == -1)
                    {
                        return EXIT_FAILURE;
                    }
                    retval = send_response(sfd, PTYPE_ACK, (lastSeqnum) % 255, window, timestamp);
                    if (retval != PKT_OK)
                    {
                        ERROR("Sending ack failed : %d", retval);
                        return EXIT_FAILURE;
                    }
                    DEBUG("send_ack : %d", lastSeqnum);

                    ack_sent++;
                    if (pkt_get_length(pkt) == 0)
                    { /* Reception du dernier paquet */
                        DEBUG("EOF");
                        eof_reached_receiver = 1;
                        return EXIT_SUCCESS;
                    }
                }
            }
            else
            {
                packet_ignored++;
            }
        }
        if ((pkt_get_type(pkt) != PTYPE_DATA && pkt_get_tr(pkt) != 0))
        { /* Paquet ignoré */
            packet_ignored++;
            data_truncated_received++;
        }

        window->pktnum++;
        n_filled++;
    }

    return n_filled;
}

void read_write_receiver(const int sfd, char *stats_filename)
{
    int retval;

    buffer = (char *)malloc(sizeof(char) * PKT_MAX_LEN);
    if (buffer == NULL)
    {
        ERROR("Error memory buffer");
        return;
    }

    window_pkt_t *window = malloc(sizeof(window_pkt_t));
    window->offset = 0;
    window->pktnum = 0;
    window->seqnum = 0;
    window->windowsize = 1;
    window->linkedList = linkedList_create();

    struct pollfd *fds = malloc(sizeof(*fds));
    int ndfs = 1;

    memset(fds, 0, sizeof(struct pollfd));

    eof_reached_receiver = 0;
    fds[0].fd = sfd;
    fds[0].events = POLLIN | POLLOUT;

    while (!eof_reached_receiver)
    {
        retval = poll(fds, ndfs, -1);
        if (retval < 0)
        {
            ERROR("error POLLIN");
            return;
        }

        if ((fds[0].revents & POLLIN) && !eof_reached_receiver)
        {
            if (window->linkedList->size != window->windowsize)
            {
                retval = fill_packet_window(sfd, window);
            }

            // Temporaire : vider la liste sans vérifier si les paquets sont en séquence (pour pouvoir faire avancer le sender, sinon le receiver bloque avec sa fenetre de reception pleine)
            /* while (window->linkedList->size >= 1)
            {
                linkedList_remove(window->linkedList);
            } */
        }
    }

    if (stats_filename != NULL)
    {
        FILE *f = fopen(stats_filename, "w");
        fprintf(f, "data_sent:%d\n", data_sent);
        fprintf(f, "data_received:%d\n", data_received);
        fprintf(f, "data_truncated_received:%d\n", data_truncated_received);
        fprintf(f, "fec_sent:%d\n", fec_sent);
        fprintf(f, "fec_received:%d\n", fec_received);
        fprintf(f, "ack_sent:%d\n", ack_sent);
        fprintf(f, "ack_received:%d\n", ack_received);
        fprintf(f, "nack_sent:%d\n", nack_sent);
        fprintf(f, "nack_received:%d\n", nack_received);
        fprintf(f, "packet_ignored:%d\n", packet_ignored);
        fprintf(f, "packet_duplicated:%d\n", packet_duplicated);
        fprintf(f, "packet_recovered:%d", packet_recovered);
        fclose(f);
    }

    DEBUG("Frees");
    free(buffer);
    linkedList_del(window->linkedList);
    free(window);
}