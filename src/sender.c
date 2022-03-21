#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include "log.h"
#include "packet_interface.h"
#include "read-write/create_socket.h"
#include "read-write/wait_for_client.h"
#include "read-write/real_address.h"
#include "read-write/read_write_loop.h"

int print_usage(char *prog_name)
{
    ERROR("Usage:\n\t%s [-f filename] [-s stats_filename] [-c] receiver_ip receiver_port", prog_name);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    int opt;
    int sock = -1;
    struct sockaddr_in6 receiver_addr;

    FILE *fd;

    char *filename = NULL;
    char *stats_filename = NULL;
    char *receiver_ip = NULL;
    char *receiver_port_err;
    bool fec_enabled = false;
    uint16_t receiver_port;

    while ((opt = getopt(argc, argv, "f:s:hc")) != -1)
    {
        switch (opt)
        {
        case 'f':
            filename = optarg;
            break;
        case 'h':
            return print_usage(argv[0]);
        case 's':
            stats_filename = optarg;
            break;
        case 'c':
            fec_enabled = true;
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

    receiver_ip = argv[optind];
    receiver_port = (uint16_t)strtol(argv[optind + 1], &receiver_port_err, 10);
    if (*receiver_port_err != '\0')
    {
        ERROR("Receiver port parameter is not a number");
        return print_usage(argv[0]);
    }

    ASSERT(1 == 1);               // Try to change it to see what happens when it fails
    DEBUG_DUMP("Some bytes", 11); // You can use it with any pointer type

    // This is not an error per-se.
    ERROR("Sender has following arguments: filename is %s, stats_filename is %s, fec_enabled is %d, receiver_ip is %s, receiver_port is %u",
          filename, stats_filename, fec_enabled, receiver_ip, receiver_port);

    DEBUG("You can only see me if %s", "you built me using `make debug`");
    ERROR("This is not an error, %s", "now let's code!");

    const char *err = real_address(receiver_ip, &receiver_addr);
    if (err)
    {
        printf("Could not resolve hostname %s : %s\n", receiver_ip, err);
        return EXIT_FAILURE;
    }

    sock = create_socket(NULL, -1, &receiver_addr, receiver_port);
    if (sock < 0)
    {
        printf("Failed to create de socket! : %s \n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("IP : %s \n", receiver_ip);
    printf("Port : %d \n", ntohs(receiver_addr.sin6_port));

    fd = fopen(filename, "rb");
    if (!fd)
    {
        printf("Unable to open file, error: %s\n", strerror(errno));
        return errno;
    }

    fseek(fd, 0L, SEEK_END);
    ftell(fd);

    ssize_t s = sendto(sock, fd, sizeof(fd), 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
    if (s < 0)
    {
        printf("Unable to open file, error: %s\n", strerror(errno));
        return errno;
    }

    fclose(fd);
    close(sock);
    return EXIT_SUCCESS;
}
