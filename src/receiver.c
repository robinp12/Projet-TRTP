#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include "log.h"
#include "packet_interface.h"

int print_usage(char *prog_name)
{
    ERROR("Usage:\n\t%s [-s stats_filename] listen_ip listen_port", prog_name);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    struct sockaddr_in6 listener_addr;
    int opt;
    int sock = -1;

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

    /* Socket */
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        printf("Could not create the IPv6 SOCK_DGRAM socket, error: %s\n", strerror(errno));
        return errno;
    }

    /* Espace memoire IPV6 */
    memset(&listener_addr, 0, sizeof(listener_addr));
    listener_addr.sin6_family = AF_INET6;
    listener_addr.sin6_port = htons(listen_port);
    memcpy(&listener_addr.sin6_addr, &in6addr_any, sizeof(in6addr_any));

    // Connexion au socket
    if (bind(sock, (struct sockaddr *)&listener_addr, sizeof(listener_addr)) < 0)
    {
        printf("ERROR during connection, error: %s\n", strerror(errno));
        return errno;
    }

    // Now let's code!
    return EXIT_SUCCESS;
}