#include <stdio.h>
#include <stdarg.h>

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
        switch (msg_verbosity_level) {
            case ERROR:
                printf("ERROR: ");
                break;
            case WARNING:
                printf("WARNING: ");
                break;
            case INFO:
                printf("INFO: ");
                break;
            case DEBUG:
                printf("DEBUG: ");
                break;
            case BORING:
                printf("BORING: ");
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
