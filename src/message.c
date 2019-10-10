#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "message.h"

struct message {
    char message_type; /*?, ! or #, but for the purposes we'll probably just be using ?.*/
    char **word_list;
    size_t number_of_words;
};


struct message *message_create(char message_type)
{
    struct message *new_message = malloc(sizeof(*new_message));
    if (new_message != NULL)
    {
        new_message->word_list = NULL;
        new_message->number_of_words = 0;
        new_message->message_type = message_type;
    }
    return new_message;
}


void message_destroy(struct message *this_message)
{
    if (this_message != NULL)
    {
        {
            char *composed_message = message_compose(this_message);
            free(composed_message);
            composed_message = NULL;
        }
        size_t i;
        for (i = 0; i < this_message->number_of_words; i++)
            free(this_message->word_list[i]);
        free(this_message->word_list);
        free(this_message);
        this_message = NULL;
    }
    else
        ;
}


char message_get_type(struct message *this_message)
{
    return this_message->message_type;
}


int message_add_word(struct message *this_message, char *new_word)
{
    if (this_message != NULL && new_word != NULL)
    {
        char **temp = realloc(this_message->word_list, sizeof(*(this_message->word_list))*(this_message->number_of_words + 1));
        if (temp != NULL)
        {
            temp[this_message->number_of_words] = strdup(new_word);
            this_message->number_of_words++;
            this_message->word_list = temp;
            return (int) this_message->number_of_words;
        }
        else
        {
            perror("realloc");
            return -1;
        }
    }
    return -1;
}


char *message_see_word(struct message *this_message, size_t this_word)
{
    if (this_message == NULL || this_word >= this_message->number_of_words)
        return NULL;
    return this_message->word_list[this_word];
}


int message_get_number_of_words(struct message *this_message)
{
    if (this_message != NULL)
        return (int) this_message->number_of_words;
    return -1;
}


char *message_compose(struct message *this_message)
{
    if (this_message == NULL || this_message->number_of_words == 0)
        return NULL;
    size_t message_length = 1;
    size_t i;
    for (i = 0; i < this_message->number_of_words; i++)
    {
        message_length += strlen(this_message->word_list[i]) + 1;
    }
    message_length++; //null character
    char *composed_message = malloc(message_length);
    sprintf(composed_message, "%c%s", this_message->message_type, this_message->word_list[0]);
    for (i = 1; i < this_message->number_of_words; i++)
    {
        sprintf(composed_message, "%s %s", composed_message, this_message->word_list[i]);
    }
    composed_message[message_length - 2] = '\0'; //to remove the trailiing space

    return composed_message;
}
