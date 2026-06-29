#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

int file_stat_path(const char *path);
int file_read_path(const char *path);
int file_write_path(const char *path, const char *text);
int file_copy_path(const char *src, const char *dst);
int file_chmod_path(const char *path, const char *mode_text);
int file_delete_path(const char *path);
void file_menu(void);

#endif
