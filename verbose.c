#include <stdio.h>
#include <stdarg.h>

#include "verbose.h"

static int verbosity = 0;

void set_verbosity(int setting)
{
    verbosity = setting;
}


int verbose_message(int msg_verbosity_level, const char *restrict message_format, ...)
{
    if (msg_verbosity_level <= verbosity)
    {
        va_list args;
        va_start(args, message_format);
        int ret = vprintf(message_format, args);
        va_end(args);
        return ret;
    }
    return 0;
}
