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
#include "read_write_sender.h"
#include "../packet_implem.h"
#include "../linkedList/linkedList.h"
#include "../log.h"

char *buffer;
static int eof_reached = 0;
int lastSeqnum = 0;
uint32_t lastTimestamp;
int resent_ack = 1;
uint32_t timestamp;
int num_ack = 0;
static int eof_seqnum = 0;

/* Variables de stats */
static int data_sent = 0;             // number of PTYPE_DATA packet sent
static int data_received = 0;         // number of valid PTYPE_DATA packets received
static int data_truncated_received;   // number of valied PTYPE_DATA packets with TR field to 1 received
static int fec_sent = 0;              // number of PTYPE_FEC packets sent
static int fec_received = 0;          // number of valid PTYPE_FEC packets received
static int ack_sent = 0;              // number of PTYPE_ACK packets sent
static int ack_received = 0;          // number of valid PTYPE_ACK packets received
static int nack_sent = 0;             // number of PTYPE_NACK packets sent
static int nack_received = 0;         // number of valid PTYPE_NACK packets received 
static int packet_ignored = 0;        // number of ignored packets

int packet_duplicated = 0;
int packet_recovered = 0;


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
    if (window->windowsize > 4)
    {
        window->windowsize /= 2;
    }
    else
    {
        return;
    }

    linkedList_t* list = window->linkedList;
    int i = 0;
    print_window(window);
    while (list->size > window->windowsize)
    {
        linkedList_remove_end(list);
        ++i;
    }
    print_window(window);
    LOG_RECEIVER("Decreased window to %d, dropped %d packets", window->windowsize, i);

    if (i > 0)
        window->seqnumTail = pkt_get_seqnum(list->tail->pkt);

    eof_reached = 0; // Because we might have removed the last packet (so the eof is no longer in the window)
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
    data_truncated_received++;
    packet_ignored++;
    decrease_window(window);

    int ack_status = send_response(sfd, PTYPE_NACK, (pkt_get_seqnum(pkt)) % 256, window, pkt_get_timestamp(pkt));
    if (ack_status != PKT_OK)
    {
        ERROR("Sending nack failed : %d", ack_status);
        return EXIT_FAILURE;
    }
    DEBUG("[%3d] send_nack", (pkt_get_seqnum(pkt)) % 256);

    nack_sent++;
    return EXIT_SUCCESS;
}

/* Envoyer un paquet de type ACK */

