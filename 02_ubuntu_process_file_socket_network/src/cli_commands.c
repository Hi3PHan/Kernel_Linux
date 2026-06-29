#include "cli_commands.h"

#include "file_manager.h"
#include "network_info.h"
#include "process_manager.h"
#include "socket_tools.h"

#include <stdio.h>
#include <string.h>

static void print_usage(void)
{
    printf("Usage:\n");
    printf("  ubuntu_sys_tool --cmd help\n");
    printf("  ubuntu_sys_tool --cmd ps\n");
    printf("  ubuntu_sys_tool --cmd proc-info <pid>\n");
    printf("  ubuntu_sys_tool --cmd start-process <command>\n");
    printf("  ubuntu_sys_tool --cmd kill <pid> [signal]\n");
    printf("  ubuntu_sys_tool --cmd stat <path>\n");
    printf("  ubuntu_sys_tool --cmd read-file <path>\n");
    printf("  ubuntu_sys_tool --cmd write-file <path> <text>\n");
    printf("  ubuntu_sys_tool --cmd copy-file <src> <dst>\n");
    printf("  ubuntu_sys_tool --cmd chmod <path> <mode>\n");
    printf("  ubuntu_sys_tool --cmd delete-file <path> --yes\n");
    printf("  ubuntu_sys_tool --cmd interfaces\n");
    printf("  ubuntu_sys_tool --cmd resolve <host>\n");
    printf("  ubuntu_sys_tool --cmd tcp-connect <host> <port>\n");
    printf("  ubuntu_sys_tool --cmd routes\n");
    printf("  ubuntu_sys_tool --cmd tcp-server <port>\n");
    printf("  ubuntu_sys_tool --cmd tcp-client <host> <port> <message>\n");
    printf("  ubuntu_sys_tool --cmd udp-receiver <port>\n");
    printf("  ubuntu_sys_tool --cmd udp-send <host> <port> <message>\n");
}

static int expect_args(int argc, int expected)
{
    if (argc != expected) {
        print_usage();
        return -1;
    }
    return 0;
}

int run_command_mode(int argc, char **argv)
{
    const char *cmd;

    if (argc == 0) {
        print_usage();
        return 1;
    }

    cmd = argv[0];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 ||
        strcmp(cmd, "--help") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(cmd, "ps") == 0) {
        return expect_args(argc, 1) == 0 ? process_list() : 1;
    }
    if (strcmp(cmd, "proc-info") == 0) {
        return expect_args(argc, 2) == 0 ? process_info(argv[1]) : 1;
    }
    if (strcmp(cmd, "start-process") == 0) {
        return expect_args(argc, 2) == 0 ? process_start(argv[1]) : 1;
    }
    if (strcmp(cmd, "kill") == 0) {
        if (argc != 2 && argc != 3) {
            print_usage();
            return 1;
        }
        return process_kill(argv[1], argc == 3 ? argv[2] : NULL);
    }
    if (strcmp(cmd, "stat") == 0) {
        return expect_args(argc, 2) == 0 ? file_stat_path(argv[1]) : 1;
    }
    if (strcmp(cmd, "read-file") == 0) {
        return expect_args(argc, 2) == 0 ? file_read_path(argv[1]) : 1;
    }
    if (strcmp(cmd, "write-file") == 0) {
        return expect_args(argc, 3) == 0 ? file_write_path(argv[1], argv[2])
                                         : 1;
    }
    if (strcmp(cmd, "copy-file") == 0) {
        return expect_args(argc, 3) == 0 ? file_copy_path(argv[1], argv[2])
                                         : 1;
    }
    if (strcmp(cmd, "chmod") == 0) {
        return expect_args(argc, 3) == 0 ? file_chmod_path(argv[1], argv[2])
                                         : 1;
    }
    if (strcmp(cmd, "delete-file") == 0) {
        if (argc != 3 || strcmp(argv[2], "--yes") != 0) {
            fprintf(stderr, "Tu choi xoa file vi thieu --yes.\n");
            print_usage();
            return 1;
        }
        return file_delete_path(argv[1]);
    }
    if (strcmp(cmd, "interfaces") == 0) {
        return expect_args(argc, 1) == 0 ? network_list_interfaces() : 1;
    }
    if (strcmp(cmd, "resolve") == 0) {
        return expect_args(argc, 2) == 0 ? network_resolve_host(argv[1]) : 1;
    }
    if (strcmp(cmd, "tcp-connect") == 0) {
        return expect_args(argc, 3) == 0 ? network_tcp_connect(argv[1], argv[2])
                                         : 1;
    }
    if (strcmp(cmd, "routes") == 0) {
        return expect_args(argc, 1) == 0 ? network_show_routes() : 1;
    }
    if (strcmp(cmd, "tcp-server") == 0) {
        return expect_args(argc, 2) == 0 ? socket_tcp_server(argv[1]) : 1;
    }
    if (strcmp(cmd, "tcp-client") == 0) {
        return expect_args(argc, 4) == 0
                   ? socket_tcp_client(argv[1], argv[2], argv[3])
                   : 1;
    }
    if (strcmp(cmd, "udp-receiver") == 0) {
        return expect_args(argc, 2) == 0 ? socket_udp_receiver(argv[1]) : 1;
    }
    if (strcmp(cmd, "udp-send") == 0) {
        return expect_args(argc, 4) == 0
                   ? socket_udp_send(argv[1], argv[2], argv[3])
                   : 1;
    }

    fprintf(stderr, "Lenh command mode khong hop le: %s\n", cmd);
    print_usage();
    return 1;
}
