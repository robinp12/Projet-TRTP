#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "read_write_receiver.h"
#include "../packet_implem.h"
#include "../linkedList/linkedList.h"
#include "../log.h"

char *buffer;
int eof_reached_receiver;
uint8_t lastSeqnum = 0;
uint32_t timestamp;
int num_ack = 0;

/* Variables de stats */
static int data_sent = 0;
static int data_received = 0;
static int data_truncated_received = 0;
static int fec_sent = 0;
static int fec_received = 0;
static int ack_sent = 0;
static int ack_received = 0;
static int nack_sent = 0;
static int nack_received = 0;
static int packet_ignored = 0;

int packet_duplicated = 0;
int packet_recovered = 0;

typedef struct window_pkt
{
    uint8_t seqnum;
    int windowsize;

    uint64_t pktnum;

    linkedList_t *linkedList;
} window_pkt_t;

/* Augmenter la taille de la fenetre d'envoi */
void increase_window(window_pkt_t *window)
{
    if (window->windowsize < 31)
    {
        window->windowsize++;
    }
}

/* Diminuer la taille de la fenetre d'envoi */
void decrease_window(window_pkt_t *window)
{
    if (window->windowsize > 2)
    {
        window->windowsize /= 2;
    }
}

/* Formater un paquet pour l'envoi */
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

/* Envoyer un paquet de type NACK */
int send_troncated_nack(const int sfd, pkt_t *pkt, window_pkt_t *window)
{
    data_received++;
    data_truncated_received++;
    packet_ignored++;
    decrease_window(window);

    int ack_status = send_response(sfd, PTYPE_NACK, (pkt_get_seqnum(pkt)) % 256, window, pkt_get_timestamp(pkt));
    if (ack_status != PKT_OK)
    {
        ERROR("Sending nack failed : %d", ack_status);
        return EXIT_FAILURE;
    }
    DEBUG("send_nack : %d", (pkt_get_seqnum(pkt)) % 256);

    nack_sent++;
    return EXIT_SUCCESS;
}

/* Envoyer un paquet de type ACK */
int send_ack(const int sfd, pkt_t *pkt, window_pkt_t *window)
{
    if (pkt_get_seqnum(pkt) <= lastSeqnum)
    {
        increase_window(window);
    }

    int ack_status = send_response(sfd, PTYPE_ACK, (lastSeqnum) % 256, window, pkt_get_timestamp(pkt));
    if (ack_status != PKT_OK)
    {
        ERROR("Sending ack failed : %d", ack_status);
        return EXIT_FAILURE;
    }
    DEBUG("send_ack : %d", lastSeqnum % 256);

    ack_sent++;
    num_ack = 0;
    return EXIT_SUCCESS;
}

/* Ecrire les données recues dans le systeme de fichier */
int write_payload(pkt_t *pkt)
{
    int wr = write(STDOUT_FILENO, pkt_get_payload(pkt), pkt_get_length(pkt));
    if (wr == -1)
    {
        return EXIT_FAILURE;
    }

    num_ack++;
    lastSeqnum = pkt_get_seqnum(pkt) + 1;

    return EXIT_SUCCESS;
}
/*
 * Recois les paquets et les traite en fonction des datas
 */
