#ifndef SOCKET_TOOLS_H
#define SOCKET_TOOLS_H

int socket_tcp_server(const char *port_text);
int socket_tcp_client(const char *host, const char *port_text, const char *message);
int socket_udp_receiver(const char *port_text);
int socket_udp_send(const char *host, const char *port_text, const char *message);
void socket_menu(void);

#endif
