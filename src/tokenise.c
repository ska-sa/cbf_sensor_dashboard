#include <stdlib.h>
#include <string.h>

#include "tokenise.h"
#include "verbose.h"

size_t tokenise_string(char* input_string, char delim, char ***tokens)
{
    char *string = strdup(input_string);
    size_t string_len = strlen(string);
    //If the string ends in a \n we want to remove it.
    if (string[string_len-1] == '\n')
        string[string_len-1] = '\0';

    size_t previous_delim_location = 0;

    //verbose_message(BORING, "Tokenise: string starting out as: %s\n", string);

    size_t num_tokens = 0;

    size_t i;
    for (i = 0; i < string_len; i++)
    {
        if (string[i] == delim)
        {
            string[i] = 0;
            (*tokens) = realloc((*tokens), sizeof(*(*tokens))*(num_tokens + 1));
            (*tokens)[num_tokens] = malloc(strlen(string + previous_delim_location) + 1);
            strcpy((*tokens)[num_tokens], string + previous_delim_location);
            num_tokens++;
            previous_delim_location = i + 1;
            //verbose_message(BORING, "Tokenise: string is now: %s\n", string + previous_delim_location);

        }
    }
    //for the last token on the string.
    (*tokens) = realloc((*tokens), sizeof(*(*tokens))*(num_tokens + 1));
    (*tokens)[num_tokens] = malloc(strlen(string + previous_delim_location) + 1);
    strcpy((*tokens)[num_tokens], string + previous_delim_location);
    num_tokens++;

    free(string);
    string = NULL;

    //verbose_message(BORING, "Tokenise: returning %d.\n", num_tokens);
    return num_tokens;
}
