#ifndef NETWORK_INFO_H
#define NETWORK_INFO_H

int network_list_interfaces(void);
int network_resolve_host(const char *host_name);
int network_tcp_connect(const char *host_name, const char *port);
int network_show_routes(void);
void network_menu(void);

#endif
