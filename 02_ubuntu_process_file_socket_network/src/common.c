#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void read_line(const char *prompt, char *buffer, size_t size)
{
    if (size == 0) {
        return;
    }

    printf("%s", prompt);
    fflush(stdout);

    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';
}

int read_int(const char *prompt)
{
    char buffer[64];
    char *end = NULL;
    long value;

    read_line(prompt, buffer, sizeof(buffer));
    value = strtol(buffer, &end, 10);
    if (end == buffer) {
        return 0;
    }

    return (int)value;
}

void pause_enter(void)
{
    char buffer[8];

    printf("\nNhan Enter de tiep tuc...");
    fflush(stdout);
    fgets(buffer, sizeof(buffer), stdin);
}
