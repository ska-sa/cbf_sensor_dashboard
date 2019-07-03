#ifndef _VERBOSE_H_
#define _VERBOSE_H_

enum verbosity_level {
    NONE,
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    BORING,
};

void set_verbosity(enum verbosity_level setting);
int verbose_message(enum verbosity_level msg_verbosity_level, const char *restrict message_format, ...);

#endif
