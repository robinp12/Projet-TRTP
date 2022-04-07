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
int eof_reached;
uint8_t lastSeqnum = 0;

typedef struct window_pkt
{
    uint8_t seqnum;
    int windowsize;

    int offset;
    uint64_t pktnum;

    linkedList_t *linkedList;
} window_pkt_t;

int send_ack(const int sfd, uint8_t seqnum)
{
    pkt_t *pkt = pkt_new();
    int retval = pkt_set_type(pkt, PTYPE_NACK);
    if (retval != PKT_OK)
    {
        ERROR("Set type failed : %d", retval);
        return -1;
    }

    pkt_set_seqnum(pkt, seqnum);
    if (retval != PKT_OK)
    {
        ERROR("Seqnum failed : %d", retval);
        return -1;
    }

    size_t len = PKT_MAX_LEN;
    retval = pkt_encode(pkt, buffer, &len);
    if (retval != PKT_OK)
    {
        ERROR("Encode failed : %d", retval);
        return -1;
    }

    DEBUG("Sending packet %d", seqnum);
    if (write(sfd, buffer, len) == -1)
    {
        ERROR("Failed to send packet : %s", strerror(errno));
        return -1;
    }
    pkt_del(pkt);
    return 0;
}

/*
 * Fill the window with packet and sent them
 */
int fill_packet_window(const int sfd, window_pkt_t *window)
{
    size_t bytes_read;
    int n_filled = 0;

    linkedList_t *list = window->linkedList;

    while ((list->size < window->windowsize) && (eof_reached == 0))
    {
        bytes_read = read(sfd, buffer, PKT_MAX_LEN);

        pkt_t *pkt = pkt_new();
        pkt_status_code status = pkt_decode(buffer, bytes_read, pkt);
        // printf("status: %d\n",status);
        if (status != PKT_OK)
        {
            ERROR("error decode");
            return EXIT_FAILURE;
        }
        window->windowsize = pkt_get_window(pkt);
        const char *payload = pkt_get_payload(pkt);
        window->seqnum = pkt_get_seqnum(pkt);
        lastSeqnum = pkt_get_seqnum(pkt);

        ASSERT(payload != NULL);

        // printf("type:%d\n", pkt_get_type(pkt));
        // printf("seqnum:%d\n", pkt_get_seqnum(pkt));
        // printf("window:%d\n", pkt_get_window(pkt));
        // printf("tr:%d\n", pkt_get_tr(pkt));
        // printf("length:%d\n", pkt_get_length(pkt));
        // DEBUG("PAYLOAD CHAR: %s\n", pkt_get_payload(pkt));

        linkedList_add_pkt(list, pkt);

        DEBUG("Receiving packet %ld", window->pktnum);
        window->pktnum++;
        n_filled++;
        if (pkt_get_length(pkt) < 512)
        {
            DEBUG("End of file");
            eof_reached = 1;
        }
    }

    return n_filled;
}

void read_write_receiver(const int sfd)
{
    buffer = (char *)malloc(sizeof(char) * PKT_MAX_LEN);
    if (buffer == NULL)
    {
        ERROR("Error memory");
        return;
    }
    int retval;

    window_pkt_t *window = malloc(sizeof(window_pkt_t));
    window->offset = 0;
    window->pktnum = 0;
    window->seqnum = 0;
    window->windowsize = 1;
    window->linkedList = linkedList_create();

    struct pollfd *fds = malloc(sizeof(*fds));
    int ndfs = 1;

    memset(fds, 0, sizeof(struct pollfd));

    eof_reached = 0;
    fds[0].fd = sfd;
    fds[0].events = POLLIN | POLLOUT;
    int wait = 0;
    while (!eof_reached)
    {

        retval = poll(fds, ndfs, -1);
        if (retval < 0)
        {
            ERROR("error poll");
            return;
        }
        if ((fds[0].revents & POLLIN) && !eof_reached)
        {
            if (window->linkedList->size != window->windowsize)
            {
                retval = fill_packet_window(sfd, window);
                if (retval >= 0)
                {
                    DEBUG("Fill window : %d", retval);
                }
            }
            wait = 0;
        }
        else if (fds[0].revents & POLLOUT)
        {
            if (!wait)
            {
                send_ack(sfd, lastSeqnum);
                DEBUG("Ack sent : %d", lastSeqnum);
                wait = 1;
            }
        }
    }

    DEBUG("Frees");
    free(buffer);
    linkedList_del(window->linkedList);
    free(window);
}