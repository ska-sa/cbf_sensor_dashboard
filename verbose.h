#ifndef _VERBOSE_H_
#define _VERBOSE_H_

void set_verbosity(int setting);
/* Verbosity setting convention
 * 1 - big problems
 * 2 - warnings
 * 3 - misc info
 * TODO should probably put this in an enum.
 */
int verbose_message(int msg_verbosity_level, const char *restrict message_format, ...);

#endif