int send_ack(const int sfd, uint32_t timestamp, window_pkt_t *window)
{
    increase_window(window);

    int ack_status = send_response(sfd, PTYPE_ACK, window->seqnumHead, window, timestamp);

    if (ack_status != PKT_OK)
    {
        ERROR("Sending ack failed : %d", ack_status);
        return EXIT_FAILURE;
    }

    LOG_RECEIVER("[%3d] Send ack", window->seqnumHead);

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
* Check if the packet is in the window
* @returns 1 if yes, 0 if no
*/
int is_in_window(window_pkt_t* window, uint8_t seqnum)
{
    const uint8_t head = window->seqnumHead;
    const uint8_t tail = (window->seqnumHead + window->windowsize) % 256;

    if (head < tail)
    {
        if (seqnum > tail + 1 || seqnum < head)
        {
            if (!(seqnum == 0 && tail == 255))
            {
                ++packet_ignored;
                LOG_RECEIVER("[%3d] Packet ignored (not in window)", seqnum);
                print_window(window);
                return 0;
            }
        }
    }
    else
    {
        if (seqnum < head && seqnum > tail + 1)
        {
            ++packet_ignored;
            LOG_RECEIVER("Packet with seqnum %d ignored (not in window)", seqnum);
            print_window(window);
            return 0;
        }
    }
    if (head == tail && seqnum != head)
    {
        ++packet_ignored;
        LOG_RECEIVER("Packet with seqnum %d ignored (not in window)", seqnum);
        print_window(window);
        return 0;
    }
    
    return 1;
}

/*
* @return 1 if seqnumA > seqnumB, 0 otherwise
*/
int is_seqnum_greater(uint8_t seqnumA, uint8_t seqnumB, window_pkt_t* window)
{
    ASSERT(seqnumA != seqnumB);

    const uint8_t head = window->seqnumHead;
    const uint8_t tail = (window->seqnumHead + window->windowsize) % 256;
    if (head < tail)
    {
        return seqnumA > seqnumB;
    }
    else
    {
        if (head == 255)
        {
            if (seqnumA == 255)
                return 0;
            return seqnumA > seqnumB;
        }
        if (seqnumA >= head && seqnumB >= head)
        {
            return seqnumA > seqnumB;
        }
        if (seqnumA <= head && seqnumB <= head)
        {
            return seqnumA > seqnumB;
        }
        if (seqnumA <= tail)
        {
            return 1;
        }
    }
    return 0;
}

/*
* Add the packet in the linkedList (in increasing order)
* @returns : -1 if duplicated
*/
int linkedList_add_increasing(window_pkt_t* window, pkt_t* pkt)
{
    linkedList_t* list = window->linkedList;
    uint8_t seqnum = pkt_get_seqnum(pkt);

    if (list->size == 0)
    {
        linkedList_add_pkt(list, pkt);
        //window->seqnumHead = seqnum;
        window->seqnumTail = seqnum;
        return 0;
    }
    
    node_t* current = list->head;
    node_t* previous = NULL;
    uint8_t currentSeqnum;

    do
    {   
        currentSeqnum = pkt_get_seqnum(current->pkt);
        if (seqnum == currentSeqnum)
        {
            ++packet_duplicated;
            return -1;
        }
        if (is_seqnum_greater(currentSeqnum, seqnum, window))
        {
            node_t* new_node = malloc(sizeof(node_t));
            new_node->pkt = pkt;

            if (previous == NULL) // add at the beginning
            {
                new_node->next = list->head;
                list->head = new_node;
                list->size++;
                return 0;
            }
            
            // node in the middle
            new_node->next = previous->next;
            previous->next = new_node;
            list->size++;
            return 0;

        }

        previous = current;
        current = current->next;
    } while (previous != list->tail);

    // node at the end
    linkedList_add_pkt(list, pkt);
    window->seqnumTail = seqnum;
    return 0;
    
}

/*
* Deal with the incoming data
*/
int receive_data(const int sfd, window_pkt_t* window)
{
    ssize_t bytes_read = read(sfd, buffer, PKT_MAX_LEN);
    pkt_t* pkt = pkt_new();
    pkt_status_code retval = pkt_decode(buffer, bytes_read, pkt);
    
    if (retval == E_CRC)
    {
        if (pkt_get_type(pkt) == PTYPE_DATA && pkt_get_tr(pkt) == 1 && is_in_window(window, pkt_get_seqnum(pkt)))
        {
            send_troncated_nack(sfd, pkt, window);
            LOG_RECEIVER("[%3d] Send nack", pkt_get_seqnum(pkt));
            pkt_del(pkt);
            return 0;
        }
        LOG_RECEIVER("Packet ignored due to bad CRC");
        packet_ignored++;
        pkt_del(pkt);
        return -1;
    }
    
    if (retval != PKT_OK)
    {
        LOG_RECEIVER("Packet ignored (pkt error number %d)", retval);
        packet_ignored++;
        pkt_del(pkt);
        return -1;
    }
    


    switch (pkt_get_type(pkt))
    {
    case PTYPE_DATA:
        if (pkt_get_tr(pkt) != 0)
        {
            send_troncated_nack(sfd, pkt, window);
            LOG_RECEIVER("[%3d] Send nack", pkt_get_seqnum(pkt));
            pkt_del(pkt);
            return 0;
        }
        uint8_t seqnum = pkt_get_seqnum(pkt);
        if (is_in_window(window, seqnum))
        {
            ++data_received;
            LOG_RECEIVER("[%3d] Received data", seqnum);

            if (linkedList_add_increasing(window, pkt) == 0)
            {
                LOG_RECEIVER("[%3d] Add packet to the list", seqnum);
                if (pkt_get_length(pkt) == 0)
                {
                    eof_reached = 1;
                    eof_seqnum = seqnum;
                }
                if (window->seqnumHead == seqnum)
                {
                    resent_ack = 0;
                    return 0;
                }
                else
                {
                    resent_ack = 1;
                    return 0;

                }
            }
            else
            {
                LOG_RECEIVER("[%3d] Packet duplicated", seqnum);
                resent_ack = 1;
                pkt_del(pkt);
                return -1;
            }
        }
        else // not in window
        {
            pkt_del(pkt);
            resent_ack = 1;
        }
        break;
    
    default:
        LOG_RECEIVER("Packet with unknow type (%d) ignored", pkt_get_type(pkt));
        ++packet_ignored;
        pkt_del(pkt);
        break;
    }

    return 0;

}

/*
* Write the received file on stdout
* @args : list, the linked list ordered
* @return : the number of packet write to stdout (and -1 if EOF is write)
*/
int flush_file(window_pkt_t* window)
{
    if (window->linkedList->size == 0)
    {
        resent_ack = 1;
        return 0;
    }

    linkedList_t* list = window->linkedList;
    int packet_write = 0;

    node_t* current = list->head;

    while (window->seqnumHead == pkt_get_seqnum(current->pkt))
    {
        if (pkt_get_length(current->pkt) == 0)
        {
            LOG_RECEIVER("All packet wrote to the file");
            lastTimestamp = pkt_get_timestamp(current->pkt);
            window->seqnumHead = (window->seqnumHead + 1) % 256;
            return -1;
        }
        int wr = write(STDOUT_FILENO, pkt_get_payload(current->pkt), pkt_get_length(current->pkt));
        if (wr == -1)
        {
            return -1;
        }
        ++packet_write;
        LOG_RECEIVER("Wrote packet %d to stdout", pkt_get_seqnum(current->pkt));

        window->seqnumHead = (window->seqnumHead + 1) % 256;
        if (list->size == 1)
            window->seqnumTail = window->seqnumHead;

        lastTimestamp = pkt_get_timestamp(current->pkt);
        linkedList_remove(list);

        if (current == list->tail)
        {
            if (list->size == 0)
                resent_ack = 1;
            return packet_write;
        }
        current = list->head;
    }
    if (list->size == 0)
        resent_ack = 1;
    return packet_write;
}

void read_write_receiver(const int sfd, const int fd_stats)
{
    buffer = (char *)malloc(sizeof(char) * PKT_MAX_LEN);
    if (buffer == NULL)
    {
        ERROR("Error memory buffer");
        return;
    }

    window_pkt_t *window = malloc(sizeof(window_pkt_t));
    
    window->seqnumNext = 0;
    window->seqnumHead = 0;
    window->seqnumTail = 0;
    window->windowsize = 4;
    window->linkedList = linkedList_create();

    if (window->linkedList == NULL)
    {
        ERROR("Failed to create linked list for the window");
        return;
    }

    struct pollfd *fds = malloc(sizeof(*fds));
    int ndfs = 1;

    memset(fds, 0, sizeof(struct pollfd));

    fds[0].fd = sfd;
    fds[0].events = POLLIN | POLLOUT;
    int eof_wrote = 0;
    int retval;

    while (!eof_wrote)
    {
        if (poll(fds, ndfs, -1) < 0)
        {
            ERROR("error POLLIN");
            return;
        }

        if ((fds[0].revents & POLLIN))
        {
            do  /* Read as much incoming packet as he can */
            {   
                if (!eof_reached || (window->seqnumHead != eof_seqnum))
                {
                    receive_data(sfd, window);
                }

                poll(fds, ndfs, -1);
            
                retval = flush_file(window);
                
                if (retval == -1)
                {
                    eof_wrote = 1;
                    send_ack(sfd, lastTimestamp, window);
                    break;
                }
            } while (fds[0].revents & POLLIN);
        }
       
        else if (fds[0].revents & POLLOUT)
        {
            if (resent_ack)
            {
                send_ack(sfd, lastTimestamp, window);
                resent_ack = 0;
            }
        }
    }

    LOG_RECEIVER("End of transmission");

    dprintf(fd_stats, "data_sent:%d\n", data_sent);
    dprintf(fd_stats, "data_received:%d\n", data_received);
    dprintf(fd_stats, "data_truncated_received:%d\n", data_truncated_received);
    dprintf(fd_stats, "fec_sent:%d\n", fec_sent);
    dprintf(fd_stats, "fec_received:%d\n", fec_received);
    dprintf(fd_stats, "ack_sent:%d\n", ack_sent);
    dprintf(fd_stats, "ack_received:%d\n", ack_received);
    dprintf(fd_stats, "nack_sent:%d\n", nack_sent);
    dprintf(fd_stats, "nack_received:%d\n", nack_received);
    dprintf(fd_stats, "packet_ignored:%d\n", packet_ignored);
    dprintf(fd_stats, "packet_duplicated:%d\n", packet_duplicated);
    dprintf(fd_stats, "packet_recovered:%d", packet_recovered);



    free(buffer);
    free(fds);
    linkedList_del(window->linkedList);
    free(window);
}