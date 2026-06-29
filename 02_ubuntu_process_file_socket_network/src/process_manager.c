#include "process_manager.h"

#include "common.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CHILDREN 32

static pid_t children[MAX_CHILDREN];
static size_t child_count = 0;
static volatile sig_atomic_t process_stop_requested = 0;
static volatile sig_atomic_t managed_child_pid = -1;

static int parse_int_range(const char *text, int min, int max, int *value)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < min ||
        parsed > max) {
        return -1;
    }

    *value = (int)parsed;
    return 0;
}

static int is_pid_name(const char *name)
{
    for (size_t i = 0; name[i] != '\0'; ++i) {
        if (!isdigit((unsigned char)name[i])) {
            return 0;
        }
    }
    return name[0] != '\0';
}

static void print_status_line(FILE *fp, const char *key, int indent)
{
    char line[256];
    size_t key_len = strlen(key);

    rewind(fp);

    while(fgets(line, sizeof(line), fp) != NULL){
        if(strncmp(line, key, key_len) == 0){
            line[strcspn(line, "\n")] = '\0';
            printf("%s%s\n", indent ? "\t" : "", line);
            return;
        }
    }
}

int process_list(void)
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    int printed = 0;

    if(dir == NULL){
        perror("opendir /proc");
        return 1;
    }

    printf("%-10s %-32s %s\n" , "PID", "NAME" , "STATUS");
    printf("------------------------------------------------------------\n");

    while((entry = readdir(dir)) != NULL){

        char path[256];
        char name[128] = "-";
        FILE *fp;

        if(!is_pid_name(entry->d_name)){
            continue;
        }

        int pid = atoi(entry->d_name);
        snprintf(path, sizeof(path), "/proc/%d/comm", pid);
        fp = fopen(path, "r");
        if(fp != NULL){
            
            if(fgets(name, sizeof(name), fp)){
                name[strcspn(name, "\n")] = '\0';
            }
            fclose(fp);
        }

        printf("%-10s %-32s /proc/%s/status\n" , entry->d_name, name, entry->d_name );

        printed++;

    }
    closedir(dir);
    return 0;
}

int process_info(const char *pid_text)
{
    int pid;
    char path[256];
    FILE *fp;

    if (parse_int_range(pid_text, 1, 4194304, &pid) < 0) {
        fprintf(stderr, "PID khong hop le.\n");
        return 1;
    }

    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("fopen status");
        return 1;
    }

    print_status_line(fp, "Name:", 0);
    print_status_line(fp, "State:", 0);
    print_status_line(fp, "Pid:", 0);
    print_status_line(fp, "PPid:", 0);
    print_status_line(fp, "VmRSS:", 0);
    fclose(fp);
    return 0;
}

static void handle_process_stop(int signo)
{
    (void)signo;
    process_stop_requested = 1;
    if (managed_child_pid > 0) {
        kill((pid_t)managed_child_pid, SIGTERM);
    }
}

static void install_process_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_process_stop;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

int process_start(const char *command)
{
    pid_t pid;
    int status = 0;

    if (command == NULL || command[0] == '\0') {
        printf("Lenh rong.\n");
        return 1;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        perror("execl");
        _exit(127);
    }

    if (child_count < MAX_CHILDREN) {
        children[child_count++] = pid;
    }

    printf("Da tao tien trinh con PID=%d\n", pid);
    fflush(stdout);

    process_stop_requested = 0;
    managed_child_pid = pid;
    install_process_signal_handlers();

    while (!process_stop_requested) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == 0) {
            sleep(1);
            continue;
        }
        if (result == -1) {
            perror("waitpid");
            managed_child_pid = -1;
            return 1;
        }
        break;
    }

    if (process_stop_requested) {
        waitpid(pid, &status, 0);
    }

    managed_child_pid = -1;
    if (WIFEXITED(status)) {
        printf("PID %d ket thuc, exit=%d.\n", pid, WEXITSTATUS(status));
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        printf("PID %d bi dung boi signal=%d.\n", pid, WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }
    return 0;
}

int process_kill(const char *pid_text, const char *signal_text)
{
    int pid;
    int sig = SIGTERM;

    if (parse_int_range(pid_text, 1, 4194304, &pid) < 0) {
        fprintf(stderr, "PID khong hop le.\n");
        return 1;
    }

    if (signal_text != NULL && parse_int_range(signal_text, 1, 64, &sig) < 0) {
        fprintf(stderr, "Signal khong hop le.\n");
        return 1;
    }

    if (kill((pid_t)pid, sig) == -1) {
        perror("kill");
        return 1;
    }

    printf("Da gui signal %d toi PID %d\n", sig, pid);
    return 0;
}

static void wait_children(void)
{
    size_t kept = 0;

    if (child_count == 0) {
        printf("Khong co tien trinh con nao trong danh sach quan ly.\n");
        return;
    }

    for (size_t i = 0; i < child_count; ++i) {
        int status = 0;
        pid_t result = waitpid(children[i], &status, WNOHANG);

        if (result == 0) {
            printf("PID %d van dang chay.\n", children[i]);
            children[kept++] = children[i];
        } else if (result > 0) {
            if (WIFEXITED(status)) {
                printf("PID %d ket thuc, exit=%d.\n", result,
                       WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("PID %d bi dung boi signal=%d.\n", result,
                       WTERMSIG(status));
            }
        } else {
            perror("waitpid");
        }
    }

    child_count = kept;
}

void process_menu(void)
{
    for (;;) {
        int choice;

        printf("\n=== Quan ly tien trinh ===\n");
        printf("1. Liet ke tien trinh tu /proc\n");
        printf("2. Xem chi tiet tien trinh\n");
        printf("3. Tao tien trinh bang fork/exec\n");
        printf("4. Gui signal toi tien trinh\n");
        printf("5. Kiem tra tien trinh con da tao\n");
        printf("0. Quay lai\n");

        choice = read_int("Chon: ");
        switch (choice) {
        case 1:
            process_list();
            pause_enter();
            break;
        case 2:
        {
            char pid_text[32];
            read_line("Nhap PID: ", pid_text, sizeof(pid_text));
            process_info(pid_text);
            pause_enter();
            break;
        }
        case 3:
        {
            char command[256];
            read_line("Nhap lenh can chay (VD: sleep 30): ", command, sizeof(command));
            process_start(command);
            pause_enter();
            break;
        }
        case 4:
        {
            char pid_text[32];
            char signal_text[32];
            read_line("Nhap PID: ", pid_text, sizeof(pid_text));
            read_line("Nhap signal (15=TERM, 9=KILL, 2=INT): ", signal_text,
                      sizeof(signal_text));
            process_kill(pid_text, signal_text[0] == '\0' ? NULL : signal_text);
            pause_enter();
            break;
        }
        case 5:
            wait_children();
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
