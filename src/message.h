#ifndef _MESSAGE_H_
#define _MESSAGE_H_


/**
 * \file  message.h
 * \brief The message type stores a list of words to be used for composing a KATCP message to be sent to a CMC server.
 *        It just helps keep track of what has been sent and what we're waiting for a reponse from.
 */


struct message;

struct message *message_create(char message_type);
void message_destroy(struct message *this_message);

char message_get_type(struct message *this_message);
int message_add_word(struct message *this_message, char *new_word);
char *message_see_word(struct message *this_message, size_t this_word);
int message_get_number_of_words(struct message *this_message);
char *message_compose(struct message *this_message);

#endif
