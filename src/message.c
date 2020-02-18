#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "message.h"

/// A struct to hold a list of words to compose a KATCP message with.
struct message {
    /// ? (request), ! (reply) or # (inform), but for the purposes we'll probably just be using ?.
    char message_type; 
    /// An array of strings which will make up the individual words of the KATCP message to be sent.
    char **word_list;
    /// The number of words on the list.
    size_t number_of_words;
};


/**
 * \fn      struct message *message_create(char message_type)
 * \details Allocate memory for a new message, initialise all the members.
 * \param   message_type A character indicating what type of KATCP message you'll be sending.
 *                       In our case this will always be ?, but we generalise here.
 * \return  A pointer to the newly-created message object.
 */
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


/**
 * \fn      void message_destroy(struct message *this_message)
 * \details Free the memory associated with the message object.
 * \param   this_message A pointer to the message to be destroyed.
 * \return  void
 */
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


/**
 * \fn      char message_get_type(struct message *this_message)
 * \details Get the type of KATCP message.
 * \param   this_message A pointer to the message in question.
 * \return  A character indicating what kind of KATCP message it is.
 */
char message_get_type(struct message *this_message)
{
    return this_message->message_type;
}


/**
 * \fn      int message_add_word(struct message *this_message, char *new_word)
 * \details Add a word to the end of the message.
 * \param   this_message A pointer to the message in question.
 * \param   new_word A string containing the word to be added to the message.
 * \return  An integer indicating the outcome of the operation.
 */
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


/**
 * \fn      char *message_see_word(struct message *this_message, size_t this_word)
 * \details Check what the contents of the message is.
 * \param   this_message A pointer to the message in question.
 * \param   this_word The index of the word in the message.
 * \return  A pointer to the word in question.
 */
char *message_see_word(struct message *this_message, size_t this_word)
{
    if (this_message == NULL || this_word >= this_message->number_of_words)
        return NULL;
    return this_message->word_list[this_word];
}


/**
 * \fn      int message_get_number_of_words(struct message *this_message)
 * \details Get the number of words that a message currently has.
 * \param   this_message A pointer to the message in question.
 * \return  An integer with the number of words in the message.
 */
int message_get_number_of_words(struct message *this_message)
{
    if (this_message != NULL)
        return (int) this_message->number_of_words;
    return -1; /// \retval -1 Returns -1 in the case of an error.
}


/**
 * \fn      char *message_compose(struct message *this_message) 
 * \details Get the entire contents of the message as a string. Mostly useful for debugging.
 * \param   this_message A pointer to the message in question.
 * \return  A newly-allocated string containing the contents of the message object.
 */
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
        composed_message = realloc(composed_message, strlen(composed_message) + strlen(this_message->word_list[i]) + 1);
        strcat(composed_message, " ");
        strcat(composed_message, this_message->word_list[i]);
    }
    composed_message[message_length - 2] = '\0'; //to remove the trailiing space

    return composed_message;
}