int receive_pkt(const int sfd, window_pkt_t *window)
{
    size_t bytes_read;
    int decode_status;

    linkedList_t *list = window->linkedList;

    bytes_read = read(sfd, buffer, PKT_MAX_LEN);
    pkt_t *pkt = pkt_new();
    decode_status = pkt_decode(buffer, bytes_read, pkt);
    DEBUG("attendu %d VS %d recu", lastSeqnum, pkt_get_seqnum(pkt));
    DEBUG("window size : %d", window->windowsize);

    if ((pkt_get_type(pkt) != PTYPE_DATA) && (pkt_get_tr(pkt) != 0))
    { /* Paquet ignoré */
        packet_ignored++;
        data_truncated_received++;
    }

    if (pkt_get_type(pkt) == PTYPE_DATA && decode_status != E_CRC)
    {
        data_received++;

        if (pkt_get_length(pkt) <= MAX_PAYLOAD_SIZE)
        {
            if (pkt_get_tr(pkt) == 1)
            { /* Paquet tronqué */
                send_troncated_nack(sfd, pkt, window);
            }

            if (pkt_get_tr(pkt) == 0)
            { /* Paquet non tronqué (OK) */
                while (list->size > 0 && lastSeqnum == pkt_get_seqnum(list->head->pkt))
                { /* Ecrit sur le stdout si les pkt correspondant sont dans la linkedlist */
                    write_payload(list->head->pkt);
                    linkedList_remove(list);
                }
                if (pkt_get_seqnum(pkt) > lastSeqnum)
                { /* Push dans la linkedlist si le seqnum plus grand */
                    if (list->size == 0 ||
                        (list->size > 0 &&
                         (pkt_get_seqnum(pkt) > pkt_get_seqnum(list->tail->pkt))))
                    {
                        linkedList_add_pkt(list, pkt);
                    }

                    /* Diminue la window si pas en sequence */
                    decrease_window(window);
                }
                else if (lastSeqnum == pkt_get_seqnum(pkt))
                { /* Prend le paquet recu */
                    write_payload(pkt);
                }

                if (num_ack >= window->windowsize ||  /* Ack apres la reception de la window */
                    pkt_get_seqnum(pkt) == 0 ||       /* Envoyer premier ack sinon sender envoi pas le suivant */
                    pkt_get_length(pkt) == 0 ||       /* Ack du dernier paquet */
                    lastSeqnum < pkt_get_seqnum(pkt)) /* Ack si nouveau seqnum recu plus grand */
                {                                     /* Ack avec le prochain seqnum attendu */
                    send_ack(sfd, pkt, window);
                }
                else if (lastSeqnum > pkt_get_seqnum(pkt))
                { /* Nack si nouveau seqnum recu plus petit */
                    send_troncated_nack(sfd, pkt, window);
                }

                if (pkt_get_length(pkt) == 0 && lastSeqnum >= pkt_get_seqnum(pkt))
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
        window->pktnum++;
    }
    return window->pktnum;
}

void read_write_receiver(const int sfd, char *stats_filename)
{
    int f;
    buffer = (char *)malloc(sizeof(char) * PKT_MAX_LEN);
    if (buffer == NULL)
    {
        ERROR("Error memory buffer");
        return;
    }

    window_pkt_t *window = malloc(sizeof(window_pkt_t));
    window->pktnum = 0;
    window->seqnum = 0;
    window->windowsize = 4;
    window->linkedList = linkedList_create();

    struct pollfd *fds = malloc(sizeof(*fds));
    int ndfs = 1;

    memset(fds, 0, sizeof(struct pollfd));

    eof_reached_receiver = 0;
    fds[0].fd = sfd;
    fds[0].events = POLLIN;

    while (!eof_reached_receiver)
    {
        if (poll(fds, ndfs, -1) < 0)
        {
            ERROR("error POLLIN");
            return;
        }

        if ((fds[0].revents & POLLIN))
        {
            DEBUG("--------------------------------------");
            receive_pkt(sfd, window);
        }
    }

    if (stats_filename != NULL)
    {
        f = open(stats_filename, O_WRONLY);
        if (f == -1)
        {
            ERROR("Unable to open stats file : %s", strerror(errno));
            return;
        }
        dprintf(f, "data_sent:%d\n", data_sent);
        dprintf(f, "data_received:%d\n", data_received);
        dprintf(f, "data_truncated_received:%d\n", data_truncated_received);
        dprintf(f, "fec_sent:%d\n", fec_sent);
        dprintf(f, "fec_received:%d\n", fec_received);
        dprintf(f, "ack_sent:%d\n", ack_sent);
        dprintf(f, "ack_received:%d\n", ack_received);
        dprintf(f, "nack_sent:%d\n", nack_sent);
        dprintf(f, "nack_received:%d\n", nack_received);
        dprintf(f, "packet_ignored:%d\n", packet_ignored);
        dprintf(f, "packet_duplicated:%d\n", packet_duplicated);
        dprintf(f, "packet_recovered:%d", packet_recovered);
        close(f);
    }
    else
    {
        f = STDERR_FILENO;
    }

    DEBUG("Frees");
    free(buffer);
    linkedList_del(window->linkedList);
    free(window);
}