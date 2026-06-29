#include "file_manager.h"

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int file_stat_path(const char *path)
{
    struct stat st;

    if(stat(path, &st) == -1){
        perror("stat");
        return 1;
    }


    printf("Path: %s\n", path);
    printf("Size: %ld bytes\n", (long)st.st_size);
    printf("Mode: %o\n", st.st_mode & 0777);
    printf("UID/GID: %ld/%ld\n", (long)st.st_uid, (long)st.st_gid);
    printf("Inode: %ld\n", (long)st.st_ino);
    return 0;
}

int file_write_path(const char *path, const char *text)
{
    int fd;
    size_t len = strlen(text);

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    if (write(fd, text, len) == -1 || write(fd, "\n", 1) == -1) {
        perror("write");
        close(fd);
        return 1;
    }

    close(fd);
    printf("Da ghi vao file: %s\n", path);
    return 0;
}

int file_read_path(const char *path)
{
    char buffer[4096];
    int fd;
    ssize_t n;

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t m = write(STDOUT_FILENO, buffer + written,
                              (size_t)(n - written));
            if (m == -1) {
                perror("write stdout");
                close(fd);
                return 1;
            }
            written += m;
        }
    }

    if (n == -1) {
        perror("read");
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}

int file_copy_path(const char *src, const char *dst)
{
  char buffer[4096];
  ssize_t tmp;

  int in_fd = open(src, O_RDONLY);
  if(in_fd < 0){
    perror("open src");
    return 1;
  }

  int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if(out_fd < 0){
    perror("open dst");
    close(in_fd);
    return 1;
  }

  while((tmp = read(in_fd, buffer, sizeof(buffer))) > 0 ){
    size_t written = 0;
    while(written < tmp){
        ssize_t m = write(out_fd, buffer + written, (size_t)(tmp - written));
        if(m ==  -1){
            perror("write");
            close(in_fd);
            close(out_fd);
            return 1;
        }
        written += (size_t)m;
    }
  }
  if (tmp == -1) {
    perror("read");
    close(in_fd);
    close(out_fd);
    return 1;
  }
  close(in_fd);
  close(out_fd);
  printf("Da copy \"%s\" -> \"%s\"\n", src, dst);
  return 0;
}

int file_chmod_path(const char *path, const char *mode_text)
{
    char *end = NULL;
    long mode;

    errno = 0;
    mode = strtol(mode_text, &end, 8);
    if (errno != 0 || end == mode_text || *end != '\0' || mode < 0 ||
        mode > 0777) {
        fprintf(stderr, "Mode khong hop le.\n");
        return 1;
    }

    if (chmod(path, (mode_t)mode) == -1) {
        perror("chmod");
        return 1;
    }

    printf("Da doi quyen %s thanh %03lo\n", path, (unsigned long)mode);
    return 0;
}

int file_delete_path(const char *path)
{
    if (unlink(path) == -1) {
        perror("unlink");
        return 1;
    }

    printf("Da xoa file: %s\n", path);
    return 0;
}

void file_menu(void)
{
    for (;;) {
        int choice;

        printf("\n=== Quan ly file ===\n");
        printf("1. Xem thong tin file bang stat\n");
        printf("2. Ghi/tao file bang open/write\n");
        printf("3. Doc file bang open/read\n");
        printf("4. Copy file bang read/write\n");
        printf("5. Doi quyen file bang chmod\n");
        printf("6. Xoa file bang unlink\n");
        printf("0. Quay lai\n");

        choice = read_int("Chon: ");
        switch (choice) {
        case 1:
        {
            char path[256];
            read_line("Nhap duong dan file: ", path, sizeof(path));
            file_stat_path(path);
            pause_enter();
            break;
        }
        case 2:
        {
            char path[256];
            char text[512];
            read_line("Nhap file can ghi: ", path, sizeof(path));
            read_line("Nhap noi dung: ", text, sizeof(text));
            file_write_path(path, text);
            pause_enter();
            break;
        }
        case 3:
        {
            char path[256];
            read_line("Nhap file can doc: ", path, sizeof(path));
            file_read_path(path);
            pause_enter();
            break;
        }
        case 4:
        {
            char src[256];
            char dst[256];
            read_line("Nhap file nguon: ", src, sizeof(src));
            read_line("Nhap file dich: ", dst, sizeof(dst));
            file_copy_path(src, dst);
            pause_enter();
            break;
        }
        case 5:
        {
            char path[256];
            char mode_text[32];
            read_line("Nhap duong dan file: ", path, sizeof(path));
            read_line("Nhap mode bat phan (VD: 644, 755): ", mode_text,
                      sizeof(mode_text));
            file_chmod_path(path, mode_text);
            pause_enter();
            break;
        }
        case 6:
        {
            char path[256];
            char answer[16];
            read_line("Nhap file can xoa: ", path, sizeof(path));
            read_line("Xac nhan xoa? [y/N]: ", answer, sizeof(answer));
            if (answer[0] == 'y' || answer[0] == 'Y') {
                file_delete_path(path);
            } else {
                printf("Da huy.\n");
            }
            pause_enter();
            break;
        }
        case 0:
            return;
        default:
            printf("Lua chon khong hop le.\n");
            pause_enter();
            break;
        }
    }
}
