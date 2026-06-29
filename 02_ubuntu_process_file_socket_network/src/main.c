#include "common.h"
#include "cli_commands.h"
#include "file_manager.h"
#include "network_info.h"
#include "process_manager.h"
#include "socket_tools.h"

#include <stdio.h>
#include <string.h>

static void print_main_menu(void)
{
    printf("\n========================================\n");
    printf("Ubuntu Process/File/Socket/Network Tool\n");
    printf("========================================\n");
    printf("1. Quan ly tien trinh\n");
    printf("2. Quan ly file\n");
    printf("3. Socket TCP/UDP\n");
    printf("4. Network info/connectivity\n");
    printf("0. Thoat\n");
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--cmd") == 0) {
        return run_command_mode(argc - 2, argv + 2);
    }

    for (;;) {
        int choice;

        print_main_menu();
        choice = read_int("Chon: ");

        switch (choice) {
        case 1:
            process_menu();
            break;
        case 2:
            file_menu();
            break;
        case 3:
            socket_menu();
            break;
        case 4:
            network_menu();
            break;
        case 0:
            printf("Tam biet.\n");
            return 0;
        default:
            printf("Lua chon khong hop le.\n");
            pause_enter();
            break;
        }
    }
}
