#include <stdio.h>

#include "include/vn_log.h"

static int vn_log_started;

void vn_log_start(void)
{
    FILE *file;

    file = fopen(VN_LOG_FILE, "w");
    if (file != 0)
    {
        fprintf(file, "VINTANETGS LOG\n");
        fclose(file);
    }
    vn_log_started = 1;
}

void vn_log_line(const char *text)
{
    FILE *file;

    if (!vn_log_started)
        vn_log_start();
    if (text == 0)
        return;

    file = fopen(VN_LOG_FILE, "a");
    if (file == 0)
        return;

    fprintf(file, "%s\n", text);
    fclose(file);
}
