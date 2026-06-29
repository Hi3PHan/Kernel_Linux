#include "network_info.h"

#include "common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int network_list_interfaces(void)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return 1;
    }

    printf("%-16s %-8s %s\n", "Interface", "Family", "Address");
    printf("------------------------------------------------------------\n");

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        int family;
        int rc;

        if (ifa->ifa_addr == NULL) {
            continue;
        }

        family = ifa->ifa_addr->sa_family;
        if (family != AF_INET && family != AF_INET6) {
            continue;
        }

        rc = getnameinfo(ifa->ifa_addr,
                         family == AF_INET ? sizeof(struct sockaddr_in)
                                           : sizeof(struct sockaddr_in6),
                         host, sizeof(host), NULL, 0, NI_NUMERICHOST);
        if (rc != 0) {
            printf("getnameinfo: %s\n", gai_strerror(rc));
            continue;
        }

        printf("%-16s %-8s %s\n", ifa->ifa_name,
               family == AF_INET ? "IPv4" : "IPv6", host);
    }

    freeifaddrs(ifaddr);
    return 0;
}

int network_resolve_host(const char *host_name)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(host_name, NULL, &hints, &result);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return 1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        char address[NI_MAXHOST];
        rc = getnameinfo(rp->ai_addr, rp->ai_addrlen, address,
                         sizeof(address), NULL, 0, NI_NUMERICHOST);
        if (rc == 0) {
            printf("%s\n", address);
        }
    }

    freeaddrinfo(result);
    return 0;
}

int network_tcp_connect(const char *host_name, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp;
    int rc;
    int connected = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(host_name, port, &hints, &result);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return 1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            continue;
        }

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            printf("Ket noi TCP thanh cong toi %s:%s\n", host_name, port);
            connected = 1;
            close(fd);
            break;
        }

        close(fd);
    }

    if (!connected) {
        printf("Khong ket noi duoc toi %s:%s\n", host_name, port);
        freeaddrinfo(result);
        return 1;
    }

    freeaddrinfo(result);
    return 0;
}

int network_show_routes(void)
{
    FILE *fp = fopen("/proc/net/route", "r");
    char line[256];

    if (fp == NULL) {
        perror("fopen /proc/net/route");
        return 1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        printf("%s", line);
    }

    fclose(fp);
    return 0;
}

void network_menu(void)
{
    for (;;) {
        int choice;

        printf("\n=== Network Ubuntu ===\n");
        printf("1. Liet ke interface va IP bang getifaddrs\n");
        printf("2. Resolve hostname bang getaddrinfo\n");
        printf("3. Test ket noi TCP toi host:port\n");
        printf("4. Xem bang route tu /proc/net/route\n");
        printf("0. Quay lai\n");

        choice = read_int("Chon: ");
        switch (choice) {
        case 1:
            network_list_interfaces();
            pause_enter();
            break;
        case 2:
        {
            char host_name[128];
            read_line("Nhap hostname (VD: ubuntu.com): ", host_name, sizeof(host_name));
            network_resolve_host(host_name);
            pause_enter();
            break;
        }
        case 3:
        {
            char host_name[128];
            char port[16];
            read_line("Nhap host/IP: ", host_name, sizeof(host_name));
            read_line("Nhap port: ", port, sizeof(port));
            network_tcp_connect(host_name, port);
            pause_enter();
            break;
        }
        case 4:
            network_show_routes();
            pause_enter();
            break;
        case 0:
            return;
        default:
            printf("Lua chon khong hop le.\n");
            pause_enter();
            break;
        }
    }
}
