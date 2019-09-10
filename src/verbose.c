#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "verbose.h"

static enum verbosity_level verbosity = 0;

void set_verbosity(enum verbosity_level setting)
{
    verbosity = setting;
}


int verbose_message(enum verbosity_level msg_verbosity_level, const char *restrict message_format, ...)
{
    if (msg_verbosity_level <= verbosity)
    {
        va_list args;
        va_start(args, message_format);

        time_t current_time = time(NULL);
        struct tm *tm = localtime(&current_time);
        char format[] = "%F %T";
        char str_time[20];
        strftime(str_time, 20, format, tm);

        switch (msg_verbosity_level) { //TODO there could be a more elegant way of doing this...
            case ERROR:
                printf("ERROR   [%s] ", str_time);
                break;
            case WARNING:
                printf("WARNING [%s] ", str_time);
                break;
            case INFO:
                printf("INFO    [%s] ", str_time);
                break;
            case DEBUG:
                printf("DEBUG   [%s] ", str_time);
                break;
            case BORING:
                printf("BORING  [%s] ", str_time);
                break;
            default:
                ;
        }
        int ret = vprintf(message_format, args);
        va_end(args);
        return ret;
    }
    return 0;
}
