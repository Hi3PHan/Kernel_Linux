#include "socket_tools.h"

#include "common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;
static volatile sig_atomic_t active_socket_fd = -1;

static int parse_port(const char *text, int *port)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < 1 ||
        parsed > 65535) {
        return -1;
    }

    *port = (int)parsed;
    return 0;
}

static void handle_stop_signal(int signo)
{
    (void)signo;
    stop_requested = 1;
    if (active_socket_fd >= 0) {
        close((int)active_socket_fd);
        active_socket_fd = -1;
    }
}

static void install_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_stop_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

static int create_tcp_server_socket(int port)
{
    int fd;
    int opt = 1;
    struct sockaddr_in addr;
    
    fd = socket(AF_INET, SOCK_STREAM, 0);

    if(fd == -1){
        perror("socket");
        return -1;
    }

    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1){
        perror("setsockopt");
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1){
        perror("bind");
        close(fd);
        return -1;
    }

    if(listen(fd, 5) == -1){
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

int socket_tcp_server(const char *port_text)
{
    int port;
    int server_fd;

    if (parse_port(port_text, &port) < 0) {
        fprintf(stderr, "Port khong hop le.\n");
        return 1;
    }

    stop_requested = 0;
    install_signal_handlers();
    server_fd = create_tcp_server_socket(port);
    if (server_fd == -1) {
        return 1;
    }

    printf("Dang listen TCP port %d. Gui SIGTERM/SIGINT de dung.\n", port);
    fflush(stdout);

    while (!stop_requested) {
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);
        char buffer[1024];
        ssize_t n;
        int client_fd;

        active_socket_fd = server_fd;
        client_fd = accept(server_fd, (struct sockaddr *)&peer, &peer_len);
        active_socket_fd = -1;

        if (client_fd == -1) {
            if (stop_requested || errno == EINTR || errno == EBADF) {
                break;
            }
            perror("accept");
            continue;
        }

        printf("Client: %s:%d\n", inet_ntoa(peer.sin_addr),
               ntohs(peer.sin_port));
        fflush(stdout);

        n = read(client_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("Nhan: %s\n", buffer);
            write(client_fd, buffer, (size_t)n);
        } else if (n == -1) {
            perror("read");
        }
        fflush(stdout);
        close(client_fd);
    }

    close(server_fd);
    printf("TCP server da dung.\n");
    return 0;
}

int socket_tcp_client(const char *host, const char *port_text, const char *message)
{
    int port;
    int fd;
    struct sockaddr_in addr;
    char buffer[1024];
    ssize_t n;

    if (parse_port(port_text, &port) < 0) {
        fprintf(stderr, "Port khong hop le.\n");
        return 1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        struct hostent *he = gethostbyname(host);
        if (he == NULL || he->h_addr_list[0] == NULL) {
            fprintf(stderr, "Khong resolve duoc host.\n");
            close(fd);
            return 1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(fd);
        return 1;
    }

    if (write(fd, message, strlen(message)) == -1) {
        perror("write");
        close(fd);
        return 1;
    }

    n = read(fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Server tra ve: %s\n", buffer);
    } else if (n == -1) {
        perror("read");
    }

    close(fd);
    return n == -1 ? 1 : 0;
}

int socket_udp_receiver(const char *port_text)
{
    int port;
    int fd;
    struct sockaddr_in addr;

    if (parse_port(port_text, &port) < 0) {
        fprintf(stderr, "Port khong hop le.\n");
        return 1;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(fd);
        return 1;
    }

    stop_requested = 0;
    install_signal_handlers();
    printf("Dang nhan UDP port %d. Gui SIGTERM/SIGINT de dung.\n", port);
    fflush(stdout);

    while (!stop_requested) {
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);
        char buffer[1024];
        ssize_t n;

        active_socket_fd = fd;
        n = recvfrom(fd, buffer, sizeof(buffer) - 1, 0,
                     (struct sockaddr *)&peer, &peer_len);
        active_socket_fd = -1;

        if (n == -1) {
            if (stop_requested || errno == EINTR || errno == EBADF) {
                break;
            }
            perror("recvfrom");
            continue;
        }

        buffer[n] = '\0';
        printf("Tu %s:%d -> %s\n", inet_ntoa(peer.sin_addr),
               ntohs(peer.sin_port), buffer);
        fflush(stdout);
    }

    close(fd);
    printf("UDP receiver da dung.\n");
    return 0;
}

int socket_udp_send(const char *host, const char *port_text, const char *message)
{
    int port;
    int fd;
    struct sockaddr_in addr;

    if (parse_port(port_text, &port) < 0) {
        fprintf(stderr, "Port khong hop le.\n");
        return 1;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "Vui long nhap IPv4 hop le cho UDP demo.\n");
        close(fd);
        return 1;
    }

    if (sendto(fd, message, strlen(message), 0, (struct sockaddr *)&addr,
               sizeof(addr)) == -1) {
        perror("sendto");
        close(fd);
        return 1;
    }

    close(fd);
    printf("Da gui UDP datagram.\n");
    return 0;
}

static void tcp_echo_server_once(void)
{
    char port[16];

    read_line("Nhap port listen (VD: 9000): ", port, sizeof(port));
    socket_tcp_server(port);
}

static void tcp_client_send(void)
{
    char host[128];
    char port[16];
    char message[512];

    read_line("Nhap host/IP server: ", host, sizeof(host));
    read_line("Nhap port: ", port, sizeof(port));
    read_line("Nhap message: ", message, sizeof(message));

    socket_tcp_client(host, port, message);
}

static void udp_receiver_once(void)
{
    char port[16];

    read_line("Nhap UDP port listen: ", port, sizeof(port));
    socket_udp_receiver(port);
}

static void udp_sender(void)
{
    char host[128];
    char port[16];
    char message[512];

    read_line("Nhap host/IP dich: ", host, sizeof(host));
    read_line("Nhap UDP port dich: ", port, sizeof(port));
    read_line("Nhap message: ", message, sizeof(message));

    socket_udp_send(host, port, message);
}

void socket_menu(void)
{
    for (;;) {
        int choice;

        printf("\n=== Socket TCP/UDP ===\n");
        printf("1. TCP echo server nhan 1 client\n");
        printf("2. TCP client gui message\n");
        printf("3. UDP receiver nhan 1 datagram\n");
        printf("4. UDP sender gui datagram\n");
        printf("0. Quay lai\n");

        choice = read_int("Chon: ");
        switch (choice) {
        case 1:
            tcp_echo_server_once();
            pause_enter();
            break;
        case 2:
            tcp_client_send();
            pause_enter();
            break;
        case 3:
            udp_receiver_once();
            pause_enter();
            break;
        case 4:
            udp_sender();
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
