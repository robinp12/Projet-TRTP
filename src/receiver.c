#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include "log.h"
#include "packet_implem.h"
#include "read-write/create_socket.h"
#include "read-write/real_address.h"
#include "read-write/wait_for_client.h"
#include "read-write/read_write_receiver.h"

int print_usage(char *prog_name)
{
    ERROR("Usage:\n\t%s [-s stats_filename] listen_ip listen_port", prog_name);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    int opt;
    struct sockaddr_in6 listener_addr;

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

    /* Resolve the hostname */
    char str[INET6_ADDRSTRLEN];
    inet_pton(AF_INET6, listen_ip, &(listener_addr.sin6_addr));
    inet_ntop(AF_INET6, &(listener_addr.sin6_addr), str, INET6_ADDRSTRLEN);
    DEBUG("IPV6 : %s", str);

    const char *err = real_address(listen_ip, &listener_addr);
    if (err)
    {
        fprintf(stderr, "Could not resolve hostname %s: %s\n", listen_ip, err);
        return EXIT_FAILURE;
    }

    /* Get a socket */
    int sfd;
    sfd = create_socket(&listener_addr, listen_port, NULL, -1); /* Connected */

    DEBUG("Waiting for client");
    if (sfd > 0 && wait_for_client(sfd) < 0)
    { /* Connected */
        ERROR("Could not connect the socket after the first message.\n");
        close(sfd);
        return EXIT_FAILURE;
    }

    DEBUG("Socket connected");
    if (sfd < 0)
    {
        fprintf(stderr, "Failed to create the socket!: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    /* Process I/O */
    read_write_receiver(sfd, stats_filename);

    close(sfd);

    return EXIT_SUCCESS;
}