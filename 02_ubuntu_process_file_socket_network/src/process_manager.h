#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

int process_list(void);
int process_info(const char *pid_text);
int process_kill(const char *pid_text, const char *signal_text);
int process_start(const char *command);
void process_menu(void);

#endif
