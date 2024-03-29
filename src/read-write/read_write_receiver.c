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
#include "../fec.h"

char *buffer;
static int eof_reached = 0;
//static int lastSeqnum = 0;
static uint32_t lastTimestamp;
static int resent_ack = 1;
// static uint32_t timestamp;
static int num_ack = 0;
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
    while (list->size > window->windowsize)
    {
        linkedList_remove_end(list);
        ++i;
    }
    if (i > 0)
        window->seqnumTail = pkt_get_seqnum(list->tail->pkt);

    LOG_RECEIVER("Decreased window to %d, dropped %d packets", window->windowsize, i);


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
            return 0;
        }
    }
    if (head == tail && seqnum != head)
    {
        ++packet_ignored;
        LOG_RECEIVER("Packet with seqnum %d ignored (not in window)", seqnum);
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
        const uint16_t newSeqnumA = (seqnumA < head) ? seqnumA + 256 : seqnumA;
        const uint16_t newSeqnumB = (seqnumB < head) ? seqnumB + 256 : seqnumB;
        return newSeqnumA > newSeqnumB;
    }
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
* Deal with the FEC packet
* @returns -1 if useless, the seqnum of the recovered packet if used
*/
int deal_with_fec(window_pkt_t* window, pkt_t* fec_pkt)
{
    linkedList_t* list = window->linkedList;
    const uint8_t seqnum = pkt_get_seqnum(fec_pkt);
    print_window(window);

    if (list->size < 3)
        return -1;

    node_t* current = list->head;
    while (current != NULL)
    {
        if (pkt_get_seqnum(current->pkt) == seqnum || (pkt_get_seqnum(current->pkt) + 1) % 256 == seqnum)
            break; // found packet n

        if (current == list->tail)
            return -1; 
        current = current->next;
    }

    if (current == list->tail)
        return -1;
    
    if (pkt_get_seqnum(current->next->pkt) == seqnum)
        current = current->next;

    LOG_RECEIVER("Start of the fec sequence %d", pkt_get_seqnum(current->pkt));
    const uint8_t nSeqnum = pkt_get_seqnum(current->pkt);

    pkt_t* pkts[3] = {};
    int missingSeqnum = -1;
    int i = 0;
    for (int j = 0; j < 4; j++)
    {
        const uint8_t currentSeqnum = pkt_get_seqnum(current->pkt);
        LOG_RECEIVER("Current %d, nseqnum+i %d", currentSeqnum, (nSeqnum+j)%256);
        if (currentSeqnum != (nSeqnum + j) % 256)
        {
            missingSeqnum = (nSeqnum + j) % 256;
            LOG_RECEIVER("missing %d", missingSeqnum);
            ++j;
        }

        LOG_RECEIVER("i %d seq %d", i, currentSeqnum);
        pkts[i] = current->pkt;
        ++i;

        if (current == list->tail)
        {
            if (i == 3 && missingSeqnum == -1)
            {
                missingSeqnum = (window->seqnumTail + 1) % 256;
                break;
            }
            return -1;
        }
        else
        {
            current = current->next;
        }

    }

    if (i != 3)
    {
        LOG_RECEIVER("i != 2");
        return -1;
    }
    LOG_RECEIVER("FEC found : %d, %d, %d", pkt_get_seqnum(pkts[0]), pkt_get_seqnum(pkts[1]), pkt_get_seqnum(pkts[2]));
    
    pkt_t* pkt_restored = fec_generation(pkts[0], pkts[1], pkts[2], fec_pkt);
    pkt_set_seqnum(pkt_restored, missingSeqnum);
    linkedList_add_increasing(window, pkt_restored);

    LOG_RECEIVER("[%3d] Add packet to the list (FEC)", pkt_get_seqnum(pkt_restored));
    // if (pkt_get_length(pkt_restored) == 0)
    // {
    //     eof_reached = 1;
    //     eof_seqnum = missingSeqnum;
    // }
    if (window->seqnumHead == missingSeqnum)
    {
        resent_ack = 0;
    }
    else
    {
        resent_ack = 1;
    }
    return missingSeqnum;
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
                    increase_window(window);
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
    case PTYPE_FEC:
        
        if (1)
        {
            LOG_RECEIVER("[%3d] Fec ignored (not implemented properly)", pkt_get_seqnum(pkt));
            ++packet_ignored;
            pkt_del(pkt);
            return -1;
        }
        
        if (pkt_get_tr(pkt) == 1)
        {
            LOG_RECEIVER("[%3d] Fec ignored (truncated)", pkt_get_seqnum(pkt));
            ++packet_ignored;
            pkt_del(pkt);
            return -1;
        }
        if (! is_in_window(window, pkt_get_seqnum(pkt)))
        {
            LOG_RECEIVER("[%3d] Fec ignored (not in window)", pkt_get_seqnum(pkt));
            ++packet_ignored;
            pkt_del(pkt);
            return -1;
        }
        ++fec_received;

        int fec_retval = deal_with_fec(window, pkt);
        if(fec_retval != -1)
        {
            ++packet_recovered;
            LOG_RECEIVER("[%3d] Packet restored with FEC", fec_retval);
        }
        else
        {
            LOG_RECEIVER("[%3d] Failed to use FEC", pkt_get_seqnum(pkt));
        }
        pkt_del(pkt);

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
            LOG_RECEIVER("[%3d] All packet sent to stdout", pkt_get_seqnum(current->pkt));
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
        LOG_RECEIVER("[%3d] Send to stdout", pkt_get_seqnum(current->pkt));

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
    
    window->seqnumHead = 0;
    window->seqnumTail = 0;
    window->windowsize = 1;
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
    dprintf(fd_stats, "packet_recovered:%d\n", packet_recovered);



    free(buffer);
    free(fds);
    linkedList_del(window->linkedList);
    free(window);
}