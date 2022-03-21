#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "log.h"
#include "packet_interface.h"
#include "read-write/create_socket.h"
#include "read-write/wait_for_client.h"
#include "read-write/real_address.h"
#include "read-write/read_write_loop.h"

int print_usage(char *prog_name)
{
    ERROR("Usage:\n\t%s [-s stats_filename] listen_ip listen_port", prog_name);
    return EXIT_FAILURE;
}

pkt_status_code format_pkt(char *pkt_format, uint8_t *seqnum, uint32_t *time, ptypes_t type)
{
    pkt_t *pkt = pkt_new();
    pkt_status_code code;
    size_t len;

    code = pkt_set_type(pkt, type);
    code = pkt_set_tr(pkt, 0);
    code = pkt_set_window(pkt, 1);
    code = pkt_set_length(pkt, 0);
    code = pkt_set_seqnum(pkt, *seqnum);
    code = pkt_set_timestamp(pkt, *time);
    code = pkt_set_timestamp(pkt, *time);
}

int main(int argc, char **argv)
{
    int opt;
    int sock = -1;
    struct sockaddr_in6 listener_addr;
    socklen_t listener_addrlen = sizeof(listener_addr);

    char *stats_filename = NULL;
    char *listen_ip = NULL;
    char *listen_port_err;
    uint16_t listen_port;

    while ((opt = getopt(argc, argv, "s:h")) != -1)
    {
        switch (opt)
        {
        case 'h':
            return print_usage(argv[0]);
        case 's':
            stats_filename = optarg;
            break;
        default:
            return print_usage(argv[0]);
        }
    }

    if (optind + 2 != argc)
    {
        ERROR("Unexpected number of positional arguments");
        return print_usage(argv[0]);
    }

    listen_ip = argv[optind];
    listen_port = (uint16_t)strtol(argv[optind + 1], &listen_port_err, 10);
    if (*listen_port_err != '\0')
    {
        ERROR("Receiver port parameter is not a number");
        return print_usage(argv[0]);
    }

    ASSERT(1 == 1);               // Try to change it to see what happens when it fails
    DEBUG_DUMP("Some bytes", 11); // You can use it with any pointer type

    // This is not an error per-se.
    ERROR("Receiver has following arguments: stats_filename is %s, listen_ip is %s, listen_port is %u",
          stats_filename, listen_ip, listen_port);

    DEBUG("You can only see me if %s", "you built me using `make debug`");
    ERROR("This is not an error, %s", "now let's code!");

    const char *err = real_address(listen_ip, &listener_addr);
    if (err)
    {
        printf("Could not resolve hostname %s : %s\n", listen_ip, err);
        return EXIT_FAILURE;
    }

    sock = create_socket(&listener_addr, listen_port, NULL, -1);

    if (sock > 0 && wait_for_client(sock) < 0)
    {
        printf("Could not connect the socket after the first message \n");
        close(sock);
        return EXIT_FAILURE;
    }

    if (sock < 0)
    {
        printf("Failed to create de socket! : %s \n", strerror(errno));
        return EXIT_FAILURE;
    }

    char buffer[1024];
    ssize_t receiv = recv(sock, buffer, 1024, MSG_PEEK);
    pkt_t *pkt = pkt_new();
    pkt_status_code code = pkt_decode(buffer, receiv, pkt);
    if (code == PKT_OK)
    {
        printf("PKT OK");
    }
    else
    {
        printf("code error : %d\n", code);
    }

    if (pkt_get_tr(pkt) == 1)
    {
    }

    close(sock);
    return EXIT_SUCCESS;
}